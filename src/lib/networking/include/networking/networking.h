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
 * The component's entry point.
 *
 * This function is meant to be called from the actual application and will take
 * care of setting up the network, depending on configuration values.
 *
 * Internally, the component will launch a dedicated task, which is meant to
 * establish and maintain network connectivity or provide a fallback, providing
 * an access point to keep the configuration web interface accessible.
 *
 * @param nvs_namespace
 * @return esp_err_t
 * @todo Complete the documentation!
 */
esp_err_t networking_start(char* nvs_namespace);

#endif  // SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
