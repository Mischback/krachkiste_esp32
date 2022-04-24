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
 * @todo   Evaluate and clear the included libraries!
 */

/* This file's header */
#include "networking/networking.h"

/* C-standard for string operations */
#include <string.h>

/* Other headers of the component */
#include "wifi.h"

/* The FreeRTOS headers are required for timers */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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


/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";


/* ***** PROTOTYPES ******************************************************** */

static void networking_task(void* pvParameters);

/* ***** FUNCTIONS ********************************************************* */

static void networking_task(void* pvParameters) {
    ESP_LOGV(TAG, "Entering networking_task()");

    ESP_LOGD(TAG, "[networking_task()] Entering loop!");
    for (;;) {
        // TODO(mischback): get rid of this as soon as possible!
        ESP_LOGV(TAG, "Inside infinite loop!");
    }

    // Most likely, this statement will never be reached, because it is placed
    // after the infinite loop.
    // However, freeRTOS requires ``task`` functions to *never return*, and
    // this statement will terminate and remove the task before returning.
    // It's kind of the common and recommended idiom.
    vTaskDelete(NULL);
}

esp_err_t networking_initialize(char* nvs_namespace) {
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

    xTaskCreate(&networking_task, "networking_task", 4096, NULL, 3, NULL);

    // return wifi_initialize(nvs_namespace);
    return ESP_OK;
}
