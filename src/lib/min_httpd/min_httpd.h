// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide a minimal webserver for the project.
 *
 * @file   min_httpd.h
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

#ifndef SRC_LIB_MIN_HTTPD_MIN_HTTPD_H_
#define SRC_LIB_MIN_HTTPD_MIN_HTTPD_H_

/* This is ESP-IDF's event library.
 * - defines ``esp_event_base_t``
 */
#include "esp_event.h"

/**
 * Handle external events and trigger the required actions.
 *
 * This event handler is meant to deal with events, that are not directly
 * associated with this component, e.g. regarding the availability of network
 * connections, thus, *external events*.
 *
 * @param arg
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   `esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data.
 */
void httpd_external_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);

#endif  // SRC_LIB_MIN_HTTPD_MIN_HTTPD_H_
