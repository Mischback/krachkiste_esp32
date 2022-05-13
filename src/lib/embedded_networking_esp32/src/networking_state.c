// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Functions to manage the internal state of the ``networking`` component.
 *
 * @file   networking_state.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "networking_state.h"

/* This is ESP-IDF's event library. */
#include "esp_event.h"

/* ESP-IDF's network abstraction layer. */
#include "esp_netif.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/* ***** TYPES ************************************************************* */

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
 * Track the internal state of the component.
 */
static struct networking_state *state = NULL;


/* ***** FUNCTIONS ********************************************************* */

void networking_state_init(void) {
    state = calloc(1, sizeof(*state));
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
    state->mode = NETWORKING_MODE_NOT_APPLICABLE;
    state->status = NETWORKING_STATUS_DOWN;
}

void networking_state_destroy(void) {
    free(state);
    state = NULL;
}

void networking_state_medium_state_init(size_t size) {
    state->medium_state = calloc(1, size);
}

void networking_state_medium_state_destroy(void) {
    free(state->medium_state);
    state->medium_state = NULL;
}

bool networking_state_is_initialized(void) {
    return state != NULL;
}

bool networking_state_is_medium_state_initialized(void) {
    return state->medium_state != NULL;
}

bool networking_state_is_interface_set(void) {
    return state->interface != NULL;
}

bool networking_state_is_medium_wireless(void) {
    return state->medium == NETWORKING_MEDIUM_WIRELESS;
}
bool networking_state_is_mode_ap(void) {
    return state->mode == NETWORKING_MODE_WIFI_AP;
}

bool networking_state_is_mode_set(void) {
    return state->mode != NETWORKING_MODE_NOT_APPLICABLE;
}

bool networking_state_is_mode_sta(void) {
    return state->mode == NETWORKING_MODE_WIFI_STA;
}

bool networking_state_is_status_idle(void) {
    return state->status == NETWORKING_STATUS_IDLE;
}

esp_netif_t *networking_state_get_interface(void) {
    return state->interface;
}

esp_event_handler_t networking_state_get_ip_event_handler(void) {
    return state->ip_event_handler;
}

esp_event_handler_t *networking_state_get_ip_event_handler_ptr(void) {
    return &(state->ip_event_handler);
}

esp_event_handler_t networking_state_get_medium_event_handler(void) {
    return state->medium_event_handler;
}

esp_event_handler_t *networking_state_get_medium_event_handler_ptr(void) {
    return &(state->medium_event_handler);
}

void *networking_state_get_medium_state(void) {
    return state->medium_state;
}

uint8_t networking_state_get_mode(void) {
    return (uint8_t)state->mode;
}

TaskHandle_t networking_state_get_task_handle(void) {
    return state->task;
}

TaskHandle_t *networking_state_get_task_handle_ptr(void) {
    return &(state->task);
}

void networking_state_clear_interface(void) {
    state->interface = NULL;
}

void networking_state_set_interface(esp_netif_t *interface) {
    state->interface = interface;
}

void networking_state_clear_medium(void) {
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
}

void networking_state_set_medium_wireless(void) {
    state->medium = NETWORKING_MEDIUM_WIRELESS;
}

void networking_state_clear_mode(void) {
    state->mode = NETWORKING_MODE_NOT_APPLICABLE;
}

void networking_state_set_mode_ap(void) {
    state->mode = NETWORKING_MODE_WIFI_AP;
}

void networking_state_set_mode_sta(void) {
    state->mode = NETWORKING_MODE_WIFI_STA;
}

void networking_state_set_status_busy(void) {
    state->status = NETWORKING_STATUS_BUSY;
}

void networking_state_set_status_connecting(void) {
    state->status = NETWORKING_STATUS_CONNECTING;
}

void networking_state_set_status_idle(void) {
    state->status = NETWORKING_STATUS_IDLE;
}

void networking_state_set_status_ready(void) {
    state->status = NETWORKING_STATUS_READY;
}
