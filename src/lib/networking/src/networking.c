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

/* This is ESP-IDF's error handling library.
 * - defines the **type** ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
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

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* ***** DEFINES *********************************************************** */
/* ***** TYPES ************************************************************* */

/**
 * Specify the actual connection medium.
 *
 * This might be a wired connection (``NETWORKING_MEDIUM_ETHERNET``) or a
 * wireless connection (``NETWORKING_MEDIUM_WIRELESS``) when the component is
 * actually up and running.
 *
 * This is actually tracked in the component's ::state.
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
 */
typedef enum {
    NETWORKING_STATUS_DOWN,
    NETWORKING_STATUS_READY,
    NETWORKING_STATUS_UP,
} networking_status;

/**
 * A component-specific struct to keep track of the internal state.
 */
struct networking_state {
    networking_medium   medium;
    networking_mode     mode;
    networking_status   status;
    esp_netif_t         *interface;
    TaskHandle_t        task;
    esp_event_handler_t *ip_event_handler;
    esp_event_handler_t *wifi_event_handler;
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
/* ***** FUNCTIONS ********************************************************* */

static void networking(void *task_parameters) {
    ESP_LOGV(TAG, "networking()");
    ESP_LOGV(TAG, "This is the actual task function!");

    /* This should probably not be reached!
     * ``freeRTOS`` requires the task functions *to never return*. Instead,
     * the common idiom is to delete the very own task at the end of these
     * functions.
     */
    vTaskDelete(NULL);
}

esp_err_t networking_init(char* nvs_namespace) {
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
        ESP_LOGD(TAG, "'esp_netif_init()' returned %d", esp_ret);
        return ESP_FAIL;
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
    // TODO(mischback) Actually register the event handler as soon as it is
    //                 implemented!
    // esp_ret = esp_event_handler_instance_register();
    // if (esp_ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Could not attach IP_EVENT event handler!");
    //     ESP_LOGD(
    //         TAG,
    //         "'esp_event_handler_instance_register()' returned %d",
    //         esp_ret);
    //     return ESP_FAIL;
    // }

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
            state->task) != pdPASS) {
        ESP_LOGE(TAG, "Could not create task!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully initialized networking component.");
    ESP_LOGD(TAG, "state->medium... %d", state->medium);
    ESP_LOGD(TAG, "state->mode..... %d", state->mode);
    ESP_LOGD(TAG, "state->status... %d", state->status);

    return ESP_OK;
}

esp_err_t networking_deinit(void) {
    ESP_LOGV(TAG, "networking_deinit()");

    if (state == NULL) {
        ESP_LOGE(TAG, "No state information available!");
        return ESP_FAIL;
    }

    /* Unregister the IP_EVENT event handler. */
    // TODO(mischback) Actually unregister the event handler
    // if (esp_event_handler_instance_unregister(
    //     IP_EVENT,
    //     ESP_EVENT_ANY_ID,
    //     state->ip_event_handler) != ESP_OK) {
    //     ESP_LOGE(TAG, "Could not unregister IP_EVENT event handler!");
    //     ESP_LOGW(TAG, "Continuing with de-initialization...");
    // }

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
    if (esp_netif_deinit() != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(
            TAG,
            "'esp_netif_deinit' returned with an unexpected return code");
    }

    return ESP_OK;
}
