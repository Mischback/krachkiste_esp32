// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide basic management of the networking capabilities of the MCU.
 *
 * This file is the actual implementation of the component. For a detailed
 * description of the actual usage, including how the component may be reused
 * in some other codebase, refer to mnet32.h .
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html
 *
 * @file   mnet32.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "mnet32/mnet32.h"

/* C's standard libraries. */
#include <string.h>

/* Other headers of the component. */
#include "mnet32_internal.h"  // The private header
#include "mnet32_state.h"     // manage the internal state
#include "mnet32_wifi.h"      // WiFi-related functions

/* This is ESP-IDF's error handling library.
 * - defines the **type** ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
 */
#include "esp_err.h"

/* This is ESP-IDF's event library. */
#include "esp_event.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ESP-IDF's network abstraction layer. */
#include "esp_netif.h"

/* ESP-IDF's wifi library.
 * The header has to be included here, because the component uses one single
 * event handler function (::mnet32_event_handler), thus, the specific
 * WiFi-related events must be known.
 */
#include "esp_wifi.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* ***** DEFINES *********************************************************** */

/**
 * The index to be used by ``xTaskNotifyWaitIndex``.
 *
 * **freeRTOS** does support tasks with with multiple *notifications*, however,
 * **ESP-IDF** limits this to just one, which is placed at index ``0`` (multiple
 * notification values are supported starting at 10.4.0).
 *
 * The component however uses the generic versions of ``xTaskNotifyWait()`` and
 * ``xTaskNotify()``, so this constant ensures, that the notifications will be
 * send to the right *receiver*.
 */
#define MNET32_TASK_NOTIFICATION_INDEX 0

/**
 * The stack size to allocate for this component's task / thread.
 *
 * @todo The actually required stack size is dependent on various other
 *       settings, e.g. the configured log level. As of now, the value has to be
 *       adjusted by modifying this constant.
 *       Using ``uxTaskGetStackHighWaterMark()`` the stack size can be
 *       evaluated:
 *         - stack size: 4096, maximum log level INFO :    2224 bytes;
 *         - stack size: 2048, maximum log level INFO :     188 bytes;
 *         - stack size: 2048, maximum log level VERBOSE : PANIC, Stack Overflow
 *         - stack size: 3072, maximum log level VERBOSE :  772 bytes;
 *         - stack size: 3072, maximum log level INFO :    1212 bytes;
 */
#define MNET32_TASK_STACK_SIZE 3072


/* ***** TYPES ************************************************************* */

/* Define the component-specific event base. */
ESP_EVENT_DEFINE_BASE(MNET32_EVENTS);


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "mnet32";


/* ***** PROTOTYPES ******************************************************** */

static void mnet32_task(void* task_parameters);
static esp_err_t mnet32_deinit(void);
static esp_err_t mnet32_init(void);
static void mnet32_emit_event(int32_t event_id, void* event_data);


/* ***** FUNCTIONS ********************************************************* */

/**
 * Run the component's specific task.
 *
 * This function is the actual dedicated task of the component. It blocks until
 * some external event (in most cases provided by **ESP-IDF**'s internal
 * modules) requires some reaction.
 *
 * There is a timeout to this blocking. After this timeout is reached, the
 * function will emit a component-specific event, publishing its internal state
 * to other components.
 *
 * ``freeRTOS``-tasks are not meant to terminate, so this function basically
 * implements an infinite loop.
 *
 * @param task_parameters As per ``freeRTOS`` prototype, currently not used.
 *
 * @todo The function's implementation has several spots marked to be considered
 *       when #16 is implemented. #16 is the issue which tracks publishing of
 *       the internal state of the component.
 */
static void mnet32_task(void* task_parameters) {
    ESP_LOGV(TAG, "mnet32_task() [the actual task function]");

    const TickType_t mon_freq = pdMS_TO_TICKS(MNET32_TASK_MONITOR_FREQUENCY);
    BaseType_t notify_result;
    uint32_t notify_value = 0;

    for (;;) {
        /* Block until notification or ``mon_freq`` reached. */
        notify_result = xTaskNotifyWaitIndexed(MNET32_TASK_NOTIFICATION_INDEX,
                                               pdFALSE,
                                               ULONG_MAX,
                                               &notify_value,
                                               mon_freq);

        /* Notification or monitoring? */
        if (notify_result == pdPASS) {
            switch (notify_value) {
            case MNET32_NOTIFICATION_CMD_NETWORKING_STOP:
                ESP_LOGD(TAG, "CMD: NETWORKING_STOP");

                /* Emit the corresponding event *before* actually shutting down
                 * the networking. This might give other components some time
                 * to handle the unavailability of networking.
                 */
                mnet32_emit_event(MNET32_EVENT_UNAVAILABLE, NULL);
                mnet32_deinit();
                break;
            case MNET32_NOTIFICATION_CMD_WIFI_START:
                ESP_LOGD(TAG, "CMD: WIFI_START");

                if (mnet32_wifi_start() != ESP_OK) {
                    ESP_LOGE(TAG, "Could not start WiFi!");
                }
                break;
            case MNET32_NOTIFICATION_CMD_WIFI_RESTART:
                ESP_LOGD(TAG, "CMD: WIFI_RESTART");

                /* Emit the corresponding event *before* actually shutting down
                 * the networking. This might give other components some time
                 * to handle the unavailability of networking.
                 */
                mnet32_emit_event(MNET32_EVENT_UNAVAILABLE, NULL);

                mnet32_wifi_deinit();
                if (mnet32_wifi_start() != ESP_OK) {
                    ESP_LOGE(TAG, "Could not restart WiFi!");
                }
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_AP_START:
                /* Handle ``WIFI_EVENT_AP_START`` (received from
                 * ::mnet32_event_handler ).
                 * The *chain* of ::mnet32_wifi_start, ::mnet32_wifi_init and ::mnet32_wifi_ap_init
                 * has set ``state->medium`` and ``state->mode``, so with this
                 * event the access point is assumed to be ready, resulting in
                 * ``state->status = MNET32_STATUS_IDLE``, because no
                 * clients have connected yet.
                 */
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_START");

                mnet32_state_set_status_idle();
                mnet32_wifi_ap_timer_start();

                mnet32_emit_event(MNET32_EVENT_READY, NULL);
                // TODO(mischback) Should the *status event* be emitted here
                //                 automatically (#16)?
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED:
                /* A client connected to the access point.
                 * Set the internal state to MNET32_STATUS_BUSY to indicate
                 * actual usage of the access point. The internal timer to
                 * shut down the access point has to be stopped, because a
                 * client *might be* consuming the web interface, so the
                 * access point has to be kept running.
                 */
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_STACONNECTED");

                mnet32_state_set_status_busy();
                mnet32_wifi_ap_timer_stop();

                // TODO(mischback) Emit *status event* (#16)!
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_STADISCONNECTED");

                if (mnet32_wifi_ap_get_connected_stations() == 0) {
                    mnet32_state_set_status_idle();

                    ESP_LOGD(TAG,
                             "No more stations connected, restarting shutdown "
                             "timer!");  // NOLINT(whitespace/line_length)
                    mnet32_wifi_ap_timer_start();
                }

                // TODO(mischback) Emit *status event* (#16)!
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_STA_START:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_START");

                mnet32_state_set_status_connecting();
                mnet32_wifi_sta_connect();
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_STA_CONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_CONNECTED");

                mnet32_state_set_status_ready();
                mnet32_wifi_sta_reset_connection_counter();
                mnet32_emit_event(MNET32_EVENT_READY, NULL);
                // TODO(mischback) Emit *status event* (#16)!
                break;
            case MNET32_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_DISCONNECTED");

                if (mnet32_wifi_sta_get_num_connection_attempts() >
                    MNET32_WIFI_STA_MAX_CONNECTION_ATTEMPTS) {
                    mnet32_wifi_sta_deinit();
                    if (mnet32_wifi_ap_init() != ESP_OK)
                        mnet32_notify(MNET32_NOTIFICATION_CMD_NETWORKING_STOP);
                } else {
                    ESP_LOGI(TAG,
                             "Got disconnected, trying to reconnect (%d/%d)",
                             mnet32_wifi_sta_get_num_connection_attempts(),
                             MNET32_WIFI_STA_MAX_CONNECTION_ATTEMPTS);
                    mnet32_wifi_sta_connect();
                }
                break;
            default:
                ESP_LOGW(TAG, "Got unhandled notification: %d", notify_value);
                break;
            }
        } else {
            ESP_LOGV(TAG, "'mon_freq' reached...");
            // TODO(mischback) Emit *status event* (#16)!

            /* The following statement is just used for development / debugging
             * and logs the (minimum) free stack size of this task in bytes.
             */
            // ESP_LOGW(TAG,
            //          "Stack High Water Mark: %d",
            //          uxTaskGetStackHighWaterMark(NULL));
        }
    }

    /* This should probably not be reached!
     * ``freeRTOS`` requires the task functions *to never return*. Instead,
     * the common idiom is to delete the very own task at the end of these
     * functions.
     */
    vTaskDelete(NULL);
}

void mnet32_event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data) {
    ESP_LOGV(TAG, "mnet32_event_handler()");

    if (event_base == WIFI_EVENT) {
        /* All WIFI_EVENT event_ids
         * See
         * [ESP-IDF WiFi Events](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#esp32-wi-fi-event-description)
         *
         * While all possible EVENT_IDs are listed here, the unused ones are
         * commented out. Manual testing reveals, that this saves some byte in
         * the actual build size. However, they are kept for reference and
         * possible future use.
         */
        switch (event_id) {
        // case WIFI_EVENT_WIFI_READY:
        //     ESP_LOGV(TAG, "WIFI_EVENT_WIFI_READY");
        //     break;
        // case WIFI_EVENT_SCAN_DONE:
        //     ESP_LOGV(TAG, "WIFI_EVENT_SCAN_DONE");
        //     break;
        case WIFI_EVENT_STA_START:
            /* This event is emitted by ``esp_wifi`` when the interface is
             * successfully started in station mode.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_STA_START);
            break;
        // case WIFI_EVENT_STA_STOP:
        //     ESP_LOGV(TAG, "WIFI_EVENT_STA_STOP");
        //     break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_CONNECTED");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_STA_CONNECTED);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            /* This event is emitted by ``esp_wifi`` in the following
             * situations:
             *   a) The application triggers the shutdown of an established
             *      WiFi connection.
             *   b) ``esp_wifi_connect()`` failed to establish a connection.
             *      This is the case if the specified SSID is not reachable or
             *      the provided PSK is rejected.
             *   c) The connection got disrupted, either by the WiFi's access
             *      point or some other external circumstances, e.g. zombies.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED);
            break;
        // case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        //     ESP_LOGV(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGED");
        //     break;
        // case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        //     ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_SUCCESS");
        //     break;
        // case WIFI_EVENT_STA_WPS_ER_FAILED:
        //     ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_FAILED");
        //     break;
        // case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        //     ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_TIMEOUT");
        //     break;
        // case WIFI_EVENT_STA_WPS_ER_PIN:
        //     ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_PIN");
        //     break;
        case WIFI_EVENT_AP_START:
            /* This event is emitted by ``esp_wifi`` when the access point is
             * successfully started.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_START");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_AP_START);
            break;
        // case WIFI_EVENT_AP_STOP:
        //     ESP_LOGV(TAG, "WIFI_EVENT_AP_STOP");
        //     break;
        case WIFI_EVENT_AP_STACONNECTED:
            /* This event is emitted by ``esp_wifi`` when a client connects to
             * the access point.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STACONNECTED");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            /* This event is emitted by ``esp_wifi`` when a client disconnects
             * from the access point.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            mnet32_notify(MNET32_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED);
            break;
        // case WIFI_EVENT_AP_PROBEREQRECVED:
        //     ESP_LOGV(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
        //     break;
        default:
            ESP_LOGD(TAG, "Got unhandled WIFI_EVENT: '%d'", event_id);
            break;
        }
    }

    if (event_base == IP_EVENT) {
        switch (event_id) {
        // case IP_EVENT_STA_GOT_IP:
        //     ESP_LOGV(TAG, "IP_EVENT_STA_GOT_IP");
        //     break;
        // case IP_EVENT_STA_LOST_IP:
        //     ESP_LOGV(TAG, "IP_EVENT_STA_LOST_IP");
        //     break;
        case IP_EVENT_AP_STAIPASSIGNED:
            // clang-format: off
            {}  // empty statement required to enable variable declaration!
            //  clang-format: on
            /* This event is emitted whenever a client connects to the access
             * point and receives an IP by DHCP.
             *
             * The assigned IP address is logged here, because the component
             * silences the logging of ``esp_netif_lwip``, which would normally
             * provide this message.
             */
            ESP_LOGD(TAG, "IP_EVENT_AP_STAIPASSIGNED");

            ip4_addr_t* tmp = (ip4_addr_t*)event_data;
            ESP_LOGI(TAG, "Station connected, " IPSTR " assigned", IP2STR(tmp));
            break;
        // case IP_EVENT_GOT_IP6:
        //     ESP_LOGV(TAG, "IP_EVENT_GOT_IP6");
        //     break;
        // case IP_EVENT_ETH_GOT_IP:
        //     ESP_LOGV(TAG, "IP_EVENT_ETH_GOT_IP");
        //     break;
        // case IP_EVENT_ETH_LOST_IP:
        //     ESP_LOGV(TAG, "IP_EVENT_ETH_LOST_IP");
        //     break;
        default:
            ESP_LOGD(TAG, "Got unhandled IP_EVENT: '%d'", event_id);
            break;
        }
    }
}

/**
 * Initialize the component.
 *
 * Initialization process includes initialization of **ESP-IDF**'s networking
 * stack (``esp_netif``), registering of ::mnet32_event_handler for
 * ``IP_EVENT`` occurences and allocating and initializing the internal ::state
 * variable.
 *
 * This function is also responsible for the creation of the component's
 * internal ``task``, which will handle all further actions of the component
 * (see ::mnet32_task).
 *
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure; see the
 *                      provided log messages (of level ``ERROR`` and ``DEBUG``)
 *                      for the actual reason of failure.
 */
static esp_err_t mnet32_init(void) {
    /* Adjust log settings!
     * The component's logging is based on **ESP-IDF**'s logging library,
     * meaning it will behave exactly like specified by the settings specified
     * in ``sdkconfig``/``menuconfig``.
     *
     * However, during development it may be desirable to activate a MAXIMUM log
     * level of VERBOSE, while keeping the DEFAULT log level to INFO. In this
     * case, the component's logging may be set to VERBOSE manually here!
     *
     * Additionally, **ESP-IDF**'s internal components, that are related to
     * networking, are silenced here.
     */
    // esp_log_level_set("mnet32", ESP_LOG_VERBOSE);
    // esp_log_level_set("mnet32.nvs", ESP_LOG_VERBOSE);
    // esp_log_level_set("mnet32.web", ESP_LOG_VERBOSE);
    // esp_log_level_set("mnet32.wifi", ESP_LOG_VERBOSE);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("wifi_init", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_ERROR);

    ESP_LOGV(TAG, "mnet32_init()");

    /* Check if this a recurrent call to the function. */
    if (mnet32_state_is_initialized()) {
        ESP_LOGE(TAG, "Internal state already initialized!");
        return ESP_FAIL;
    }

    esp_err_t esp_ret;

    /* Initialize the network stack.
     * This has to be done exactly one time.
     * While this is a mandatory requirement of this component, the actual
     * application may be functional without network access.
     */
    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize network stack!");
        ESP_LOGD(TAG,
                 "'esp_netif_init()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        return esp_ret;
    }

    /* Initialize internal state information */
    mnet32_state_init();

    /* Register IP_EVENT event handler.
     * These events are required for any *medium*, so the handler can already
     * be registered at this point. The handlers for medium-specific events
     * are registered in the respective module.
     */
    esp_ret = esp_event_handler_instance_register(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        mnet32_event_handler,
        NULL,
        (void**)mnet32_state_get_ip_event_handler_ptr());
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach IP_EVENT event handler!");
        ESP_LOGD(TAG,
                 "'esp_event_handler_instance_register()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        return esp_ret;
    }

    /* Create the actual dedicated task for the component. */
    if (xTaskCreate(mnet32_task,
                    "mnet32_task",
                    MNET32_TASK_STACK_SIZE,
                    NULL,
                    MNET32_TASK_PRIORITY,
                    mnet32_state_get_task_handle_ptr()) != pdPASS) {
        ESP_LOGE(TAG, "Could not create task!");
        return ESP_FAIL;
    }

    /* Place the first command for the dedicated mnet32_task. */
    mnet32_notify(MNET32_NOTIFICATION_CMD_WIFI_START);

    return ESP_OK;
}

/**
 * De-initialize the component.
 *
 * Basically this function destroys all of the component's setup, reversing
 * anything done by ::mnet32_init .
 *
 * This includes unregistering the ::mnet32_event_handler for ``IP_EVENT``
 * occurences, deleting the internal task, freeing memory and actually
 * deinitializing **ESP-IDF**'s networking stack (though this is - as of now -
 * a no-op).
 *
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` if there is no
 *                   internal ::state
 */
static esp_err_t mnet32_deinit(void) {
    ESP_LOGV(TAG, "mnet32_deinit()");

    if (!mnet32_state_is_initialized()) {
        ESP_LOGE(TAG, "No state information available!");
        return ESP_FAIL;
    }

    esp_err_t esp_ret;
    if (mnet32_state_is_medium_wireless())
        mnet32_wifi_deinit();

    /* Unregister the IP_EVENT event handler. */
    esp_ret = esp_event_handler_instance_unregister(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        mnet32_state_get_ip_event_handler());
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister IP_EVENT event handler!");
        ESP_LOGD(TAG,
                 "'esp_event_handler_instance_unregister()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    /* Stop and remove the dedicated networking task */
    if (mnet32_state_get_task_handle() != NULL)
        vTaskDelete(mnet32_state_get_task_handle());

    /* Free internal state memory. */
    mnet32_state_destroy();

    /* De-initialize the network stack.
     * This is actually not supported by **ESP-IDF**, but included here for
     * completeness and to be fully compatible, *if* **ESP-IDF** include this
     * in a future release.
     */
    esp_ret = esp_netif_deinit();
    if (esp_ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG,
                 "'esp_netif_deinit()' returned with an unexpected return "
                 "code: %s [%d]",  // NOLINT(whitespace/line_length)
                 esp_err_to_name(esp_ret),
                 esp_ret);
    }

    return ESP_OK;
}

/**
 * Emit component-specific events.
 *
 * The events will be queued in the default system event loop. If the event
 * loop is already filled, the function will **not block**, effectively losing
 * the event.
 *
 * @param event_id   The actual events are defined in mnet32.h and may be
 *                   referenced by their human-readable name in the ``enum``.
 * @param event_data Optional pointer to event-specific context data. This
 *                   might be set to ``NULL`` to emit events without contextual
 *                   data.
 *                   The calling function has to allocate (and free) the actual
 *                   memory for the context data. It may be free'd after this
 *                   function returned, because **ESP-IDF**'s event loop will
 *                   manage a copy of this data, once the event is posted to the
 *                   loop.
 */
static void mnet32_emit_event(int32_t event_id, void* event_data) {
    ESP_LOGV(TAG, "mnet32_emit_event()");

    esp_err_t esp_ret;

    if (event_data == NULL) {
        ESP_LOGV(TAG, "Event without context data!");
        esp_ret =
            esp_event_post(MNET32_EVENTS, event_id, NULL, 0, (TickType_t)0);
    } else {
        ESP_LOGV(TAG, "Event with context data!");
        esp_ret = esp_event_post(MNET32_EVENTS,
                                 event_id,
                                 event_data,
                                 sizeof(event_data),
                                 (TickType_t)0);
    }

    if (esp_ret != ESP_OK) {
        ESP_LOGW(TAG, "Could not emit event!");
        ESP_LOGD(TAG,
                 "esp_event_post() returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        ESP_LOGD(TAG, "event_base....... %s", MNET32_EVENTS);
        ESP_LOGD(TAG, "event_id......... %d", event_id);
        ESP_LOGD(TAG, "event_data....... %p", event_data);
        ESP_LOGD(TAG, "event_data_size.. %d", sizeof(event_data));
    }
}

void mnet32_notify(uint32_t notification) {
    ESP_LOGV(TAG, "mnet32_notify()");

    xTaskNotifyIndexed(mnet32_state_get_task_handle(),
                       MNET32_TASK_NOTIFICATION_INDEX,
                       notification,
                       eSetValueWithOverwrite);
}

esp_err_t mnet32_start(void) {
    ESP_LOGV(TAG, "mnet32_start()");

    if (mnet32_init() != ESP_OK) {
        mnet32_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t mnet32_stop(void) {
    ESP_LOGV(TAG, "mnet32_stop()");

    mnet32_notify(MNET32_NOTIFICATION_CMD_NETWORKING_STOP);

    return ESP_OK;
}
