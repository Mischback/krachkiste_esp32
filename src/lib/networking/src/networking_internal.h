// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
#define SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_

/* This is ESP-IDF's event library. */
#include "esp_event.h"

/* ESP-IDF's network abstraction layer */
#include "esp_netif.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * This is the list of accepted notifications.
 */
typedef enum {
    NETWORKING_NOTIFICATION_BASE,
    NETWORKING_NOTIFICATION_CMD_WIFI_START,
} networking_notification;

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
    esp_event_handler_t ip_event_handler;
    esp_event_handler_t wifi_event_handler;
};


/**
 * This method is used as interface to the dedicated networking task.
 *
 * @param notification
 * @todo Complete the documentation!
 */
void networking_notify(uint32_t notification);

/**
 * Get the medium of the component's internal state.
 *
 * @return networking_medium
 * @todo Complete the documentation!
 */
networking_medium networking_state_get_medium(void);

/**
 * Set the medium of the component's internal state.
 *
 * @param medium
 * @todo Complete the documentation!
 */
void networking_state_set_medium(networking_medium medium);

/**
 * Get the mode of the component's internal state.
 *
 * @return networking_mode
 * @todo Complete the documentation!
 */
networking_mode networking_state_get_mode(void);

/**
 * Set the mode of the component's internal state.
 *
 * @param mode
 * @todo Complete the documentation!
 */
void networking_state_set_mode(networking_mode mode);

/**
 * Get the status of the component's internal state.
 *
 * @return networking_status
 * @todo Complete the documentation!
 */
networking_status networking_state_get_status(void);

/**
 * Set the status of the component's internal state.
 *
 * @param status
 * @todo Complete the documentation!
 */
void networking_state_set_status(networking_status status);

#endif  // SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
