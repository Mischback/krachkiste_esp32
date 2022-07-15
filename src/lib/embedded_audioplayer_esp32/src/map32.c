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

#include "audio_pipeline.h"

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
#include "i2s_stream.h"
#include "mp3_decoder.h"

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

static audio_pipeline_handle_t map32_pipeline = NULL;
static audio_element_handle_t map32_sink = NULL;
static audio_element_handle_t map32_decoder = NULL;

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

    ESP_LOGV(TAG, "Building default pipeline configuration...");
    audio_pipeline_cfg_t pipeline_config = DEFAULT_AUDIO_PIPELINE_CONFIG();
    map32_pipeline = audio_pipeline_init(&pipeline_config);
    if (map32_pipeline == NULL) {
        ESP_LOGE(TAG, "Could not initialize audio pipeline!");
        return ESP_FAIL;
    }

    ESP_LOGV(TAG, "Setting up common audio elements...");

    ESP_LOGV(TAG, "Setup of I2S stream writer...");
    i2s_stream_cfg_t i2s_config = I2S_STREAM_CFG_DEFAULT();
    i2s_config.type = AUDIO_STREAM_WRITER;
    // TODO(mischback) This is probably the place to set the actual pins of
    //                 I2S, but there is no example actually doing this. Needs
    //                 more research!
    //                 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
    //                 i2s_config.i2s_config.gpio_cfg.mclk = (?!?)
    // TODO(mischback) This may be guarded with an #ifdef to make this
    //                 compatible with ESP-ADF's logic
    map32_sink = i2s_stream_init(&i2s_config);
    if (map32_sink == NULL) {
        ESP_LOGE(TAG, "Could not initialize i2s_writer!");
        return ESP_FAIL;
    }

    ESP_LOGV(TAG, "Setup of MP3 decoder...");
    mp3_decoder_cfg_t mp3_config = DEFAULT_MP3_DECODER_CONFIG();
    map32_decoder = mp3_decoder_init(&mp3_config);
    if (map32_decoder == NULL) {
        ESP_LOGE(TAG, "Could not initialize decoder!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// TODO(mischback) Add documentation!
static esp_err_t map32_deinit(void) {
    ESP_LOGV(TAG, "map32_deinit()");

    esp_err_t esp_ret;

    ESP_LOGV(TAG, "Deinitializing audio pipeline...");
    esp_ret = audio_pipeline_deinit(map32_pipeline);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize audio pipeline.");
        ESP_LOGD(TAG, "Calling free()");
        free(map32_pipeline);
    }

    esp_ret = audio_element_deinit(map32_sink);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize i2s_writer.");
        ESP_LOGD(TAG, "Calling free()");
        free(map32_sink);
    }

    esp_ret = audio_element_deinit(map32_decoder);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize decoder.");
        ESP_LOGD(TAG, "Calling free()");
        free(map32_decoder);
    }

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
