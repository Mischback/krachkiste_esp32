// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
#define SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 */
#include "esp_err.h"

/**
 * The **freeRTOS**-specific priority for the component's task.
 *
 * @todo Determine a sane (default) value for this! Evaluate other (built-in)
 *       task priorities.
 * @todo Should this be make configurable by ``sdkconfig``?
 */
#define NETWORKING_TASK_PRIORITY 10

/**
 * Initialize the ``networking`` component to establish and maintain network
 * connectivity.
 *
 * @param nvs_namespace
 * @return esp_err_t
 * @todo Complete the documentation!
 */
esp_err_t networking_init(char* nvs_namespace);

/**
 * Deinitialize the ``networking`` component.
 *
 * @return esp_err_t
 * @todo Complete the documentation!
 */
esp_err_t networking_deinit(void);

#endif  // SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
