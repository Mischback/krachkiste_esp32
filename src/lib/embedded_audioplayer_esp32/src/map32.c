// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide the actual audio player.
 *
 * **Resources:**
 * - https://github.com/espressif/esp-adf/blob/master/examples/player/pipeline_http_mp3/main/play_http_mp3_example.c
 * - https://github.com/BM45/iRadioMini/blob/master/main/modules/player.c
 *
 * @file   map32.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 *
 * @todo In order to make the http_stream_reader work with "https://...", the
 *       certificate check of ESP-TLS has to be disabled. Probably the best way
 *       is to include the corresponding options in ``sdkconfig.defaults``, as
 *       shown here: https://github.com/BM45/iRadioMini/blob/master/sdkconfig.defaults
 *       THIS MAY BE UNSAFE, but as the project only perform HTTPS requests to
 *       radio streams, it should be good enough.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "map32/map32.h"

/* ESP-ADF's audio processing pipeline
 * - provided by ESP-ADF's component ``audio_pipeline``
 */
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

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``queue.h`` for the internal command queue
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* ESP-ADF's abstraction of HTTP (input) streams
 * - provided by ESP-ADF's component ``audio_stream``
 */
#include "http_stream.h"

/* ESP-ADF's abstraction of I2S streams
 * - provided by ESP-ADF's component ``audio_stream``
 */
#include "i2s_stream.h"

/* ESP-ADF's included decoder
 * - provided by ESP-ADF's component ``esp-adf-libs`` (this is one of the
 *   components that are not provided as source code!)
 */
#include "mp3_decoder.h"

/* ***** DEFINES *********************************************************** */

/**
 * The stack size to allocate for player's control task / thread.
 *
 * @todo The actually required stack size is dependent on various other
 *       settings, e.g. the configured log level. As of now, the value has to be
 *       adjusted by modifying this constant.
 *       Using ``uxTaskGetStackHighWaterMark()`` the stack size can be
 *       evaluated.
 */
#define MAP32_CTRL_TASK_STACK_SIZE 2048


/* ***** TYPES ************************************************************* */

/**
 * Define the selectable audio sources.
 *
 * While the audio pipeline has some fixed elements, like the mp3 decoder and
 * the I2S output, the input is actually dependent on the actual source.
 * This ``enum`` defines all accepted sources that are then initialized and
 * added to the audio pipeline as required.
 *
 * @todo Provide a readable string based name for the sources, e.g. by defining
 *       a ``static const char[] *`` (syntax may vary!), so that the value of
 *       this enum may be used as index to the ``char*`` array.
 * @todo Implement helper functions to automatically get the previous and next
 *       value in this enum (basically ``+1`` and ``-1``, but in a circle).
 */
typedef enum {
    MAP32_SOURCE_HTTP = 0,
} map32_source;

/**
 * Track the playback status of the player.
 */
typedef enum {
    MAP32_STATUS_NOT_READY = 0,
    MAP32_STATUS_READY,
    MAP32_STATUS_PLAYING,
} map32_status;

/**
 * Track the internal status of the player.
 */
struct map32_state {
    map32_status status;
    map32_source source;
    QueueHandle_t cmd_queue;
    TaskHandle_t ctrl_task;
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t audio_source;
    audio_element_handle_t audio_decoder;
    audio_element_handle_t audio_sink;
};


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "map32";

static struct map32_state* state = NULL;


/* ***** PROTOTYPES ******************************************************** */

static audio_element_handle_t map32_audio_source_init(map32_source source);
static void map32_ctrl_func(void* task_parameters);
static esp_err_t map32_init(void);
static esp_err_t map32_deinit(void);


/* ***** FUNCTIONS ********************************************************* */

// TODO(mischback) Add documentation!
static audio_element_handle_t map32_audio_source_init(map32_source source) {
    ESP_LOGV(TAG, "map32_audio_source_init()");
    ESP_LOGV(TAG, "source: %d", source);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    return http_stream_init(&http_cfg);
}

BaseType_t map32_ctrl_command(map32_command cmd) {
    ESP_LOGV(TAG, "map32_ctrl_command()");

    map32_command tmp = cmd;
    return xQueueSend(state->cmd_queue, &tmp, portMAX_DELAY);
}

// TODO(mischback) Add documentation!
static void map32_ctrl_func(void* task_parameters) {
    ESP_LOGV(TAG, "map32_ctrl_func()");

    // TODO(mischback) Make this configurable!
    const TickType_t mon_freq = pdMS_TO_TICKS(5000);
    map32_command cmd;
    BaseType_t queue_status;

    for (;;) {
        queue_status = xQueueReceive(state->cmd_queue, &cmd, mon_freq);

        if (queue_status == pdPASS) {
            switch (cmd) {
            case MAP32_CMD_START:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_START");

                /* Build the audio pipeline, depending on the input source and
                 * start the corresponding task.
                 */
                // TODO(mischback) Implement something to start at the same
                //                 source / station / track that was played
                //                 before the ESP32 was shut down.
                //                 See corresponding TODO in map32_init()!

                /* Actually build the audio pipeline with the (pre-initialized)
                 * audio elements by linking them together, switching the
                 * internal state to MAP32_STATUS_READY and then issue the
                 * "play" command.
                 */
                audio_pipeline_register(state->pipeline,
                                        state->audio_source,
                                        "source");
                audio_pipeline_register(state->pipeline,
                                        state->audio_decoder,
                                        "decoder");
                audio_pipeline_register(state->pipeline,
                                        state->audio_sink,
                                        "sink");
                const char* link_tag[3] = {"source", "decoder", "sink"};
                audio_pipeline_link(state->pipeline, &link_tag[0], 3);

                // TODO(mischback) This is just a temporary hack, this has to be
                //                 handled in a dedicated logic, depending on
                //                 the saved "last played" song/station.
                audio_element_set_uri(
                    state->audio_source,
                    "https://wdr-1live-live.icecastssl.wdr.de/wdr/1live/live/"
                    "mp3/128/stream.mp3");

                state->status = MAP32_STATUS_READY;
                map32_ctrl_command(MAP32_CMD_PLAY);
                break;
            case MAP32_CMD_PLAY:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_PLAY");

                /* Depending on the internal state, this should make the music
                 * play. If the player is already running, this command does not
                 * have any effect.
                 */
                if (state->status != MAP32_STATUS_READY) {
                    ESP_LOGW(TAG,
                             "Received command PLAY but internal state is not "
                             "READY!");
                    continue;
                }

                audio_pipeline_run(state->pipeline);
                state->status = MAP32_STATUS_PLAYING;
                break;
            case MAP32_CMD_PAUSE:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_PAUSE");

                /* If the player is already running and actually playing music,
                 * this command will pause playback.
                 */
                break;
            case MAP32_CMD_STOP:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_STOP");

                /* Totally stop playback and "destroy" the active audio pipeline
                 * to free resources. Highly dependent on the actual state of
                 * the player.
                 */
                break;
            case MAP32_CMD_PREV_SOURCE:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_PREV_SOURCE");

                /* Destroy the current audio pipeline and rebuild it with a new
                 * source, depending on "some list"
                 */
                break;
            case MAP32_CMD_NEXT_SOURCE:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_NEXT_SOURCE");

                /* Destroy the current audio pipeline and rebuild it with a new
                 * source, depending on "some list"
                 */
                break;
            case MAP32_CMD_PREV_TRACK:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_PREV_TRACK");

                /* Switch to the prev track of the current playlist.
                 * (this may even be a list of radio stations)
                 */
                // TODO(mischback) If a single mp3 song is played, from SD card
                //                 or a network share, should this only restart
                //                 the current track?!
                break;
            case MAP32_CMD_NEXT_TRACK:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_NEXT_TRACK");

                /* Switch to the prev track of the current playlist.
                 * (this may even be a list of radio stations)
                 */
                break;
            case MAP32_CMD_VOLUP:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_VOLUP");

                /* Adjust the volume! */
                // TODO(mischback) How to adjust the volume (in software), if
                //                 an external DAC / Amp is used? Is this
                //                 possible in I2S?!
                break;
            case MAP32_CMD_VOLDOWN:
                ESP_LOGD(TAG, "map32_ctrl_func: MAP32_CMD_VOLDOWN");

                /* Adjust the volume! */
                // TODO(mischback) How to adjust the volume (in software), if
                //                 an external DAC / Amp is used? Is this
                //                 possible in I2S?!
                break;
            }
        } else {
            ESP_LOGD(TAG, "'mon_freq' reached...");
        }
    }

    /* This should probably not be reached!
     * ``freeRTOS`` requires the task functions *to never return*. Instead,
     * the common idiom is to delete the very own task at the end of these
     * functions.
     */
    vTaskDelete(NULL);
}

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

    /* initialize internal state */
    state = calloc(1, sizeof(*state));
    state->status = MAP32_STATUS_NOT_READY;
    // TODO(mischback) This is the point to retrieve a saved source, basically
    //                 making it possible to start the player with the last
    //                 played song/station, resuming operation after a reboot
    state->source = MAP32_SOURCE_HTTP;
    state->cmd_queue = xQueueCreate(3, sizeof(map32_command));

    ESP_LOGD(TAG, "Building default pipeline configuration...");
    audio_pipeline_cfg_t pipeline_config = DEFAULT_AUDIO_PIPELINE_CONFIG();
    state->pipeline = audio_pipeline_init(&pipeline_config);
    if (state->pipeline == NULL) {
        ESP_LOGE(TAG, "Could not initialize audio pipeline!");
        return ESP_FAIL;
    }

    ESP_LOGV(TAG, "Setting up common audio elements...");

    ESP_LOGD(TAG, "Setup of I2S stream writer...");
    i2s_stream_cfg_t i2s_config = I2S_STREAM_CFG_DEFAULT();
    i2s_config.type = AUDIO_STREAM_WRITER;
    // TODO(mischback) This is probably the place to set the actual pins of
    //                 I2S, but there is no example actually doing this. Needs
    //                 more research!
    //                 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
    //                 i2s_config.i2s_config.gpio_cfg.mclk = (?!?)
    // TODO(mischback) This may be guarded with an #ifdef to make this
    //                 compatible with ESP-ADF's logic
    state->audio_sink = i2s_stream_init(&i2s_config);
    if (state->audio_sink == NULL) {
        ESP_LOGE(TAG, "Could not initialize i2s_writer!");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Setup of MP3 decoder...");
    mp3_decoder_cfg_t mp3_config = DEFAULT_MP3_DECODER_CONFIG();
    state->audio_decoder = mp3_decoder_init(&mp3_config);
    if (state->audio_decoder == NULL) {
        ESP_LOGE(TAG, "Could not initialize decoder!");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Setup of last audio input...");
    ESP_LOGV(TAG, "as of now, this will be a hardcoded HTTP stream!");
    state->audio_source = map32_audio_source_init(state->source);
    if (state->audio_source == NULL) {
        ESP_LOGE(TAG, "Could not initialize audio source!");
        return ESP_FAIL;
    }

    /* Create the component's tasks  */
    if (xTaskCreate(map32_ctrl_func,
                    "map32_ctrl_task",
                    MAP32_CTRL_TASK_STACK_SIZE,
                    NULL,
                    MAP32_CTRL_TASK_PRIORITY,
                    state->ctrl_task) != pdPASS) {
        ESP_LOGE(TAG, "Could not create control task!");
        return ESP_FAIL;
    }

    map32_ctrl_command(MAP32_CMD_START);

    return ESP_OK;
}

// TODO(mischback) Add documentation!
static esp_err_t map32_deinit(void) {
    ESP_LOGV(TAG, "map32_deinit()");

    esp_err_t esp_ret;

    ESP_LOGV(TAG, "Deinitializing audio pipeline...");
    esp_ret = audio_pipeline_deinit(state->pipeline);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize audio pipeline.");
    }

    esp_ret = audio_element_deinit(state->audio_sink);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize i2s_writer.");
    }

    esp_ret = audio_element_deinit(state->audio_decoder);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not deinitialize decoder.");
    }

    if (state->ctrl_task != NULL) {
        vTaskDelete(state->ctrl_task);
    }

    if (state->cmd_queue != NULL) {
        vQueueDelete(state->cmd_queue);
    }

    free(state);
    state = NULL;

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
