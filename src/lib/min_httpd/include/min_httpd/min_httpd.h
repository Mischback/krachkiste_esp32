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

#ifndef SRC_LIB_MIN_HTTPD_INCLUDE_MIN_HTTPD_MIN_HTTPD_H_
#define SRC_LIB_MIN_HTTPD_INCLUDE_MIN_HTTPD_MIN_HTTPD_H_

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 */
#include "esp_err.h"

/* This is ESP-IDF's event library.
 * - defines ``esp_event_base_t``
 */
#include "esp_event.h"

/* This is ESP-IDF's http server library.
 * - defines ``httpd_req_t``
 */
#include "esp_http_server.h"


/**
 * The port the server will listen.
 *
 * ``80`` is actually the default value as provided by **ESP-IDF**'s
 * ``HTTPD_DEFAULT_CONFIG()``. But the value may be overridden with a
 * project-specific value.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MIN_HTTPD_HTTP_PORT 80

/**
 * Maximum number of URI handlers for the server.
 *
 * ``8`` is actually the default value as provided by **ESP-IDF**'s
 * ``HTTPD_DEFAULT_CONFIG()``, but it may (and probably must) be overridden
 * with a project-specific value.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MIN_HTTPD_MAX_URI_HANDLERS 8

/**
 * Component-specific event base.
 */
ESP_EVENT_DECLARE_BASE(MIN_HTTPD_EVENTS);

/**
 * Component-specific events.
 *
 * MIN_HTTPD_READY - Emitted at the end of server's startup routine to indicate
 *                   to other components that the server is ready to accept
 *                   further *URI handlers*.
 */
enum { MIN_HTTPD_READY };


/**
 * Handle external events that should cause the HTTP server to start.
 *
 * This is a specific handler, that does not actually parse or verify the
 * event, that triggered its execution.
 *
 * This requires the *calling code*, or more specifically, the code that
 * connects this handler with events, to specifically select the events, that
 * should *make the HTTP server* **start**.
 *
 * @param arg        Generic arguments.
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   `esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data.
 */
void min_httpd_external_event_handler_start(void* arg,
                                            esp_event_base_t event_base,
                                            int32_t event_id,
                                            void* event_data);

/**
 * Handle external events that should cause the HTTP server to stop.
 *
 * This is a specific handler, that does not actually parse or verify the
 * event, that triggered its execution.
 *
 * This requires the *calling code*, or more specifically, the code that
 * connects this handler with events, to specifically select the events, that
 * should *make the HTTP server* **stop**.
 *
 * @param arg        Generic arguments.
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   `esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data.
 */
void min_httpd_external_event_handler_stop(void* arg,
                                           esp_event_base_t event_base,
                                           int32_t event_id,
                                           void* event_data);

/**
 * Provide a simple log message.
 *
 * Logs the request's *method* and the requested *URI*, using log level
 * ``INFO``.
 *
 * ``success`` indicates, if the ``request`` was processed successfully and
 * this status is also included in the provided log message.
 *
 * @param request A pointer to the request, that should be logged.
 * @param success This flag indicates, if the request could be served
 *                successfully.
 */
void min_httpd_log_message(httpd_req_t* request, esp_err_t success);

#endif  // SRC_LIB_MIN_HTTPD_INCLUDE_MIN_HTTPD_MIN_HTTPD_H_
