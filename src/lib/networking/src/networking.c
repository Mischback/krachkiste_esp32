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

    /* Initialize internal state information */
    state = calloc(1, sizeof(struct networking_state));
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
    state->mode = NETWORKING_MODE_NOT_APPLICABLE;
    state->status = NETWORKING_STATUS_DOWN;

    ESP_LOGI(TAG, "Successfully initialized networking component.");
    ESP_LOGD(TAG, "state->medium... %d", state->medium);
    ESP_LOGD(TAG, "state->mode..... %d", state->mode);
    ESP_LOGD(TAG, "state->status... %d", state->status);

    return ESP_OK;
}
