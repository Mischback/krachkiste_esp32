// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide the actual audio player.
 *
 * @file   map32.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "map32/map32.h"

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
 * - provided by ESP-IDF's component ``esp_common``
 */
#include "esp_err.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 * - provided by ESP-IDF's component ``log``
 */
#include "esp_log.h"

/* ***** DEFINES *********************************************************** */

/* ***** TYPES ************************************************************* */

/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "map32";

/* ***** PROTOTYPES ******************************************************** */

/* ***** FUNCTIONS ********************************************************* */

esp_err_t map32_start(void) {
    ESP_LOGV(TAG, "map32_start()");

    return ESP_OK;
}
