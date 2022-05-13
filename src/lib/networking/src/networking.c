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
 *   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html
 *
 * @file   networking.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "networking/networking.h"

/* C's standard libraries. */
#include <string.h>

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
 * event handler function (::networking_event_handler), thus, the specific
 * WiFi-related events must be known.
 */
#include "esp_wifi.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


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
#define NETWORKING_TASK_NOTIFICATION_INDEX 0

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 */
#define NETWORKING_WIFI_NVS_SSID "net_ssid"

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 */
#define NETWORKING_WIFI_NVS_PSK "net_psk"


/* ***** TYPES ************************************************************* */

/* Define the component-specific event base. */
ESP_EVENT_DEFINE_BASE(NETWORKING_EVENTS);

/**
 * This is the list of accepted notifications.
 */
typedef enum {
    NETWORKING_NOTIFICATION_BASE,
    NETWORKING_NOTIFICATION_CMD_NETWORKING_STOP,
    NETWORKING_NOTIFICATION_CMD_WIFI_START,
    NETWORKING_NOTIFICATION_EVENT_WIFI_AP_START,
    NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED,
    NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED,
    NETWORKING_NOTIFICATION_EVENT_WIFI_STA_START,
    NETWORKING_NOTIFICATION_EVENT_WIFI_STA_CONNECTED,
    NETWORKING_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED,
} networking_notification;

/**
 * Specify the actual connection medium.
 *
 * This might be a wired connection (``NETWORKING_MEDIUM_ETHERNET``) or a
 * wireless connection (``NETWORKING_MEDIUM_WIRELESS``) when the component is
 * actually up and running.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    NETWORKING_MEDIUM_UNSPECIFIED,
    NETWORKING_MEDIUM_ETHERNET,
    NETWORKING_MEDIUM_WIRELESS,
} networking_medium;

/**
 * Specify the mode of the wireless connection.
 *
 * This is only applicable for ``NETWORKING_MEDIUM_WIRELESS`` and will be set
 * to ``NETWORKING_MODE_NOT_APPLICABLE`` on initialization or if the medium
 * is set to ``NETWORKING_MEDIUM_ETHERNET``.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    NETWORKING_MODE_NOT_APPLICABLE,
    NETWORKING_MODE_WIFI_AP,
    NETWORKING_MODE_WIFI_STA,
} networking_mode;

/**
 * Specify the actual status of the connection.
 *
 * The connection status must be evaluated in the context of its ``medium`` -
 * and in case of a wireless connection - its ``mode``.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    NETWORKING_STATUS_DOWN,
    NETWORKING_STATUS_READY,
    NETWORKING_STATUS_CONNECTING,
    NETWORKING_STATUS_IDLE,
    NETWORKING_STATUS_BUSY,
} networking_status;

/**
 * A component-specific struct to keep track of the internal state.
 *
 * It contains several fields that track the component's internal state aswell
 * as fields to keep track of the actual *interface*
 * (``esp_netif_t *interface``), the dedicated networking *task*
 * (``TaskHandle_t task``) and the required event handler instances.
 */
struct networking_state {
    networking_medium   medium;
    networking_mode     mode;
    networking_status   status;
    esp_netif_t         *interface;
    TaskHandle_t        task;
    esp_event_handler_t ip_event_handler;
    esp_event_handler_t medium_event_handler;
    void                *medium_state;
};


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";

/**
 * Track the internal state of the component.
 */
static struct networking_state *state = NULL;


/* ***** PROTOTYPES ******************************************************** */

static esp_err_t get_nvs_handle(
    const char *namespace,
    nvs_open_mode_t mode,
    nvs_handle_t *handle);
static esp_err_t get_string_from_nvs(
    nvs_handle_t handle,
    const char *key,
    char *ret_buffer,
    const size_t max_buf_size);

static void networking(void *task_parameters);
static void networking_notify(uint32_t notification);
static void networking_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);
static esp_err_t networking_deinit(void);
static esp_err_t networking_init(char* nvs_namespace);
static void networking_emit_event(int32_t event_id, void *event_data);


/* ***** FUNCTIONS ********************************************************* */

static void networking(void *task_parameters) {
    ESP_LOGV(TAG, "networking() [the actual task function]");
    ESP_LOGD(TAG, "task_parameters: %s", (char *)task_parameters);

    const TickType_t mon_freq = pdMS_TO_TICKS(
        NETWORKING_TASK_MONITOR_FREQUENCY);
    BaseType_t notify_result;
    uint32_t notify_value = 0;

    for (;;) {
        /* Block until notification or ``mon_freq`` reached. */
        notify_result = xTaskNotifyWaitIndexed(
            NETWORKING_TASK_NOTIFICATION_INDEX,
            pdFALSE,
            ULONG_MAX,
            &notify_value,
            mon_freq);

        /* Notification or monitoring? */
        if (notify_result == pdPASS) {
            switch (notify_value) {
            case NETWORKING_NOTIFICATION_CMD_NETWORKING_STOP:
                ESP_LOGD(TAG, "CMD: NETWORKING_STOP");

                /* Emit the corresponding event *before* actually shutting down
                 * the networking. This might give other components some time
                 * to handle the unavailability of networking.
                 */
                networking_emit_event(NETWORKING_EVENT_UNAVAILABLE, NULL);
                networking_deinit();
                break;
            case NETWORKING_NOTIFICATION_CMD_WIFI_START:
                ESP_LOGD(TAG, "CMD: WIFI_START");

                if (wifi_start((char *)task_parameters) != ESP_OK) {
                    ESP_LOGE(TAG, "Could not start WiFi!");
                }
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_AP_START:
                /* Handle ``WIFI_EVENT_AP_START`` (received from
                 * ::networking_event_handler ).
                 * The *chain* of ::wifi_start, ::wifi_init and ::wifi_ap_init
                 * has set ``state->medium`` and ``state->mode``, so with this
                 * event the access point is assumed to be ready, resulting in
                 * ``state->status = NETWORKING_STATUS_IDLE``, because no
                 * clients have connected yet.
                 */
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_START");

                state->status = NETWORKING_STATUS_IDLE;
                wifi_ap_timer_start();

                networking_emit_event(NETWORKING_EVENT_READY, NULL);
                // TODO(mischback) Should the *status event* be emitted here
                //                 automatically?

                // TODO(mischback) Determine which of access point-specific
                //                 information should be included in the
                //                 component's *status* event (e.g. AP SSID?)!
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED:
                /* A client connected to the access point.
                 * Set the internal state to NETWORKING_STATUS_BUSY to indicate
                 * actual usage of the access point. The internal timer to
                 * shut down the access point has to be stopped, because a
                 * client *might be* consuming the web interface, so the
                 * access point has to be kept running.
                 */
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_STACONNECTED");

                state->status = NETWORKING_STATUS_BUSY;
                wifi_ap_timer_stop();

                // TODO(mischback) Determine which of the access point-specific
                //                 information should be included in the
                //                 components *status* event (e.g. number of
                //                 connected clients?)
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_AP_STADISCONNECTED");

                if (wifi_ap_get_connected_stations() == 0) {
                    state->status = NETWORKING_STATUS_IDLE;

                    ESP_LOGD(
                        TAG,
                        "No more stations connected, restarting shutdown timer!");  // NOLINT(whitespace/line_length)
                    wifi_ap_timer_start();
                }

                // TODO(mischback) Determine which of the access point-specific
                //                 information should be included in the
                //                 components *status* event (e.g. number of
                //                 connected clients?)
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_STA_START:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_START");

                state->status = NETWORKING_STATUS_CONNECTING;
                wifi_sta_connect();
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_STA_CONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_CONNECTED");

                state->status = NETWORKING_STATUS_READY;
                wifi_sta_reset_connection_counter();
                networking_emit_event(NETWORKING_EVENT_READY, NULL);
                // TODO(mischback) Should the *status event* be emitted here
                //                 automatically?
                break;
            case NETWORKING_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED:
                ESP_LOGD(TAG, "EVENT: WIFI_EVENT_STA_DISCONNECTED");

                if (wifi_sta_get_num_connection_attempts()
                    > NETWORKING_WIFI_STA_MAX_CONNECTION_ATTEMPTS) {
                    wifi_sta_deinit();
                    if (wifi_ap_init() != ESP_OK)
                        networking_notify(
                            NETWORKING_NOTIFICATION_CMD_NETWORKING_STOP);
                } else {
                    wifi_sta_connect();
                }
                break;
            default:
                ESP_LOGW(TAG, "Got unhandled notification: %d", notify_value);
                break;
            }
        } else {
            // TODO(mischback) The component should kind of *publish* relevant
            //                 informations of its internal state using
            //                 **EPS-IDF**'s event system. This will be the
            //                 place to actually emit the *event*.
            ESP_LOGV(TAG, "'mon_freq' reached...");
        }
    }

    /* This should probably not be reached!
     * ``freeRTOS`` requires the task functions *to never return*. Instead,
     * the common idiom is to delete the very own task at the end of these
     * functions.
     */
    vTaskDelete(NULL);
}

/**
 * Handle ``IP_EVENT`` and ``WIFI_EVENT`` occurences.
 *
 * @param arg
 * @param event_base The base of the actual event. The handler *can handle*
 *                   occurences of ``IP_EVENT`` and ``WIFI_EVENT``.
 * @param event_id   The actual event, as specified by its ``base`` and its
 *                   ``id``.
 * @param event_data
 *
 * @todo Complete this documentation block!
 * @todo The current implementation defines ``case`` statements for all events,
 *       that *might* happen. Probably many of them are not acutally required
 *       for the component to work. All ``case`` statements do provide a log
 *       message of level ``VERBOSE``.
 *       a) Can this code be kept without impacting the build size? Or in other
 *          words: Will these statements be *optimized away* if the minimum
 *          log level is greater than ``VERBOSE``?
 *       b) For actual *events in use* by the component, switch to log level
 *          ``DEBUG``.
 * @todo When ``ethernet`` networking is implemented, the related events must
 *       be included here!
 */
static void networking_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "networking_event_handler()");

    if (event_base == WIFI_EVENT) {
        /* All WIFI_EVENT event_ids
         * See [ESP-IDF documentation link]
         */
        switch (event_id) {
        case WIFI_EVENT_WIFI_READY:
            ESP_LOGV(TAG, "WIFI_EVENT_WIFI_READY");
            break;
        case WIFI_EVENT_SCAN_DONE:
            ESP_LOGV(TAG, "WIFI_EVENT_SCAN_DONE");
            break;
        case WIFI_EVENT_STA_START:
            /* This event is emitted by ``esp_wifi`` when the interface is
             * successfully started in station mode.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
            networking_notify(NETWORKING_NOTIFICATION_EVENT_WIFI_STA_START);
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_STOP");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGD(TAG, "WIFI_EVENT_STA_CONNECTED");
            networking_notify(NETWORKING_NOTIFICATION_EVENT_WIFI_STA_CONNECTED);
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
            networking_notify(
                NETWORKING_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED);
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGED");
            break;
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_SUCCESS");
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_FAILED");
            break;
        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_TIMEOUT");
            break;
        case WIFI_EVENT_STA_WPS_ER_PIN:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_PIN");
            break;
        case WIFI_EVENT_AP_START:
            /* This event is emitted by ``esp_wifi`` when the access point is
             * successfully started.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_START");
            networking_notify(NETWORKING_NOTIFICATION_EVENT_WIFI_AP_START);
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STOP");
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            /* This event is emitted by ``esp_wifi`` when a client connects to
             * the access point.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STACONNECTED");
            networking_notify(
                NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            /* This event is emitted by ``esp_wifi`` when a client disconnects
             * from the access point.
             */
            ESP_LOGD(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            networking_notify(
                NETWORKING_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED);
            break;
        case WIFI_EVENT_AP_PROBEREQRECVED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
            break;
        default:
            ESP_LOGW(TAG, "Got unhandled WIFI_EVENT: '%d'", event_id);
            break;
        }
    }

    if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGV(TAG, "IP_EVENT_STA_GOT_IP");
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGV(TAG, "IP_EVENT_STA_LOST_IP");
            break;
        case IP_EVENT_AP_STAIPASSIGNED:
            /* This event is emitted whenever a client connects to the access
             * point and receives an IP by DHCP.
             */
            // TODO(mischback) As of now, this event is not used, instead all
            //                 of the component's actions are taken in the
            //                 (corresponding) WIFI_EVENT_AP_STACONNECTED and,
            //                 more relevant, WIFI_EVENT_AP_STADISCONNECTED.
            //                 LONG STORY SHORT: This case may be removed!
            ESP_LOGV(TAG, "IP_EVENT_AP_STAIPASSIGNED");
            break;
        case IP_EVENT_GOT_IP6:
            ESP_LOGV(TAG, "IP_EVENT_GOT_IP6");
            break;
        case IP_EVENT_ETH_GOT_IP:
            ESP_LOGV(TAG, "IP_EVENT_ETH_GOT_IP");
            break;
        case IP_EVENT_ETH_LOST_IP:
            ESP_LOGV(TAG, "IP_EVENT_ETH_LOST_IP");
            break;
        default:
            ESP_LOGW(TAG, "Got unhandled IP_EVENT: '%d'", event_id);
            break;
        }
    }
}

/**
 * Initialize the component.
 *
 * Initialization process includes initialization of **ESP-IDF**'s networking
 * stack (``esp_netif``), registering of ::networking_event_handler for
 * ``IP_EVENT`` occurences and allocating and initializing the internal ::state
 * variable.
 *
 * This function is also responsible for the creation of the component's
 * internal ``task``, which will handle all further actions of the component
 * (see ::networking).
 *
 * @param nvs_namespace The name of the namespace to be used to retrieve
 *                      non-volatile configuration options from.
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure; see the
 *                      provided log messages (of level ``ERROR`` and ``DEBUG``)
 *                      for the actual reason of failure.
 */
static esp_err_t networking_init(char* nvs_namespace) {
    // Set log-level of our own code to VERBOSE
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "networking_init()");

    /* Check if this a recurrent call to the function. */
    if (state != NULL) {
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
        ESP_LOGD(
            TAG,
            "'esp_netif_init()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    /* Initialize internal state information */
    state = calloc(1, sizeof(*state));
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
    state->mode = NETWORKING_MODE_NOT_APPLICABLE;
    state->status = NETWORKING_STATUS_DOWN;

    /* Register IP_EVENT event handler.
     * These events are required for any *medium*, so the handler can already
     * be registered at this point. The handlers for medium-specific events
     * are registered in the respective module.
     */
    esp_ret = esp_event_handler_instance_register(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        networking_event_handler,
        NULL,
        (void **)&(state->ip_event_handler));
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach IP_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    /* Create the actual dedicated task for the component. */
    // TODO(mischback) Determine a usable ``usStackDepth``!
    //                 a) In **ESP-IDF** the ``usStackDepth`` is not based on
    //                    the actual stack width, but ``StackType_t`` is
    //                    ``uint_8`` (one byte).
    //                 b) Verify configMINIMAL_STACK_SIZE (``freeRTOS.h``)!
    //                 c) 4096 byte = 4KB. This seems quite high. Before this
    //                    component may be considered *completed*, this value
    //                    should be minimized (see freeRTOS'
    //                    ``uxTaskGetStackHighWaterMark()``).
    if (xTaskCreate(
            networking,
            "networking",
            4096,
            nvs_namespace,
            NETWORKING_TASK_PRIORITY,
            &(state->task)) != pdPASS) {
        ESP_LOGE(TAG, "Could not create task!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully initialized networking component.");
    ESP_LOGD(TAG, "state->medium.............. %d", state->medium);
    ESP_LOGD(TAG, "state->mode................ %d", state->mode);
    ESP_LOGD(TAG, "state->status.............. %d", state->status);
    ESP_LOGD(TAG, "state->task................ %p", state->task);
    ESP_LOGD(TAG, "state->ip_event_handler.... %p", state->ip_event_handler);
    ESP_LOGD(TAG, "state->medium_event_handler.. %p", state->medium_event_handler);  // NOLINT(whitespace/line_length)

    /* Place the first command for the dedicated networking task. */
    networking_notify(NETWORKING_NOTIFICATION_CMD_WIFI_START);

    return ESP_OK;
}

/**
 * De-initialize the component.
 *
 * Basically this function destroys all of the component's setup, reversing
 * anything done by ::networking_init .
 *
 * This includes unregistering the ::networking_event_handler for ``IP_EVENT``
 * occurences, deleting the internal task, freeing memory and actually
 * deinitializing **ESP-IDF**'s networking stack (though this is - as of now -
 * a no-op).
 *
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` if there is no
 *                   internal ::state
 */
static esp_err_t networking_deinit(void) {
    ESP_LOGV(TAG, "networking_deinit()");

    if (state == NULL) {
        ESP_LOGE(TAG, "No state information available!");
        return ESP_FAIL;
    }

    esp_err_t esp_ret;
    if (state->medium == NETWORKING_MEDIUM_WIRELESS)
        esp_ret = wifi_deinit();

    /* Unregister the IP_EVENT event handler. */
    esp_ret = esp_event_handler_instance_unregister(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        state->ip_event_handler);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister IP_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_unregister()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    /* Stop and remove the dedicated networking task */
    if (state->task != NULL)
        vTaskDelete(state->task);

    /* Free internal state memory. */
    free(state);
    state = NULL;

    /* De-initialize the network stack.
     * This is actually not supported by **ESP-IDF**, but included here for
     * completeness and to be fully compatible, *if* **ESP-IDF** include this
     * in a future release.
     */
    esp_ret = esp_netif_deinit();
    if (esp_ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(
            TAG,
            "'esp_netif_deinit()' returned with an unexpected return code: %s [%d]",  // NOLINT(whitespace/line_length)
            esp_err_to_name(esp_ret),
            esp_ret);
    }

    return ESP_OK;
}

/**
 * Emit component-specific events.
 *
 * @param event_id   The actual events are defined in networking.h and may be
 *                   referenced by their human-readable name in the ``enum``.
 * @param event_data Optional pointer to event-specific context data. This
 *                   might be set to ``NULL`` to emit events without contextual
 *                   data.
 *                   The calling function has to allocate (and free) the actual
 *                   memory for the context data. It may be free'd after this
 *                   function returned, because **ESP-IDF**'s event loop will
 *                   manage a copy of this data, once the event is posted to the
 *                   loop.
 *
 * @todo Should ``time_to_wait`` be configurable? Are there events that **MUST**
 *       be place in the event loop while having other events that may be
 *       discarded?
 */
static void networking_emit_event(int32_t event_id, void *event_data) {
    ESP_LOGV(TAG, "networking_emit_event()");

    esp_err_t esp_ret;

    if (event_data == NULL) {
        ESP_LOGV(TAG, "Event without context data!");
        esp_ret = esp_event_post(
            NETWORKING_EVENTS,
            event_id,
            NULL,
            0,
            (TickType_t) 0);
    } else {
        ESP_LOGV(TAG, "Event with context data!");
        esp_ret = esp_event_post(
            NETWORKING_EVENTS,
            event_id,
            event_data,
            sizeof(event_data),
            (TickType_t) 0);
    }

    if (esp_ret != ESP_OK) {
        ESP_LOGW(TAG, "Could not emit event!");
        ESP_LOGD(
            TAG,
            "esp_event_post() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGD(TAG, "event_base....... %s", NETWORKING_EVENTS);
        ESP_LOGD(TAG, "event_id......... %d", event_id);
        ESP_LOGD(TAG, "event_data....... %p", event_data);
        ESP_LOGD(TAG, "event_data_size.. %d", sizeof(event_data));
    }
}

/**
 * Send a notification to the component's internal ``task``.
 *
 * Technically, this sends a notification to the task as specified by
 * ::networking , unblocking the task and triggering some action.
 *
 * The function sends a notification on the index specified by
 * ``NETWORKING_TASK_NOTIFICATION_INDEX`` with ``eAction`` set to
 * ``eSetValueWithOverwrite``, meaning: if the task was already notified, that
 * notification will be overwritten.
 *
 * @param notification
 */
static void networking_notify(uint32_t notification) {
    ESP_LOGV(TAG, "networking_notify()");

    xTaskNotifyIndexed(
        state->task,
        NETWORKING_TASK_NOTIFICATION_INDEX,
        notification,
        eSetValueWithOverwrite);
}

esp_err_t networking_start(char* nvs_namespace) {
    ESP_LOGV(TAG, "networking_start()");

    if (networking_init(nvs_namespace) != ESP_OK) {
        networking_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t networking_stop(void) {
    ESP_LOGV(TAG, "networking_stop()");

    networking_notify(NETWORKING_NOTIFICATION_CMD_NETWORKING_STOP);

    return ESP_OK;
}

static esp_err_t get_nvs_handle(
    const char *namespace,
    nvs_open_mode_t mode,
    nvs_handle_t *handle) {
    ESP_LOGV(TAG, "'get_nvs_handle()'");

    esp_err_t esp_ret = nvs_open(namespace, mode, handle);

    if (esp_ret != ESP_OK) {
        /* This might fail for different reasons, e.g. the NVS is not correctly
         * set up or initialized.
         * Assuming that the NVS **is** available, this will fail with
         * ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
         * the name ``nvs_namespace`` (yet).
         * This might happen during first start of the applications, as there
         * is no WiFi config yet, so the namespace was never used before.
         */
        ESP_LOGE(TAG, "Could not open NVS handle '%s'!", namespace);
        ESP_LOGD(
            TAG,
            "'nvs_open()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

static esp_err_t get_string_from_nvs(
    nvs_handle_t handle,
    const char *key,
    char *ret_buffer,
    const size_t max_buf_size) {
    ESP_LOGV(TAG, "'get_string_from_nvs()'");

    esp_err_t esp_ret;
    size_t req_size;

    esp_ret = nvs_get_str(handle, key, NULL, &req_size);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for %s!", key);
        ESP_LOGD(
            TAG,
            "'nvs_get_str()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    if (req_size > max_buf_size) {
        ESP_LOGE(TAG, "Provided buffer has insufficient size!");
        ESP_LOGD(
            TAG,
            "Required: %d / available: %d",
            req_size,
            max_buf_size);
        return ESP_FAIL;
    }

    esp_ret = nvs_get_str(handle, key, ret_buffer, &req_size);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(
            TAG, "Could not read value of '%s'!", key);
        ESP_LOGD(
            TAG,
            "'nvs_get_str()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}
