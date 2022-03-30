// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * This is the applications main module, providing the required main entry point
 * of the application with its :c:func:`app_main`.
 */

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
*/
#include "esp_log.h"

#include "wifi_management.h"

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See :idf_api:`its API documentation <system/log.html#how-to-use-this-library>`.
 */
static const char* TAG = "krachkiste.main";

/**
 * The application's main entry point.
 */
void app_main(void) {
    // set log-level of our own code to DEBUG (sdkconfig.defaults sets the
    // default log-level to INFO)
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "Entering app_main()");

    initialize_wifi();
}
