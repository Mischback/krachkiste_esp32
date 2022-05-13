// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Functions to manage the internal state of the ``mnet32`` component.
 *
 * @file   mnet32_state.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "mnet32_state.h"

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
 * This might be a wired connection (``MNET32_MEDIUM_ETHERNET``) or a
 * wireless connection (``MNET32_MEDIUM_WIRELESS``) when the component is
 * actually up and running.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    MNET32_MEDIUM_UNSPECIFIED,
    MNET32_MEDIUM_ETHERNET,
    MNET32_MEDIUM_WIRELESS,
} mnet32_medium;

/**
 * Specify the mode of the wireless connection.
 *
 * This is only applicable for ``MNET32_MEDIUM_WIRELESS`` and will be set
 * to ``MNET32_MODE_NOT_APPLICABLE`` on initialization or if the medium
 * is set to ``MNET32_MEDIUM_ETHERNET``.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    MNET32_MODE_NOT_APPLICABLE,
    MNET32_MODE_WIFI_AP,
    MNET32_MODE_WIFI_STA,
} mnet32_mode;

/**
 * Specify the actual status of the connection.
 *
 * The connection status must be evaluated in the context of its ``medium`` -
 * and in case of a wireless connection - its ``mode``.
 *
 * This is tracked in the component's ::state.
 */
typedef enum {
    MNET32_STATUS_DOWN,
    MNET32_STATUS_READY,
    MNET32_STATUS_CONNECTING,
    MNET32_STATUS_IDLE,
    MNET32_STATUS_BUSY,
} mnet32_status;

/**
 * A component-specific struct to keep track of the internal state.
 *
 * It contains several fields that track the component's internal state aswell
 * as fields to keep track of the actual *interface*
 * (``esp_netif_t *interface``), the dedicated component's *task*
 * (``TaskHandle_t task``) and the required event handler instances.
 */
struct mnet32_state {
    mnet32_medium       medium;
    mnet32_mode         mode;
    mnet32_status       status;
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
static struct mnet32_state *state = NULL;


/* ***** FUNCTIONS ********************************************************* */

void mnet32_state_init(void) {
    state = calloc(1, sizeof(*state));
    state->medium = MNET32_MEDIUM_UNSPECIFIED;
    state->mode = MNET32_MODE_NOT_APPLICABLE;
    state->status = MNET32_STATUS_DOWN;
}

void mnet32_state_destroy(void) {
    free(state);
    state = NULL;
}

void mnet32_state_medium_state_init(size_t size) {
    state->medium_state = calloc(1, size);
}

void mnet32_state_medium_state_destroy(void) {
    free(state->medium_state);
    state->medium_state = NULL;
}

bool mnet32_state_is_initialized(void) {
    return state != NULL;
}

bool mnet32_state_is_medium_state_initialized(void) {
    return state->medium_state != NULL;
}

bool mnet32_state_is_interface_set(void) {
    return state->interface != NULL;
}

bool mnet32_state_is_medium_wireless(void) {
    return state->medium == MNET32_MEDIUM_WIRELESS;
}
bool mnet32_state_is_mode_ap(void) {
    return state->mode == MNET32_MODE_WIFI_AP;
}

bool mnet32_state_is_mode_set(void) {
    return state->mode != MNET32_MODE_NOT_APPLICABLE;
}

bool mnet32_state_is_mode_sta(void) {
    return state->mode == MNET32_MODE_WIFI_STA;
}

bool mnet32_state_is_status_idle(void) {
    return state->status == MNET32_STATUS_IDLE;
}

esp_netif_t *mnet32_state_get_interface(void) {
    return state->interface;
}

esp_event_handler_t mnet32_state_get_ip_event_handler(void) {
    return state->ip_event_handler;
}

esp_event_handler_t *mnet32_state_get_ip_event_handler_ptr(void) {
    return &(state->ip_event_handler);
}

esp_event_handler_t mnet32_state_get_medium_event_handler(void) {
    return state->medium_event_handler;
}

esp_event_handler_t *mnet32_state_get_medium_event_handler_ptr(void) {
    return &(state->medium_event_handler);
}

void *mnet32_state_get_medium_state(void) {
    return state->medium_state;
}

uint8_t mnet32_state_get_mode(void) {
    return (uint8_t)state->mode;
}

TaskHandle_t mnet32_state_get_task_handle(void) {
    return state->task;
}

TaskHandle_t *networking_state_get_task_handle_ptr(void) {
    return &(state->task);
}

void mnet32_state_clear_interface(void) {
    state->interface = NULL;
}

void mnet32_state_set_interface(esp_netif_t *interface) {
    state->interface = interface;
}

void mnet32_state_clear_medium(void) {
    state->medium = MNET32_MEDIUM_UNSPECIFIED;
}

void mnet32_state_set_medium_wireless(void) {
    state->medium = MNET32_MEDIUM_WIRELESS;
}

void mnet32_state_clear_mode(void) {
    state->mode = MNET32_MODE_NOT_APPLICABLE;
}

void mnet32_state_set_mode_ap(void) {
    state->mode = MNET32_MODE_WIFI_AP;
}

void mnet32_state_set_mode_sta(void) {
    state->mode = MNET32_MODE_WIFI_STA;
}

void mnet32_state_set_status_busy(void) {
    state->status = MNET32_STATUS_BUSY;
}

void mnet32_state_set_status_connecting(void) {
    state->status = MNET32_STATUS_CONNECTING;
}

void mnet32_state_set_status_idle(void) {
    state->status = MNET32_STATUS_IDLE;
}

void mnet32_state_set_status_ready(void) {
    state->status = MNET32_STATUS_READY;
}
