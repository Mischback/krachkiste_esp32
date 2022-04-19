// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide basic management of the networking capabilities of the MCU.
 *
 * This file is the actual implementation of the component. For a detailed
 * description of the actual usage, including how the component may be reused
 * in some other codebase, refer to networking.h .
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *
 * @file   networking.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This files header */
#include "networking.h"

/* C-standard for string operations */
#include <string.h>

/* Other headers of the component */
#include "wifi.h"

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
 * Determine the maximum number of WiFi networks to be included in a scan.
 *
 * This is used by ::wifi_scan_for_networks.
 */
#define DEFAULT_SCAN_LIST_SIZE 5

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


/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "krachkiste.networking";


/* ***** PROTOTYPES ******************************************************** */
static void wifi_scan_for_networks(void);
static esp_err_t networking_get_wifi_credentials(
    char* wifi_ssid,
    char* wifi_password);


/* ***** FUNCTIONS ********************************************************* */

/**
 * Scan for all available wifi networks.
 *
 * This was actually the first example code, that was imported to get started
 * with this component.
 *
 * As of now, the function is **not in use**.
 *
 * The code is directly fetched from
 * https://github.com/espressif/esp-idf/tree/master/examples/wifi/scan
 */
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
 * Retrieve credentials for a WiFi network from the NVS.
 *
 * The function opens the non-volatile storage namespace (as specified by
 * ::PROJECT_NVS_STORAGE_NAMESPACE) and retrieves the WiFi's ``SSID`` from
 * ::NETWORKING_WIFI_NVS_KEY_SSID and the ``PSK`` from
 * ::NETWORKING_WIFI_NVS_KEY_PSK.
 *
 * @param wifi_ssid     A pointer to a ``char`` array to store the ``SSID``.
 *                      The ``char`` array has to be defined outside of this
 *                      function before calling. It is recommended to declare
 *                      that ``char`` array with length
 *                      ::NETWORKING_WIFI_SSID_MAX_LEN.
 * @param wifi_password A pointer to a ``char`` array to store the ``PSK``. The
 *                      ``char`` array has to be defined outside of this
 *                      function before calling. It is recommended to declare
 *                      that ``char`` array with length
 *                      ::NETWORKING_WIFI_PSK_MAX_LEN.
 * @return              ``ESP_OK`` (equals ``0``) on success, ``ESP_FAIL``
 *                      (equals ``-1``) on failure. Every failed access to the
 *                      NVS will directly return a failure, meaning that the
 *                      credentials could not be retrieved. Every fail will
 *                      also cause emitting of a log message of level ``ERROR``.
 */
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
            NETWORKING_WIFI_NVS_KEY_SSID,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_NVS_KEY_SSID,
        wifi_ssid,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_SSID,
            esp_err_to_name(err));
        return ESP_FAIL;
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
        wifi_password,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_PSK,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Close the handle to the NVS
    nvs_close(storage_handle);

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
    // ESP_LOGD(TAG, "Address of 'ap_netif': %p", networking_wifi_ap_netif);

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
    // ESP_LOGD(TAG, "Address of 'ap_netif': %p", networking_wifi_ap_netif);
}
