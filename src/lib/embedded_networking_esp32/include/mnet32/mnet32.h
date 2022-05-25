// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_INCLUDE_MNET32_MNET32_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_INCLUDE_MNET32_MNET32_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 */
#include "esp_err.h"

/* This is ESP-IDF's event library. */
#include "esp_event.h"

/* ESP-IDF's wifi library
 * - defines constants used for MNET32_WIFI_STA_THRESHOLD_AUTH
 */
#include "esp_wifi.h"


/**
 * The namespace to store component-specific values in the non-volatile storage.
 *
 * @todo Should this be make configurable by ``sdkconfig``?
 */
#define MNET32_NVS_NAMESPACE "mnet32"

/**
 * The **freeRTOS**-specific priority for the component's task.
 *
 * @todo Determine a sane (default) value for this! Evaluate other (built-in)
 *       task priorities.
 * @todo Should this be make configurable by ``sdkconfig``?
 */
#define MNET32_TASK_PRIORITY 10

/**
 * The component will automatically provide status information to other
 * components with this frequency.
 *
 * @todo Determine a sane (default) value for this! Currently: 5 seconds.
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_TASK_MONITOR_FREQUENCY 5000

/**
 * The URI to serve the component's web interface from.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WEB_URL_CONFIG "/config/wifi"

/**
 * The channel to be used while providing the project-specific access point.
 *
 * @todo Is there a nice way to provide a **dynamic** channel?
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_AP_CHANNEL 5

/**
 * The maximum number of allowed clients while providing the project-specific
 * access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_AP_MAX_CONNS 3

/**
 * Timespan to keep the project-specific access point available.
 *
 * The value is specified in milliseconds, default value of ``60000`` are
 * *60 seconds*.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_AP_LIFETIME 60000

/**
 * The password to access the project-specific access point.
 *
 * The component ``esp_wifi`` requires the password to be at least 8
 * characters! It fails badly otherwise. ::mnet32_wifi_ap_init does handle this.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_AP_PSK "foobar"

/**
 * The actual SSID of the project-specific access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_AP_SSID "krachkiste_ap"

/**
 * The maximum number of connection attempts for station mode.
 *
 * After this number is reached, the component will launch the access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_STA_MAX_CONNECTION_ATTEMPTS 3

/**
 * Set a minimum required WiFi security while scanning for networks.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Include a dedicated warning about security!
 */
#define MNET32_WIFI_STA_THRESHOLD_AUTH WIFI_AUTH_WPA_PSK

/**
 * Set a minimum required WiFi signal strength while scanning for networks.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define MNET32_WIFI_STA_THRESHOLD_RSSI -127


/**
 * Declare the component-specific event base.
 *
 */
ESP_EVENT_DECLARE_BASE(MNET32_EVENTS);

enum {
    MNET32_EVENT_UNAVAILABLE,
    MNET32_EVENT_READY
};


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
 * @return esp_err_t
 * @todo Complete the documentation!
 */
esp_err_t mnet32_start(void);

/**
 * Stop all networking and free resources.
 *
 * @return esp_err_t Always returns ``ESP_OK``.
 */
esp_err_t mnet32_stop(void);

/**
 * Handle the event, that the http server is ready to accept further
 * *URI handlers*.
 *
 * This is a specific handler, that does not actually parse or verify the
 * event, that triggered its execution.
 *
 * This requires the *calling code*, or more specifically, the code that
 * connects this handler with events, to specifically select the events, that
 * should make this component register its specific *URI handlers*.
 *
 * @param arg        Generic arguments.
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   `esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data. This handler assumes, that the
 *                   provided ``arg`` is an actual ``http_handle_t*`` to the
 *                   http server instance.
 */
void mnet32_web_attach_handlers(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_INCLUDE_MNET32_MNET32_H_
