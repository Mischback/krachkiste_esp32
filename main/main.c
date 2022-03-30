/*
 * SPDX-FileCopyrightText: 2022 Mischback
 * SPDX-License-Identifier: MIT
 * SPDX-FileType: SOURCE
 *
 * This is the applications main file, providing the required main entry point
 * of the application.
 */

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
*/
#include <esp_log.h>

// label for the file (or module)-specific logger
static const char* TAG = "Krachkiste.main";

void app_main(void) {
    // set log-level of our own code to DEBUG (sdkconfig.defaults sets the
    // default log-level to INFO)
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "Entering app_main()");
}
