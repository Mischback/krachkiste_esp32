// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide a basic management of the WiFi capabilities of the MCU.
 *
 * Resources:
 *   - https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/softAP/main/softap_example_main.c
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 */

/* The components header files (forward declaration) */
#include "include/networking.h"  // public header
#include "networking_p.h"  // private header

/* C-standard for string operations */
#include <string.h>

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


#define DEFAULT_SCAN_LIST_SIZE 5

/**
 * The project-specific namespace to access the non-volatile storage.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``), consider
 *       ``ESP-IDF``'s ``NVS_KEY_NAME_SIZE``
 *       (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#_CPPv48nvs_openPKc15nvs_open_mode_tP12nvs_handle_t)
 */
#define PROJECT_NVS_STORAGE_NAMESPACE "krachkiste"

/**
 * The channel to use while providing the project-specific access point.
 *
 * @todo Is there a nice way to provide a **dynamic** channel?
 * @todo Make this configurable (pre-build with ``sdkconfig``)!
 */
#define NETWORKING_WIFI_AP_CHANNEL 5

/**
 * The project-specific access point is only started for a pre-determined
 * lifetime.
 *
 * The value is specified in milliseconds, default value of ``120000`` are
 * *120 seconds*.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_LIFETIME 120000

/**
 * The maximum number of allowed clients while providing the project-specific
 * access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_MAX_CONNS 3

/**
 * The password to access the project-specific access point.
 *
 * The component ``esp_wifi`` requires the password to be at least 8
 * characters! It fails badly otherwise. ``networking_wifi_ap_initialize`` does
 * handle this.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_PSK "foobar"

/**
 * The actual SSID of the project-specific access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_SSID "krachkiste_ap"

/**
 * The maximum length of the ``char`` array to store SSID.
 *
 * IEEE 802.11 says, that the maximum length of an SSID is 32, so this is set
 * to 33 (to allow for the terminating ``"\0"``).
 *
 * @todo Should this be made configurable by project settings? 33 is in fact
 *       a value derived from IEEE 802.11, so allowing for manual configuration
 *       can *only* be used to minimize the required memory usage. But the
 *       saving would be minimal, probably not worth the effort.
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 33

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_SSID_NVS_KEY "net_ssid"

/**
 * The maximum length of the ``char`` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE 801.11 says, that the maximum length of an PSK is 64, so this is set
 * to 65 (to allow for the terminating ``"\0"``).
 *
 * @todo Should this be made configurable by project settings? 65 is in fact
 *       a value derived from IEEE 802.11, so allowing for manual configuration
 *       can *only* be used to minimize the required memory usage. But the
 *       saving would be minimal, probably not worth the effort.
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 65

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_PSK_NVS_KEY "net_pass"


/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See :idf_api:`its API documentation <system/log.html#how-to-use-this-library>`.
 */
static const char* TAG = "krachkiste.networking";

/**
 * Reference to the ``netif`` object for the access point.
 */
static esp_netif_t* networking_wifi_ap_netif = NULL;

/**
 * Reference to the ``timer`` object that is used to shutdown the access point.
 */
static TimerHandle_t networking_wifi_ap_shutdown_timer = NULL;


static void wifi_scan_for_networks(void) {
    // ported example code
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // initialize the wifi configuration with known defaults (of ESP-IDF)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t max_number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        // print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            // print_cipher_type(
            //     ap_info[i].pairwise_cipher,
            //     ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
}

/**
 * Shut down the access point and clean up.
 *
 * @param xTimer Reference to the freeRTOS' ``timer`` that triggered this
 *               callback.
 *
 * This function is a callback to freeRTOS' ``timer`` implementation. The
 * actual ``TimerHandle_t`` is ``networking_wifi_ap_shutdown_timer``,
 * which is created/initialized in ``networking_wifi_ap_initialize`` and
 * started in ``networking_wifi_ap_event_handler``.
 */
static void networking_wifi_ap_shutdown_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "Access Point is shutting down...");

    // Unregister the event handler for AP mode
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &networking_wifi_ap_event_handler));

    // Stop the access point and remove its netif
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_netif_destroy_default_wifi(networking_wifi_ap_netif);
    networking_wifi_ap_netif = NULL;

    // Stop and remove the timer
    xTimerStop(xTimer, (TickType_t) 0);
    xTimerDelete(xTimer, (TickType_t) 0);
    networking_wifi_ap_shutdown_timer = NULL;

    ESP_LOGI(TAG, "Access Point is shut down!")
}

static esp_err_t networking_get_wifi_credentials(
    char* wifi_ssid,
    char* wifi_password) {
    ESP_LOGV(TAG, "Entering networking_get_wifi_credentials()");

    // Open NVS storage of the given namespace
    nvs_handle_t storage_handle;
    esp_err_t err = nvs_open(
        PROJECT_NVS_STORAGE_NAMESPACE,
        NVS_READONLY,
        &storage_handle);
    if (err != ESP_OK) {
        // This might fail for different reasons, e.g. the NVS is not correctly
        // set up or initialized.
        // Assuming that the NVS **is** available, this will fail with
        // ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
        // the name PROJECT_NVS_STORAGE_NAMESPACE (yet).
        // This might happen during first start of the applications, as there
        // is no WiFi config yet, so the namespace was never used before.
        ESP_LOGE(TAG, "Could not open NVS handle (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Got a handle to the NVS, no read the required values!
    ESP_LOGD(
        TAG,
        "NVS handle %s successfully opened.",
        PROJECT_NVS_STORAGE_NAMESPACE);

    // Read the SSID and password from NVS
    size_t required_size;
    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_SSID_NVS_KEY,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_SSID_NVS_KEY,
        wifi_ssid,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_SSID_NVS_KEY,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_PSK_NVS_KEY,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_PSK_NVS_KEY,
        wifi_password,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_PSK_NVS_KEY,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Close the handle to the NVS
    nvs_close(storage_handle);

    return ESP_OK;
}

static void networking_wifi_ap_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {

    switch (event_id) {
        // Please note, that all ``case`` statements have an empty statement
        // (``{}``) appended to enable the declaration of variables with the
        // correct scope. See https://stackoverflow.com/a/18496437
        // However, ``cpplint`` does not like the recommended ``;`` and
        // recommends an empty block ``{}``.
        case WIFI_EVENT_AP_START: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_START");

            // The access point got started: create a timer to shut it down
            // (eventually).
            xTimerStart(networking_wifi_ap_shutdown_timer, (TickType_t) 0);
            break;
        case WIFI_EVENT_AP_STACONNECTED: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STACONNECTED");

            wifi_event_ap_staconnected_t* connect =
                (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[connect] "MACSTR" (%d)",
                MAC2STR(connect->mac),
                connect->aid);

            // A client connected to the access point, stop the shutdown timer!
            if (xTimerIsTimerActive(
                networking_wifi_ap_shutdown_timer) == pdTRUE) {
                xTimerStop(networking_wifi_ap_shutdown_timer, (TickType_t) 0);
            }
            break;
        case WIFI_EVENT_AP_STADISCONNECTED: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STADISCONNECTED");

            wifi_event_ap_stadisconnected_t* disconnect =
                (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[disconnect] "MACSTR" (%d)",
                MAC2STR(disconnect->mac),
                disconnect->aid);
            break;
        default:
            break;
    }
}

/**
 * Initializes and starts an access point.
 *
 * @return ``ESP_OK`` (= ``0``) on success, non-zero error code on error
 *
 * This function is responsible for the whole setup process of the access
 * point, including its initial configuration, setting up the required network
 * interface (``netif``), registering the event handler
 * ``networking_wifi_ap_event_handler`` and preparing the required timer
 * to shutdown the access point after a given time.
 *
 * The function will provide a log message of level ``INFO`` containing the
 * SSID and - if set - password/PSK of the access point's network.
 */
static esp_err_t networking_wifi_ap_initialize(void) {
    ESP_LOGV(TAG, "Entering networking_wifi_ap_initialize()");

    networking_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    networking_wifi_ap_shutdown_timer = xTimerCreate(
        NULL,
        pdMS_TO_TICKS(NETWORKING_WIFI_AP_LIFETIME),
        pdFALSE,
        (void *) 0,
        networking_wifi_ap_shutdown_callback);

    wifi_init_config_t ap_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ap_init_cfg));

    // Register event handler for AP mode
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &networking_wifi_ap_event_handler,
        NULL,
        NULL));

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(
        TAG,
        "Launched Access Point! SSID: '%s', PSK: '%s'",
        ap_config.ap.ssid,
        ap_config.ap.password);

    return ESP_OK;
}

void networking_initialize(void) {
    // set log-level of our own code to VERBOSE (sdkconfig.defaults sets the
    // default log-level to INFO)
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ESP_LOGV(TAG, "Entering networking_initialize()");

    // Initialize the underlying TCP/IP stack
    // This should be done exactly once from application code, so this function
    // seems like a good enough place, as it is the starting point for all
    // networking-related code.
    ESP_ERROR_CHECK(esp_netif_init());

    // FIXME(mischback): just for debugging during development
    ESP_LOGD(TAG, "Address of 'ap_netif': %p", networking_wifi_ap_netif);

    // Read WiFi credentials from non-volatile storage (NVS)
    char wifi_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char wifi_password[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(&wifi_ssid, 0, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(&wifi_password, 0, NETWORKING_WIFI_PSK_MAX_LEN);
    esp_err_t err = networking_get_wifi_credentials(wifi_ssid, wifi_password);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Could not read WiFi credentials from NVS");
        ESP_LOGD(TAG, "Trying to start access point now!");

        ESP_ERROR_CHECK(networking_wifi_ap_initialize());
    } else {
        ESP_LOGD(TAG, "SSID: >%s<", wifi_ssid);
        ESP_LOGD(TAG, "Password: >%s<", wifi_password);
    }

    // FIXME(mischback): just for debugging during development
    ESP_LOGD(TAG, "Address of 'ap_netif': %p", networking_wifi_ap_netif);
}
