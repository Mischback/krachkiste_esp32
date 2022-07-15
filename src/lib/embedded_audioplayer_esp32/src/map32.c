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

static esp_err_t map32_init(void);
static esp_err_t map32_deinit(void);


/* ***** FUNCTIONS ********************************************************* */

// TODO(mischback) Add documentation!
static esp_err_t map32_init(void) {
    /* Adjust log settings!
     * The component's logging is based on **ESP-IDF**'s logging library,
     * meaning it will behave exactly like specified by the settings specified
     * in ``sdkconfig``/``menuconfig``.
     *
     * However, during development it may be desirable to activate a MAXIMUM log
     * level of VERBOSE, while keeping the DEFAULT log level to INFO. In this
     * case, the component's logging may be set to VERBOSE manually here!
     *
     * Additionally, **ESP-IDF**'s and **ESP-ADF**'s internal components, that
     * are related to or used during audio playback, are silenced here.
     */
    esp_log_level_set("map32", ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "map32_init()");

    return ESP_OK;
}

// TODO(mischback) Add documentation!
static esp_err_t map32_deinit(void) {
    ESP_LOGV(TAG, "map32_deinit()");

    return ESP_OK;
}

esp_err_t map32_start(void) {
    ESP_LOGV(TAG, "map32_start()");

    if (map32_init() != ESP_OK) {
        map32_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t map32_stop(void) {
    ESP_LOGV(TAG, "map32_stop()");

    // TODO(mischback) Actually implement this function. Most likely, the
    //                 implementation will just send a message to the
    //                 component's task (see ``mnet32.c``)

    return ESP_OK;
}
