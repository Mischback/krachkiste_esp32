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
 * This method is used as interface to the dedicated networking task.
 *
 * @param notification
 * @todo Complete the documentation!
 */
void networking_notify(uint32_t notification);

#endif  // SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
