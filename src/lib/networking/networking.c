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


#define DEFAULT_SCAN_LIST_SIZE 5


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

    char* ssid = "foobar";
    char* password = "123456789a";

    strncpy(wifi_ssid, ssid, strlen(ssid) + 1);
    strncpy(wifi_password, password, strlen(password) + 1);

    return ESP_OK;
}


void networking_initialize(void) {
    // set log-level of our own code to VERBOSE (sdkconfig.defaults sets the
    // default log-level to INFO)
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ESP_LOGV(TAG, "Entering networking_initialize()");

    // Read WiFi credentials from non-volatile storage (NVS)
    // TODO(mischback): Make this configurable, IEEE says 32 chars
    char wifi_ssid[33];
    // TODO(mischback): Make this configurable, IEEE says 64 chars
    char wifi_password[65];
    memset(&wifi_ssid, 0, sizeof(wifi_ssid));
    memset(&wifi_password, 0, sizeof(wifi_password));
    ESP_ERROR_CHECK(
        networking_get_wifi_credentials(wifi_ssid, wifi_password));
    ESP_LOGD(TAG, "SSID: >%s<", wifi_ssid);
    ESP_LOGD(TAG, "Password: >%s<", wifi_password);
}
