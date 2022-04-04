// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * This is the applications main module, providing the required main entry point
 * of the application with its :c:func:`app_main`.
 */

// grabbed this from https://github.com/tonyp7/esp32-wifi-manager/blob/master/examples/default_demo/main/user_main.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* This is ESP-IDF's event library.
 * It is used by several components to publish internal state information to
 * other components as needed.
 * While it is possible to maintain app-specific event loops, most of the
 * components provided by ESP-IDF use a ``default`` event loop, which may be
 * re-used / shared by application code.
 */
#include "esp_event.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"

/* Project-specific library to manage wifi connections. */
#include "networking.h"

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See :idf_api:`its API documentation <system/log.html#how-to-use-this-library>`.
 */
static const char* TAG = "krachkiste.main";

// grabbed this from https://github.com/tonyp7/esp32-wifi-manager/blob/master/examples/default_demo/main/user_main.c
void monitoring_task(void *pvParameter) {
    for (;;) {
        ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/**
 * The application's main entry point.
 */
void app_main(void) {
    // set log-level of our own code to DEBUG (sdkconfig.defaults sets the
    // default log-level to INFO)
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "Entering app_main()");

    // Initialize the default event loop
    // The default event loop is used (and thus, required) by ESP-IDF's esp_wifi
    // component (at least).
    // As the default event loop may be re-used by application code, no
    // application-speciifc custom event loop is created.
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize the non-volatile storage (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // grabbed this from https://github.com/tonyp7/esp32-wifi-manager/blob/master/examples/default_demo/main/user_main.c
    /* your code should go here. Here we simply create a task on core 2 that
     * monitors free heap memory
     */
    xTaskCreatePinnedToCore(
        &monitoring_task,
        "monitoring_task",
        2048,
        NULL,
        1,
        NULL,
        1);

    networking_initialize();
}
