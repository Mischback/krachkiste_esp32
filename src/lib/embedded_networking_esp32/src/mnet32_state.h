// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_STATE_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_STATE_H_

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


void mnet32_state_init(void);
void mnet32_state_destroy(void);
void mnet32_state_medium_state_init(size_t size);
void mnet32_state_medium_state_destroy(void);

bool mnet32_state_is_initialized(void);
bool mnet32_state_is_medium_state_initialized(void);

bool mnet32_state_is_interface_set(void);
bool mnet32_state_is_medium_wireless(void);
bool mnet32_state_is_mode_ap(void);
bool mnet32_state_is_mode_set(void);
bool mnet32_state_is_mode_sta(void);
bool mnet32_state_is_status_idle(void);

esp_netif_t *mnet32_state_get_interface(void);
esp_event_handler_t mnet32_state_get_ip_event_handler(void);
esp_event_handler_t *mnet32_state_get_ip_event_handler_ptr(void);
esp_event_handler_t mnet32_state_get_medium_event_handler(void);
esp_event_handler_t *mnet32_state_get_medium_event_handler_ptr(void);
void *mnet32_state_get_medium_state(void);
uint8_t mnet32_state_get_mode(void);
TaskHandle_t mnet32_state_get_task_handle(void);
TaskHandle_t *networking_state_get_task_handle_ptr(void);

void mnet32_state_clear_interface(void);
void mnet32_state_set_interface(esp_netif_t *interface);
void mnet32_state_clear_medium(void);
void mnet32_state_set_medium_wireless(void);
void mnet32_state_clear_mode(void);
void mnet32_state_set_mode_ap(void);
void mnet32_state_set_mode_sta(void);
void mnet32_state_set_status_busy(void);
void mnet32_state_set_status_connecting(void);
void mnet32_state_set_status_idle(void);
void mnet32_state_set_status_ready(void);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_STATE_H_
