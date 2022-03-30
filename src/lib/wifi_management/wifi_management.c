// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide a basic management of the WiFi capabilities of the MCU.
 */

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
*/
#include "esp_log.h"

// This is the components header file (forward declaration)
#include "wifi_management.h"


/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See :idf_api:`its API documentation <system/log.html#how-to-use-this-library>`.
 */
static const char* TAG = "krachkiste.wifi";


void initialize_wifi(void) {
    // set log-level of our own code to DEBUG (sdkconfig.defaults sets the
    // default log-level to INFO)
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "Entering initializewifi()");
}
