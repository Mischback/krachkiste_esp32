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
#include "networking_internal.h"
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

static TaskHandle_t networking_task_handle = NULL;


/* ***** PROTOTYPES ******************************************************** */

static void networking_task(void* pvParameters);

/* ***** FUNCTIONS ********************************************************* */

esp_err_t networking_destroy(void) {
    ESP_LOGV(TAG, "[networking_destroy] entering function...");

    // Terminate the dedicated networking task.
    vTaskDelete(networking_task_handle);
    networking_task_handle = NULL;

    // Deinitialize the underlying TCP/IP stack
    // This operation is currently not supported by **ESP-IDF**, but it is
    // logically the right thing to do.
    // As of now, this returns ``ESP_ERR_NOT_SUPPORTED``.
    if (esp_netif_deinit() != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(
            TAG,
            "[networking_destroy] FAILED: deinitialization of netif returned "
            "with an unexpected return code!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t networking_initialize(char* nvs_namespace) {
    // set log-level of our own code to VERBOSE (sdkconfig.defaults sets the
    // default log-level to INFO)
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "[networking_initialize] entering function...");

    // Initialize the underlying TCP/IP stack
    // This should be done exactly once from application code, so this function
    // seems like a good enough place, as it is the starting point for all
    // networking-related code.
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the actual dedicated task for the networking component.
    // As of now, the only external value which has to been passed to the
    // ``networking_task()`` function is the name of the nvs_namespace, thus,
    // the ``void *`` ``pvParameters`` is re-used and cast accordingly.
    // If there is further need of parameters, a struct has to be specified to
    // pass all parameters down to the task.
    // TODO(mischback) Evaluate and minimize ``usStackDepth``!
    xTaskCreate(
        &networking_task,
        "networking",
        4096,
        nvs_namespace,
        3,
        &networking_task_handle);

    // Determine sucess of task creation and provide return value.
    if (networking_task_handle == NULL) {
        ESP_LOGE(TAG, "[networking_initialize] FAILED: Could not create task!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[networking_initialize] Created networking task!");

    // Give the first command to the networking task: Start WiFi networking!
    xTaskNotifyIndexed(
        networking_task_handle,
        NETWORKING_NOTIFICATION_TASK_INDEX,
        NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT,
        eSetBits);
    return ESP_OK;
}

static void networking_task(void* pvParameters) {
    ESP_LOGV(TAG, "Entering networking_task()");

    BaseType_t notify_return;
    uint32_t notify_value = 0;
    esp_err_t action_return;
    const TickType_t max_block_time = pdMS_TO_TICKS(
        NETWORKING_STATUS_UPDATE_FREQUENCY);

    ESP_LOGD(TAG, "[networking_task] Entering loop!");
    for (;;) {
        // Block this task until a notification to the task is present.
        // - notification is expected at index
        //   ``NETWORKING_NOTIFICATION_TASK_INDEX`` (``0`` for now)
        // - notification bits are **not** cleared on entry...
        // - ... but before leaving the handling
        // - notification will be stored (cpoied) to ``notify_value``
        // - the maximum duration of the blocked state can be configured by
        //   adjusting NETWORKING_STATUS_UPDATE_FREQUENCY)
        notify_return = xTaskNotifyWaitIndexed(
            NETWORKING_NOTIFICATION_TASK_INDEX,
            0x00,
            ULONG_MAX,
            &notify_value,
            max_block_time);
        ESP_LOGV(TAG, "[networking_task] Leaving blocked state!");

        if (notify_return == pdPASS) {
            ESP_LOGD(TAG, "[networking_task] Got a notification!");

            // COMMAND DESTROY
            // This command needs to be given in the following situations:
            // - the networking component had some internal failure, which
            //   can not be recovered
            // - the networking component could not establish a network
            //   connection; shutting down the component to free memory
            if ((notify_value &
                 NETWORKING_NOTIFICATION_COMMAND_DESTROY) != 0) {
                ESP_LOGI(TAG, "[networking_task] Shutting down networking!");
                ESP_ERROR_CHECK(networking_destroy());
            }

            // COMMAND WIFI_START
            // This command starts the WiFi connection process by calling
            // ``wifi_start()``. Depending on its return code, further actions
            // are triggered (see descriptions below).
            if ((notify_value &
                 NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT) != 0) {
                action_return = wifi_start((char*)pvParameters);
                // If ``esp_wifi_init()`` failed, try again.
                if (action_return == NETWORKING_WIFI_RET_INIT_FAILED) {
                    ESP_LOGD(TAG, "[networking_task] FAILED: wifi_start");
                    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_stop());
                    // Second try to start WiFi!
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT_RETRY,
                        eSetBits);
                }
                // If ``wifi_start()`` several times, emit a warning but don't
                // take any further actions.
                else if (action_return == NETWORKING_WIFI_RET_ALREADY_STARTED) {  // linting: expected to fail with [whitespace/line_length] [2], [whitespace/newline] [4], [readability/braces] [5]
                    ESP_LOGW(
                        TAG,
                        "[networking_task] wifi_start() was already called!");
                    ESP_LOGI(TAG, "You may want to 'restart' wifi instead.");
                }
                // If ``wifi_start()`` returns successfully, it's time to
                // actually connect to a WiFi network.
                else if (action_return == NETWORKING_WIFI_RET_OK) {  // linting: expected to fail with [whitespace/line_length] [2], [whitespace/newline] [4], [readability/braces] [5]
                    ESP_LOGD(TAG, "[networking_task] wifi_start(): Success!");
                    // Successfully started WiFi, now initialize station mode.
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_WIFI_STA_INIT,
                        eSetBits);
                }
            }

            // COMMAND WIFI_START_RETRY
            // If the first call to ``wifi_start()`` failed, retry.
            // If this call fails too, the networking component will assume,
            // that there is a non-recoverable failure and will emit a
            // notification to shut down networking completely (see
            // COMMAND DESTROY above).
            // THIS IS A VERY SIMPLE IMPLEMENTATION! It relies on the fact,
            // that this notification is not issued from outside of this
            // function, specifically from COMMAND WIFI_START.
            if ((notify_value &
                 NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT_RETRY) != 0) {
                if (wifi_start((char*)pvParameters) != NETWORKING_WIFI_RET_OK) {
                    ESP_LOGE(
                        TAG,
                        "[networking_task] FAILED: wifi_start failed again!");
                    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_stop());
                    // wifi_start() failed two times, something is terribly
                    // wrong. Kill networking!
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_DESTROY,
                        eSetBits);
                } else {
                    ESP_LOGD(TAG, "[networking_task] wifi_start(): Success!");
                    // Successfully started WiFi, now initialize station mode.
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_WIFI_STA_INIT,
                        eSetBits);
                }
            }

            // COMMAND WIFI_STA_INIT
            // The component is in a state, where WiFi is ready. Now initialize
            // the station mode.
            if ((notify_value &
                 NETWORKING_NOTIFICATION_COMMAND_WIFI_STA_INIT) != 0) {
                ESP_LOGV(TAG, "[networking_task] COMMAND WIFI_STA_INIT");
                action_return = wifi_sta_initialize();

                // ``wifi_sta_initialize()`` checks for credentials. If there
                // are no credentials, it exits as soon as possible with this
                // specific return value.
                // The component may now directly start the access point mode.
                if (action_return == NETWORKING_WIFI_RET_CONFIG_UNAVAILABLE) {
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_WIFI_AP_INIT,
                        eSetBits);
                }
                // Initialization is completed, ready to go!
                else if (action_return == NETWORKING_WIFI_RET_OK) {  // linting: expected to fail with [whitespace/line_length] [2], [whitespace/newline] [4], [readability/braces] [5]
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_WIFI_START,
                        eSetBits);
                }
                // At this point, some error condition is reached, that
                // basically means, that a call of **ESP-IDF**'s code failed.
                // This is **not recoverable**!
                else {  // linting: expected to fail with [whitespace/line_length] [2], [whitespace/newline] [4], [readability/braces] [5]
                    xTaskNotifyIndexed(
                        networking_task_handle,
                        NETWORKING_NOTIFICATION_TASK_INDEX,
                        NETWORKING_NOTIFICATION_COMMAND_DESTROY,
                        eSetBits);
                }
            }

        } else {
            // TODO(mischback) This might be used to have periodically status
            //                 updates to other components (using the event
            //                 system)
            ESP_LOGD(TAG, "[networking_task] No pending notification!");
        }
    }

    // Most likely, this statement will never be reached, because it is placed
    // after the infinite loop.
    // However, freeRTOS requires ``task`` functions to *never return*, and
    // this statement will terminate and remove the task before returning.
    // It's kind of the common and recommended idiom.
    // ``vTaskDelete(NULL)`` means: *Delete your very own task*.
    vTaskDelete(NULL);
}
