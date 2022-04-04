// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide a basic management of the WiFi capabilities of the MCU.
 */

// The components header files (forward declaration)
#include "include/networking.h"  // public header
#include "networking_p.h"  // private header

/* C-standard for string operations */
#include <string.h>

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ESP_ERROR_CHECK
 * - defines the return values ESP_OK (0) and ESP_FAIL (-1)
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

/* ESP-IDF's wifi library */
#include "esp_wifi.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


#define DEFAULT_SCAN_LIST_SIZE 5

/**
 * The project-specific namespace to access the non-volatile storage.
 *
 * TODO(mischback): Make this configurable!
 *                  Respect NVS_KEY_NAME_SIZE - 1 as per https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#_CPPv48nvs_openPKc15nvs_open_mode_tP12nvs_handle_t
 */
#define PROJECT_NVS_STORAGE_NAMESPACE "krachkiste"

/**
 * The maximum length of the :c:type:`char` array to store SSID.
 *
 * IEEE says, that the maximum length of an SSID is 32, so this is set to 33,
 * to allow for the terminating ``\0``.
 *
 * TODO(mischback): Should this be made configurable by project settings?
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 33

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 *
 * TODO(mischback): Make this configurable!
 */
#define NETWORKING_WIFI_SSID_NVS_KEY "net_ssid"

/**
 * The maximum length of the :c:type:`char` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE says, that the maximum length of an PSK is 64, so this is set to 65,
 * to allow for the terminating ``\0``.
 *
 * TODO(mischback): Should this be made configurable by project settings?
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 65

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 *
 * TODO(mischback): Make this configurable!
 */
#define NETWORKING_WIFI_PSK_NVS_KEY "net_pass"


/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See :idf_api:`its API documentation <system/log.html#how-to-use-this-library>`.
 */
static const char* TAG = "krachkiste.networking";


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


void networking_initialize(void) {
    // set log-level of our own code to VERBOSE (sdkconfig.defaults sets the
    // default log-level to INFO)
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ESP_LOGV(TAG, "Entering networking_initialize()");

    // Read WiFi credentials from non-volatile storage (NVS)
    char wifi_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char wifi_password[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(&wifi_ssid, 0, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(&wifi_password, 0, NETWORKING_WIFI_PSK_MAX_LEN);
    esp_err_t err = networking_get_wifi_credentials(wifi_ssid, wifi_password);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Could not read WiFi credentials from NVS");
        ESP_LOGD(TAG, "Try to start access point now!");
    } else {
        ESP_LOGD(TAG, "SSID: >%s<", wifi_ssid);
        ESP_LOGD(TAG, "Password: >%s<", wifi_password);
    }
}
