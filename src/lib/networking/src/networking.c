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

/* This file's header */
#include "networking/networking.h"

/* Other headers of the component. */
#include "networking_internal.h"

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

/* ESP-IDF's network abstraction layer */
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

/* ***** TYPES ************************************************************* */

/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";


/* ***** PROTOTYPES ******************************************************** */

static void networking(void *task_parameters);
static void networking_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);
static esp_err_t networking_deinit(void);
static esp_err_t networking_init(char* nvs_namespace);


/* ***** FUNCTIONS ********************************************************* */

static void networking(void *task_parameters) {
    ESP_LOGV(TAG, "networking()");
    ESP_LOGV(TAG, "This is the actual task function!");

    // TODO(mischback) Make this configurable by ``sdkconfig``!
    const TickType_t mon_freq = pdMS_TO_TICKS(5000);  // 5 sec
    BaseType_t notify_result;
    uint32_t notify_value = 0;

    ESP_LOGV(TAG, "networking(): Entering infinite loop...");
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
            ESP_LOGV(TAG, "WIFI_EVENT_STA_START");
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_STOP");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_DISCONNECTED");
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
            ESP_LOGV(TAG, "WIFI_EVENT_AP_START");
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STOP");
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STACONNECTED");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
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

static esp_err_t networking_init(char* nvs_namespace) {
    // Set log-level of our own code to VERBOSE
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "networking_init()");

    /* Check if this a recurrent call to the function. */
    if (networking_state != NULL) {
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
        ESP_LOGD(TAG, "'esp_netif_init()' returned %d", esp_ret);
        return ESP_FAIL;
    }

    /* Initialize internal state information */
    networking_state = calloc(1, sizeof(*networking_state));
    networking_state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
    networking_state->mode = NETWORKING_MODE_NOT_APPLICABLE;
    networking_state->status = NETWORKING_STATUS_DOWN;

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
        (void **)&(networking_state->ip_event_handler));
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach IP_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %d",
            esp_ret);
        return ESP_FAIL;
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
            NULL,
            NETWORKING_TASK_PRIORITY,
            &(networking_state->task)) != pdPASS) {
        ESP_LOGE(TAG, "Could not create task!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully initialized networking component.");
    ESP_LOGD(
        TAG,
        "networking_state->medium.......... %d",
        networking_state->medium);
    ESP_LOGD(
        TAG,
        "networking_state->mode............ %d",
        networking_state->mode);
    ESP_LOGD(
        TAG,
        "networking_state->status.......... %d",
        networking_state->status);
    ESP_LOGD(
        TAG,
        "networking_state->task............ %p",
        networking_state->task);
    ESP_LOGD(
        TAG,
        "networking_state->ip_event_handler %p",
        networking_state->ip_event_handler);

    /* Place the first command for the dedicated networking task. */
    networking_notify(NETWORKING_NOTIFICATION_CMD_WIFI_START);

    return ESP_OK;
}

static esp_err_t networking_deinit(void) {
    ESP_LOGV(TAG, "networking_deinit()");

    if (networking_state == NULL) {
        ESP_LOGE(TAG, "No state information available!");
        return ESP_FAIL;
    }

    /* Unregister the IP_EVENT event handler. */
    if (esp_event_handler_instance_unregister(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        networking_state->ip_event_handler) != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister IP_EVENT event handler!");
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    /* Stop and remove the dedicated networking task */
    if (networking_state->task != NULL)
        vTaskDelete(networking_state->task);

    /* Free internal state memory. */
    free(networking_state);
    networking_state = NULL;

    /* De-initialize the network stack.
     * This is actually not supported by **ESP-IDF**, but included here for
     * completeness and to be fully compatible, *if* **ESP-IDF** include this
     * in a future release.
     */
    if (esp_netif_deinit() != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(
            TAG,
            "'esp_netif_deinit' returned with an unexpected return code");
    }

    return ESP_OK;
}

void networking_notify(uint32_t notification) {
    ESP_LOGV(TAG, "networking_notify()");

    xTaskNotifyIndexed(
        networking_state->task,
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
