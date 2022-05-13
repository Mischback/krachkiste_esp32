// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * WiFi-related functions of the ``networking`` component.
 *
 * @file   networking_wifi.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "mnet32_wifi.h"

/* C's standard libraries. */
#include <string.h>

/* Other headers of the component. */
#include "mnet32/mnet32.h"    // The public header
#include "mnet32_internal.h"  // The private header
#include "mnet32_state.h"     // manage the internal state

/* This is ESP-IDF's event library. */
#include "esp_event.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ESP-IDF's network abstraction layer. */
#include "esp_netif.h"

/* ESP-IDF's wifi library. */
#include "esp_wifi.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``timers.h`` for timers
 */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"


/* ***** DEFINES *********************************************************** */

/**
 * The maximum length of the ``char`` array to store SSID.
 *
 * IEEE 802.11 says, that the maximum length of an SSID is 32, which is also
 * the value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 32

/**
 * The maximum length of the ``char`` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE 801.11 says, that the maximum length of an PSK is 64, which is also the
 * value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 64


/* ***** TYPES ************************************************************* */

/**
 * Medium/mode specific state information for access point mode.
 *
 * The access point is kept alive for a given timespan. This is controlled by
 * a ``freeRTOS`` timer. A reference to this timer must be kept while in access
 * point mode.
 */
struct medium_state_wifi_ap {
    TimerHandle_t ap_shutdown_timer;
};

/**
 * Medium/mode specific state information for station mode.
 *
 * In station mode, the number of failed connection attempts has to be tracked.
 */
struct medium_state_wifi_sta {
    int8_t num_connection_attempts;
};


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";


/* ***** PROTOTYPES ******************************************************** */

static esp_err_t wifi_init(char *nvs_namespace);
static esp_err_t wifi_ap_deinit(void);
static void wifi_ap_timed_shutdown(TimerHandle_t timer);
static esp_err_t wifi_get_config_from_nvs(
    char *nvs_namespace,
    char **ssid,
    char **psk);
static esp_err_t wifi_sta_init(char **sta_ssid, char **sta_psk);


/* ***** FUNCTIONS ********************************************************* */


/**
 * WiFi-specific initialization.
 *
 * Perform the required steps to use the controller's WiFi interface. This
 * includes the initialization of the WiFi interface, registering of
 * ::mnet32_event_handler for ``WIFI_EVENT`` occurences and reading an
 * optionally available configuration (for station mode) from the non-volatile
 * storage.
 *
 * The actual, *mode-specific* initialization is done in ::wifi_sta_init and
 * ::mnet32_wifi_ap_init, depending on the availability of credentials for station
 * mode.
 *
 * This function will set ``state->medium`` to ``MNET32_MEDIUM_WIRELESS``
 * (``state->mode`` will be set in the specific initialization function).
 *
 * @param nvs_namespace The NVS namespace to read values from.
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure; This
 *                      function may return ``ESP_FAIL`` by itsself or by the
 *                      specific initialization functions ::wifi_sta_init and
 *                      ::mnet32_wifi_ap_init, see the provided log messages (of
 *                      level ``ERROR`` and ``DEBUG``) for the actual reason
 *                      of failure. **This function does return** ``ESP_OK``
 *                      in case that ``state->mode`` **is not**
 *                      ``NETWORKING_MODE_APPLICABLE``, because it assumes,
 *                      that there has been a previous call to ``wifi_init``.
 */
static esp_err_t wifi_init(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_init()");

    /* Initialization has only be performed once */
    if (mnet32_state_is_mode_set()) {
        ESP_LOGW(TAG, "WiFi seems to be already initialized!");
        return ESP_OK;
    }

    esp_err_t esp_ret;

    /* Initialize the WiFi networking stack. */
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&init_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize WiFi!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_init()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    mnet32_state_set_medium_wireless();

    /* Register WIFI_EVENT event handler.
     * These events are required for any *mode*, so the handler can already
     * be registered at this point.
     */
    esp_ret = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        mnet32_event_handler,
        NULL,
        (void **)mnet32_state_get_medium_event_handler_ptr());
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach WIFI_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    /* Read WiFi configuration from non-volatile storage (NVS).
     * If the config can not be read, directly start in access point mode. If
     * there is a config, try station mode first.
     */
    char nvs_sta_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char nvs_sta_psk[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(nvs_sta_ssid, 0x00, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(nvs_sta_psk, 0x00, NETWORKING_WIFI_PSK_MAX_LEN);

    esp_ret = wifi_get_config_from_nvs(
        nvs_namespace,
        (char **)&nvs_sta_ssid,
        (char **)&nvs_sta_psk);
    if (esp_ret != ESP_OK) {
        ESP_LOGI(TAG, "Could not read credentials, starting access point!");
        return mnet32_wifi_ap_init();
    }

    ESP_LOGD(TAG, "Retrieved SSID.. '%s'", nvs_sta_ssid);
    ESP_LOGD(TAG, "Retrieved PSK... '%s'", nvs_sta_psk);

    esp_ret = wifi_sta_init((char **)&nvs_sta_ssid, (char **)&nvs_sta_psk);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start WiFi station mode!");
        ESP_LOGI(TAG, "Starting access point!");
        mnet32_wifi_sta_deinit();
        return mnet32_wifi_ap_init();
    }

    /* At this point, the WiFi is started in station mode. All further
     * actions will be triggered by ::mnet32_event_handler
     */
    return ESP_OK;
}

esp_err_t mnet32_wifi_deinit(void) {
    ESP_LOGV(TAG, "mnet32_wifi_deinit()");

    /* Unregister the WIFI_EVENT event handler. */
    esp_err_t esp_ret = esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        mnet32_state_get_medium_event_handler());
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister WIFI_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_unregister()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    if (mnet32_state_is_mode_ap())
        wifi_ap_deinit();

    if (mnet32_state_is_mode_sta())
        mnet32_wifi_sta_deinit();

    esp_ret = esp_wifi_deinit();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinitialization of WiFi failed!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_deinit() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
    }
    mnet32_state_clear_medium();

    return ESP_OK;
}

esp_err_t mnet32_wifi_ap_init(void) {
    ESP_LOGV(TAG, "mnet32_wifi_ap_init()");

    /* Create a network interface for access point mode. */
    mnet32_state_set_interface(esp_netif_create_default_wifi_ap());
    if (!mnet32_state_is_interface_set()) {
        ESP_LOGE(TAG, "Could not create network interface for AP!");
        return ESP_FAIL;
    }

    /* Allocate memory for the specific state information. */
    mnet32_state_medium_state_init(sizeof(struct medium_state_wifi_ap));

    /* Create the timer to eventually shut down the access point. */
    ((struct medium_state_wifi_ap *)mnet32_state_get_medium_state())->ap_shutdown_timer =  // NOLINT(whitespace/line_length)
        xTimerCreate(
            NULL,
            pdMS_TO_TICKS(MNET32_WIFI_AP_LIFETIME),
            pdFALSE,
            (void *) 0,
            wifi_ap_timed_shutdown);

    /* Setup the configuration for access point mode.
     * These values are based off project-specific settings, that may be
     * changed by ``menuconfig`` / ``sdkconfig`` during building the
     * application. They can not be changed through the webinterface.
     */
    wifi_config_t ap_config = {
        .ap = {
            .ssid = MNET32_WIFI_AP_SSID,
            .ssid_len = strlen(MNET32_WIFI_AP_SSID),
            .channel = MNET32_WIFI_AP_CHANNEL,
            .password = MNET32_WIFI_AP_PSK,
            .max_connection = MNET32_WIFI_AP_MAX_CONNS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    /* esp_wifi requires PSKs to be at least 8 characters. Just switch to
     * an **open** WiFi, if the provided PSK has less than 8 characters.
     * TODO(mischback): Verify how that minimal password length is set. Is this
     *                  an ``#define`` that may be picked up in our code or is
     *                  it really hardcoded in the esp_wifi library/component?
     */
    if (strlen(MNET32_WIFI_AP_PSK) <= 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
        ESP_LOGW(
            TAG,
            "The provided PSK for the access point has less than 8 characters, "
            "switching to an open WiFi. No password will be required to "
            "connect to the access point.");
    }

    esp_err_t esp_ret;

    /* Apply configuration and start WiFi interface in access point mode. */
    esp_ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi mode to AP!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_mode() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    mnet32_state_set_mode_ap();

    esp_ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi config for AP!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_config()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start wifi in AP mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_start()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

/**
 * Deinitialize WiFi (access point mode).
 *
 * Stops the access point and destroys the ``netif``.
 *
 * This function is meant to be called by ::mnet32_wifi_deinit and performs only the
 * specific deinitialization steps of *access point mode*, e.g. it does **not**
 * unregister ::mnet32_event_handler from ``WIFI_EVENT``.
 *
 * The function sets ``state->mode`` to ``MNET32_MODE_NOT_APPLICABLE`` and
 * ``state->interface`` to ``NULL``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
static esp_err_t wifi_ap_deinit(void) {
    ESP_LOGV(TAG, "wifi_ap_deinit()");

    if (!mnet32_state_is_mode_set()) {
        ESP_LOGE(TAG, "WiFi is not initialized!");
        ESP_LOGD(TAG, "Current WiFi mode is %d", mnet32_state_get_mode());
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_err_t esp_ret = esp_wifi_stop();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not stop WiFi (AP mode)!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_stop()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_netif_destroy_default_wifi(mnet32_state_get_interface());
    mnet32_state_clear_interface();

    if (mnet32_state_is_medium_state_initialized())
        mnet32_state_medium_state_destroy();

    mnet32_state_clear_mode();

    return ESP_OK;
}

static void wifi_ap_timed_shutdown(TimerHandle_t timer) {
    ESP_LOGV(TAG, "wifi_ap_timed_shutdown()");

    if (!mnet32_state_is_status_idle()) {
        ESP_LOGW(TAG, "Access Point is not idle! Skipping shutdown!");
        return;
    }

    xTimerStop(timer, (TickType_t) 0);
    xTimerDelete(timer, (TickType_t) 0);

    mnet32_stop();
}

int8_t mnet32_wifi_ap_get_connected_stations(void) {
    ESP_LOGV(TAG, "mnet32_wifi_ap_get_connected_stations()");

    wifi_sta_list_t tmp;

    if (esp_wifi_ap_get_sta_list(&tmp) != ESP_OK) {
        ESP_LOGW(TAG, "Could not determine number of connected stations!");
        return -1;  // indicate error with a totally unexpected value!
    }
    ESP_LOGD(TAG, "Connected stations: %d", tmp.num);

    return tmp.num;
}

void mnet32_wifi_ap_timer_start(void) {
    ESP_LOGV(TAG, "mnet32_wifi_ap_timer_start()");

    struct medium_state_wifi_ap *tmp = mnet32_state_get_medium_state();

    if (tmp->ap_shutdown_timer == NULL) {
        ESP_LOGW(TAG, "The ap_shutdown_timer is not available!");
        return;  // fail silently
    }

    if (xTimerIsTimerActive(tmp->ap_shutdown_timer) == pdTRUE) {
        ESP_LOGW(TAG, "Access point's shutdown timer is already running!");
        return;  // fail silently
    }

    xTimerStart(tmp->ap_shutdown_timer, (TickType_t) 0);
    ESP_LOGD(TAG, "Access point's shutdown timer started!");
}

void mnet32_wifi_ap_timer_stop(void) {
    ESP_LOGV(TAG, "mnet32_wifi_ap_timer_stop()");

    struct medium_state_wifi_ap *tmp = mnet32_state_get_medium_state();

    if (tmp->ap_shutdown_timer == NULL) {
        ESP_LOGW(TAG, "The ap_shutdown_timer is not available!");
        return;  // fail silently
    }

    if (xTimerIsTimerActive(tmp->ap_shutdown_timer) != pdTRUE) {
        ESP_LOGW(TAG, "Access point's shutdown timer is not running!");
        return;  // fail silently
    }

    xTimerStop(tmp->ap_shutdown_timer, (TickType_t) 0);
    ESP_LOGD(TAG, "Access point's shutdown timer stopped!");
}

/**
 * Retrieve SSID and PSK for station mode from non-volatile storage.
 *
 * @param nvs_namespace The NVS namespace to read values from.
 * @param ssid          A pointer to a *big enough* ``char`` array to store
 *                      the read value to (*big enough* =
 *                      ::NETWORKING_WIFI_SSID_MAX_LEN )
 * @param psk           A pointer to a *big enough* ``char`` array to store
 *                      thr read value to (*big enough* =
 *                      ::NETWORKING_WIFI_PSK_MAX_LEN )
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure; see the
 *                      provided log messages (of level ``ERROR`` and ``DEBUG``)
 *                      for the actual reason of failure.
 *
 * @todo Needs validation/testing. ``ssid`` and ``psk`` should be ``char**``?
 * @todo This should be refactored!
 *       - provide a generic function to open nvs, e.g.
 *         ``static nvs_handle_t get_nvs_handle(char *nvs_namespace)``
 *       - provide a generic function to read a string from nvs, e.g.
 *         ``static char* get_nvs_string(nvs_handle_t handle, char *key)``
 *       - the prototype of this function may be left untouched!
 */
static esp_err_t wifi_get_config_from_nvs(
    char *nvs_namespace,
    char **ssid,
    char **psk) {
    ESP_LOGV(TAG, "wifi_get_config_from_nvs()");

    /* Open NVS storage handle. */
    // FIXME(mischback) Restore implementation once the station mode is working!
    // nvs_handle_t handle;
    // esp_err_t esp_ret;

    // esp_ret = get_nvs_handle(nvs_namespace, NVS_READONLY, &handle);
    // if (esp_ret != ESP_OK)
    //     return esp_ret;
    // ESP_LOGD(TAG, "Handle '%s' successfully opened!", nvs_namespace);

    // esp_ret = get_string_from_nvs(
    //     handle,
    //     NETWORKING_WIFI_NVS_SSID,
    //     (char *)ssid,
    //     NETWORKING_WIFI_SSID_MAX_LEN);
    // if (esp_ret != ESP_OK)
    //     return esp_ret;

    // esp_ret = get_string_from_nvs(
    //     handle,
    //     NETWORKING_WIFI_NVS_PSK,
    //     (char *)psk,
    //     NETWORKING_WIFI_PSK_MAX_LEN);
    // if (esp_ret != ESP_OK)
    //     return esp_ret;

    // FIXME(mischback) This is just for temporary testing!
    //                  And no, these are not my actual credentials!
    // strcpy((char *)ssid, "WiFi_SSID");
    // strcpy((char *)psk, "WiFi_PSK");

    return ESP_FAIL;
}

/**
 * Initialize the WiFi for station mode.
 *
 * Basically this creates the station-specific configuration, applies it to
 * **ESP-IDF**'s wifi module and starts the wifi.
 *
 * It sets ``state->mode`` to ``MNET32_MODE_WIFI_STA`` and provides a
 * reference to the ``netif`` in ``state->interface``.
 *
 * @param sta_ssid   The SSID of the WiFi network to connect to.
 * @param sta_psk    The pre-shared key (PSK) of the WiFi network to connect to.
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 *                   On ``ESP_OK`` the calling code may assume that the access
 *                   point is successfully started. Subsequent actions are
 *                   triggered by ::mnet32_event_handler and performed by
 *                   ::networking .
 */
static esp_err_t wifi_sta_init(char **sta_ssid, char **sta_psk) {
    ESP_LOGV(TAG, "wifi_sta_init()");

    /* Create a network interface for Access Point mode. */
    mnet32_state_set_interface(esp_netif_create_default_wifi_sta());
    if (!mnet32_state_is_interface_set()) {
        ESP_LOGE(TAG, "Could not create network interface for station mode!");
        return ESP_FAIL;
    }

    /* Allocate memory for the specific state information. */
    mnet32_state_medium_state_init(sizeof(struct medium_state_wifi_sta));
    mnet32_wifi_sta_reset_connection_counter();

    /* Setup the configuration for station mode.
     * Some of the settings may be configured by ``menuconfig`` / ``sdkconfig``,
     * the ``SSID`` and ``PSK`` are read from the non-volatile storage (NVS) and
     * passed into this function.
     */
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
            .threshold.rssi = MNET32_WIFI_STA_THRESHOLD_RSSI,
            .threshold.authmode = MNET32_WIFI_STA_THRESHOLD_AUTH,
        },
    };
    // Inject the SSID / PSK as fetched from the NVS.
    // ``memcpy`` feels like *force*, but it works.
    memcpy(
        sta_config.sta.ssid,
        (char *)sta_ssid,
        strlen((char *)sta_ssid));
    memcpy(
        sta_config.sta.password,
        (char *)sta_psk,
        strlen((char *)sta_psk));

    esp_err_t esp_ret;

    /* Apply configuration and start WiFi interface in station mode .*/
    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi mode to STA!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_mode() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    mnet32_state_set_mode_sta();

    esp_ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi config for station mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_config()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start wifi in station mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_start()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

esp_err_t mnet32_wifi_sta_deinit(void) {
    ESP_LOGV(TAG, "mnet32_wifi_sta_deinit()");

    if (!mnet32_state_is_mode_set()) {
        ESP_LOGE(TAG, "WiFi is not initialized!");
        ESP_LOGD(TAG, "Current WiFi mode is %d", mnet32_state_get_mode());
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_err_t esp_ret = esp_wifi_stop();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not stop WiFi (station mode)!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_stop()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_netif_destroy_default_wifi(mnet32_state_get_interface());
    mnet32_state_clear_interface();

    if (mnet32_state_is_medium_state_initialized())
        mnet32_state_medium_state_destroy();

    mnet32_state_clear_mode();

    return ESP_OK;
}

void mnet32_wifi_sta_connect(void) {
    ESP_LOGV(TAG, "mnet32_wifi_sta_connect()");

    ((struct medium_state_wifi_sta *)mnet32_state_get_medium_state())->num_connection_attempts++;  // NOLINT(whitespace/line_length)

    esp_err_t esp_ret = esp_wifi_connect();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Connect command failed!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_connect()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
    }
}

int8_t mnet32_wifi_sta_get_num_connection_attempts(void) {
    return ((struct medium_state_wifi_sta *)mnet32_state_get_medium_state())->num_connection_attempts;  // NOLINT(whitespace/line_length)
}

void mnet32_wifi_sta_reset_connection_counter(void) {
    ((struct medium_state_wifi_sta *)mnet32_state_get_medium_state())->num_connection_attempts = 0;  // NOLINT(whitespace/line_length)
}

esp_err_t mnet32_wifi_start(char *nvs_namespace) {
    ESP_LOGV(TAG, "mnet32_wifi_start()");

    if (wifi_init(nvs_namespace) != ESP_OK) {
        mnet32_wifi_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}
