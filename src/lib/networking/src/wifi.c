// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide the functions related to WiFi networking.
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *
 * @file   wifi.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This file's header */
#include "wifi.h"

/* C-standard for string operations */
#include <string.h>

/* Other headers of the component */
#include "networking/networking.h"

/* The FreeRTOS headers are required for timers */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ``ESP_ERROR_CHECK``
 * - defines the return values ``ESP_OK`` (0) and ``ESP_FAIL`` (-1)
 */
#include "esp_err.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ESP-IDF's network abstraction layer */
#include "esp_netif.h"

/* ESP-IDF's wifi library */
#include "esp_wifi.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


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


/**
 * Custom type to store the WiFi-related configuration.
 *
 * This type is used to store the WiFi-configuration of a WiFi network, to
 * which this component should establish a connection. It is not used to store
 * the configuration of the internal access point.
 *
 * See ::project_wifi_config
 */
typedef struct {
    char ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char psk[NETWORKING_WIFI_PSK_MAX_LEN];
} networking_wifi_config_t;


/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking.wifi";

/**
 * The configuration to be used while connecting to a WiFi network.
 *
 * The variable is initialized with ``0x00`` in ::wifi_initialize and then
 * populated by ::get_wifi_config_from_nvs.
 */
static networking_wifi_config_t* project_wifi_config = NULL;

/**
 * Reference to the ``netif`` object for the access point.
 */
static esp_netif_t* ap_netif = NULL;

/**
 * Number of connected clients, if in access point mode.
 *
 * This number will be modified by ::wifi_event_handler, depending on
 * connecting / disconnecting clients.
 *
 * It is reset to ``0`` everytime the access point is started.
 */
static uint8_t ap_num_clients = 0;

/**
 * Reference to the freeRTOS' ``timer`` object that is used to shutdown the
 * access point.
 */
static TimerHandle_t ap_shutdown_timer = NULL;

/**
 * Reference to the ``netif`` object while in ``station`` mode.
 */
static esp_netif_t* sta_netif = NULL;

/**
 * Track the number of connection attempts.
 *
 * This number will be modieifd by ::wifi_event_handler, depending on
 * successful / failed connection attempts.
 *
 * See ::NETWORKING_WIFI_STA_MAX_CONNECTION_ATTEMPTS for the maximum of failed
 * connection attempts.
 */
static uint8_t sta_num_reconnects = 0;


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t ap_launch(void);
static void ap_shutdown(TimerHandle_t xTimer);
static esp_err_t connect_to_wifi(void);
static void get_wifi_config_from_nvs(char* nvs_namespace);
static void sta_shutdown(void);
static void wifi_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);


/* ***** FUNCTIONS ********************************************************* */

/**
 * Initializes and starts a local access point.
 *
 * This function is responsible for the whole setup process of the access
 * point, including its initial configuration, setting up the required network
 * interface (``netif``) ::ap_netif and preparing the required timer to
 * shutdown the access point after a given time.
 *
 * If the configured password ::NETWORKING_WIFI_AP_PSK is shorter than ``8``
 * characters, ``esp_wifi``'s internal functions will fail badly. Because of
 * this, this function will not apply the password but instead switch the
 * access point's ``authmode`` to ``WIFI_AUTH_OPEN``, effectivly providing an
 * open WiFi. A log message of level ``WARN`` will be emitted.
 *
 * The function will provide a log message of level ``INFO`` containing the
 * SSID and - if set - password/PSK of the access point's network.
 *
 * @return ``ESP_OK`` (= ``0``) on success, non-zero error code on error
 */
static esp_err_t ap_launch(void) {
    ESP_LOGV(TAG, "Entering ap_launch()");

    // Create a network interface for the access point.
    ap_netif = esp_netif_create_default_wifi_ap();

    ap_shutdown_timer = xTimerCreate(
        NULL,
        pdMS_TO_TICKS(NETWORKING_WIFI_AP_LIFETIME),
        pdFALSE,
        (void *) 0,
        ap_shutdown);

    // Setup the configuration for access point mode.
    // These values are based off project-specific settings, that may be
    // changed by ``menuconfig`` / ``sdkconfig`` during building the
    // application. They can not be changed through the webinterface.
    wifi_config_t ap_config = {
        .ap = {
            .ssid = NETWORKING_WIFI_AP_SSID,
            .ssid_len = strlen(NETWORKING_WIFI_AP_SSID),
            .channel = NETWORKING_WIFI_AP_CHANNEL,
            .password = NETWORKING_WIFI_AP_PSK,
            .max_connection = NETWORKING_WIFI_AP_MAX_CONNS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    // esp_wifi requires PSKs to be at least 8 characters. Just switch to
    // an **open** WiFi, if the provided PSK has less than 8 characters.
    // TODO(mischback): Verify how that minimal password length is set. Is this
    //                  an ``#define`` that may be picked up in our code or is
    //                  it really hardcoded in the esp_wifi library/component?
    if (strlen(NETWORKING_WIFI_AP_PSK) <= 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
        ESP_LOGW(
            TAG,
            "The provided PSK for the access point has less than 8 characters, "
            "switching to an open WiFi. No password will be required to "
            "connect to the access point.");
    }

    // Actually start the access point
    // a) stop anything WiFi-related (ignoring failures) to make sure, that
    //    there is a well-defined starting point.
    // b) set the mode to "access point"
    // c) apply the access point-specific configuration
    // d) actuall start the access point
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // TODO(mischback): Should the password really be disclosed?!
    ESP_LOGI(
        TAG,
        "Launched Access Point! SSID: '%s', PSK: '%s'",
        ap_config.ap.ssid,
        ap_config.ap.password);

    return ESP_OK;
}

/**
 * Shut down the access point and clean up.
 *
 * This function is a callback to freeRTOS' ``timer`` implementation. The
 * actual ``TimerHandle_t`` is ::ap_shutdown_timer,
 * which is created/initialized in ::ap_launch and
 * started in ::wifi_event_handler.
 *
 * @param xTimer Reference to the freeRTOS' ``timer`` that triggered this
 *               callback.
 */
static void ap_shutdown(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "Access Point is shutting down...");

    // Stop the access point and remove its netif
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_netif_destroy_default_wifi(ap_netif);
    ap_netif = NULL;

    // Stop and remove the timer
    xTimerStop(xTimer, (TickType_t) 0);
    xTimerDelete(xTimer, (TickType_t) 0);
    ap_shutdown_timer = NULL;

    ESP_LOGI(TAG, "Access Point is shut down!");
}

/**
 * Connect to a (stored) WiFi network.
 *
 * Basically this function does the setup of the station mode and starts the
 * interface.
 *
 * The actual magic of establishing and maintaining the connection and
 * switching to the local access point (if required) is done in
 * ::wifi_event_handler.
 *
 * @return ``ESP_OK`` (= ``0``) on success, non-zero error code on error
 */
static esp_err_t connect_to_wifi(void) {
    ESP_LOGV(TAG, "Entering connect_to_wifi()");

    // Check the ``ssid`` of the project-specific WiFi configuration.
    // If this is an empty string, most likely there was no configuration
    // stored in the NVS, so the local access point can be started immediatly.
    if (strlen(project_wifi_config->ssid) == 0) {
        return ap_launch();
    }

    // Create a network interface for the station mode.
    sta_netif = esp_netif_create_default_wifi_sta();

    // Setup the configuration for station mode.
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
            .threshold.rssi = NETWORKING_WIFI_STA_THRESHOLD_RSSI,
            .threshold.authmode = NETWORKING_WIFI_STA_THRESHOLD_AUTH,
        },
    };
    // Inject the SSID / PSK as fetched from the NVS.
    // ``memcpy`` feels like *force*, but it works.
    memcpy(
        sta_config.sta.ssid,
        project_wifi_config->ssid,
        strlen(project_wifi_config->ssid));
    memcpy(
        sta_config.sta.password,
        project_wifi_config->psk,
        strlen(project_wifi_config->psk));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

/**
 * Retrieve WiFi configuration from the NVS.
 *
 * The function opens the non-volatile storage namespace (as specified by
 * ``nvs_namespace``) and retrieves the WiFi's ``SSID`` from
 * ::NETWORKING_WIFI_NVS_KEY_SSID and the *pre-shared key* ``PSK`` from
 * ::NETWORKING_WIFI_NVS_KEY_PSK.
 *
 * The retrieved values are stored in the modules ::project_wifi_config.
 *
 * This function does not return anything. If the namespace could not be opened
 * or one of the specified keys could not be retrieved, the function emits a
 * log message of level ``ERROR``.
 *
 * Please note: During the applications initial startup, these *errors* are to
 * be expected, as the WiFi is not yet configured.
 *
 * @param nvs_namespace Namespace to be opened and used to retrieve the values.
 */
static void get_wifi_config_from_nvs(char* nvs_namespace) {
    ESP_LOGV(TAG, "Entering get_wifi_config_from_nvs()");

    // Open NVS storage of the given namespace
    nvs_handle_t storage_handle;
    esp_err_t err = nvs_open(
        nvs_namespace,
        NVS_READONLY,
        &storage_handle);

    if (err != ESP_OK) {
        // This might fail for different reasons, e.g. the NVS is not correctly
        // set up or initialized.
        // Assuming that the NVS **is** available, this will fail with
        // ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
        // the name ``nvs_namespace`` (yet).
        // This might happen during first start of the applications, as there
        // is no WiFi config yet, so the namespace was never used before.
        ESP_LOGE(TAG, "Could not open NVS handle (%s)", esp_err_to_name(err));
        return;
    }

    // Got a handle to the NVS, now read the required values!
    ESP_LOGD(
        TAG,
        "NVS handle to '%s' successfully opened.",
        nvs_namespace);

    // Read the SSID and password from NVS.
    size_t required_size;
    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_NVS_KEY_SSID,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_NVS_KEY_SSID,
        project_wifi_config->ssid,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_SSID,
            esp_err_to_name(err));

        // Close the handle to the NVS and return.
        nvs_close(storage_handle);
        return;
    }

    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_NVS_KEY_PSK,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_NVS_KEY_PSK,
        project_wifi_config->psk,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_PSK,
            esp_err_to_name(err));

        // Close the handle to the NVS and return.
        nvs_close(storage_handle);
        return;
    }

    // Close the handle to the NVS.
    nvs_close(storage_handle);

    ESP_LOGD(TAG, "Successfully read WiFi config from NVS");

    return;
}

/**
 * Shut down the station mode and clean up.
 */
static void sta_shutdown(void) {
    ESP_LOGV(TAG, "Entering sta_shutdown()");

    // Stop the access point and remove its netif.
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_netif_destroy_default_wifi(sta_netif);
    sta_netif = NULL;

    ESP_LOGI(TAG, "Shut down of station mode WiFi completed!");
}

/**
 * Handle all WiFi-related events.
 *
 * This event handler is registered in ::wifi_initialize. The handler is
 * attached to the ``default`` event loop, as provided by ``esp_event`` (the
 * loop has to be started outside of this component, most likely in the
 * application's ``app_main()``).
 *
 * The function handles the access point and station mode. The related events
 * are distinct, so there should not be any interferences.
 *
 * @param arg        Generic arguments.
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   ``esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data.
 */
static void wifi_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "Entering wifi_event_handler()");

    if (event_base != WIFI_EVENT) {
        return;
    }

    switch (event_id) {
        // Please note, that some ``case`` statements have an empty statement
        // (``{}``) appended to enable the declaration of variables with the
        // correct scope. See https://stackoverflow.com/a/18496437
        // However, ``cpplint`` does not like the recommended ``;`` and
        // recommends an empty block ``{}``.
        case WIFI_EVENT_AP_START:
            // This event is emitted when the access point is started.
            // The timer to shutdown the access point is started.
            ESP_LOGV(TAG, "[wifi_event_handler] WIFI_EVENT_AP_START");

            // Reset the number of connected clients.
            ap_num_clients = 0;

            // The access point got started: start a timer to shut it down
            // (eventually).
            xTimerStart(ap_shutdown_timer, (TickType_t) 0);
            break;
        case WIFI_EVENT_AP_STACONNECTED: {}
            // This event is emitted when a station connects to the local
            // access point.
            // The shutdown timer is stopped.
            ESP_LOGV(TAG, "[wifi_event_handler] WIFI_EVENT_AP_STACONNECTED");

            wifi_event_ap_staconnected_t* connect =
                (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[connect] "MACSTR" (%d)",
                MAC2STR(connect->mac),
                connect->aid);

            // Increment the number of connected clients.
            ap_num_clients++;

            // A client connected to the access point, stop the shutdown timer!
            if (xTimerIsTimerActive(ap_shutdown_timer) == pdTRUE) {
                xTimerStop(ap_shutdown_timer, (TickType_t) 0);
                ESP_LOGV(TAG, "ap_shutdown_timer stopped!");
            }
            break;
        case WIFI_EVENT_AP_STADISCONNECTED: {}
            // This event is emitted when a station disconnects from the local
            // access point.
            // If this was the last connected client, restart the shutdown
            // timer to kill the access point.
            ESP_LOGV(TAG, "[wifi_event_handler] WIFI_EVENT_AP_STADISCONNECTED");

            wifi_event_ap_stadisconnected_t* disconnect =
                (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[disconnect] "MACSTR" (%d)",
                MAC2STR(disconnect->mac),
                disconnect->aid);

            // Decrement the number of connected clients.
            ap_num_clients--;

            // Check if there are any clients left and re-start the shutdown
            // timer, if there are no clients!
            if (ap_num_clients == 0) {
                if (xTimerIsTimerActive(ap_shutdown_timer) == pdFALSE) {
                    xTimerStart(ap_shutdown_timer, (TickType_t) 0);
                    ESP_LOGV(TAG, "ap_shutdown_timer restarted!");
                } else {
                    // Probably dead code... Better safe than sorry.
                    ESP_LOGD(
                        TAG,
                        "No more clients, but ap_shutdown_timer running...");
                }
            }
            break;
        case WIFI_EVENT_STA_START:
            // This event is emitted after the WiFi is started in station mode.
            // Simply call ``esp_wifi_connect()`` to actually establish the
            // OSI layer 2 connection.
            ESP_LOGD(TAG, "[wifi_event_handler] WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            // This event just means, that a connection was established on
            // OSI layer 2. Generally, layer 3 (IP) is required for any real
            // networking task.
            ESP_LOGD(TAG, "[wifi_event_handler] STA_CONNECTED");

            // Reset the tracker for failed connection attempts.
            // TODO(mischback) Does this make sense here? Or should this be
            //                 done later, i.e. for the IP_EVENT_STA_GOT_IP?
            sta_num_reconnects = 0;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            // This event is generated for very different reasons:
            // a) The application triggers a shutdown of the connection, by
            //    calling ``esp_wifi_disconnect()``, ``esp_wifi_stop()`` or
            //    ``esp_wifi_restart()``.
            // b) ``esp_wifi_connect()`` failed to establish a connection, i.e.
            //    because the (stored) network is not reachable or the password
            //    is rejected.
            // c) The connection got disrupted, either by the WiFi's access
            //    point or other external circumstances (most likely zombies).
            //
            // Case a) is not covered by this component, as its purpose is to
            // establish and maintain network connectivity.
            // Cases b) and c) must be handled here.
            //
            // TODO(mischback) Here's the part for some additional logic,
            //                 implementing a maximum number of retries
            //                 before the internal access point is launched
            ESP_LOGD(TAG, "[wifi_event_handler] STA_DISCONNECTED");

            sta_num_reconnects++;

            if (sta_num_reconnects <=
                NETWORKING_WIFI_STA_MAX_CONNECTION_ATTEMPTS) {
                esp_wifi_connect();
            } else {
                sta_shutdown();
                ap_launch();
            }
            break;
        default:
            // Any other WIFI_EVENT is just logged here.
            // TODO(mischback) This should be removed as soon as possible!
            ESP_LOGV(TAG, "[wifi_event_handler] some WIFI_EVENT");
            break;
    }
}

esp_err_t wifi_initialize(char* nvs_namespace) {
    // Set log-level of our own code to VERBOSE.
    // TODO(mischback) This is just for development purposes and should be
    //                 removed as soon as possible. The actual logging should
    //                 be controlled by ``menuconfig`` / ``sdkconfig``.
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "Entering wifi_initialize()");

    // This is the entry point of the wifi-related source code. First of all,
    // initialize the module's variables.
    project_wifi_config = malloc(sizeof(networking_wifi_config_t));
    memset(project_wifi_config, 0x00, sizeof(networking_wifi_config_t));

    // FIXME(mischback) This is just for temporary testing!
    //                  And no, these are not my actual credentials!
    // strcpy(project_wifi_config->ssid, "WiFi_SSID");
    // strcpy(project_wifi_config->psk, "WiFi_PSK");

    // Read WiFi credentials from non-volatile storage (NVS).
    // During initialization, the config just has to be read once.
    // At least the SSID field must be a non-empty string, so check this.
    if (strlen(project_wifi_config->ssid) == 0) {
        get_wifi_config_from_nvs(nvs_namespace);
    }

    // Initialize the WiFi.
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    // Register event handler for WiFi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        NULL));

    if (connect_to_wifi() == ESP_OK) {
        return ESP_OK;
    }

    // Could not connect to (stored) WiFi network and something went wrong
    // during launch of the internal access point. Clean up and return FAILURE.
    ESP_ERROR_CHECK(esp_wifi_deinit());
    return ESP_FAIL;
}
