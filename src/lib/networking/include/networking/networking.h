// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
#define SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 */
#include "esp_err.h"

/* This is ESP-IDF's event library.
 * - defines ``esp_event_base_t``
 */
#include "esp_event.h"

/* ESP-IDF's wifi library
 * - defines constants used for NETWORKING_WIFI_STA_THRESHOLD_AUTH
 */
#include "esp_wifi.h"


/**
 * The channel to be used while providing the project-specific access point.
 *
 * @todo Is there a nice way to provide a **dynamic** channel?
 * @todo Make this configurable (pre-build with ``sdkconfig``)!
 */
#define NETWORKING_WIFI_AP_CHANNEL 5

/**
 * Timespan to keep the project-specific access point available.
 *
 * The value is specified in milliseconds, default value of ``120000`` are
 * *120 seconds*.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_LIFETIME 120000

/**
 * The maximum number of allowed clients while providing the project-specific
 * access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_MAX_CONNS 3

/**
 * The password to access the project-specific access point.
 *
 * The component ``esp_wifi`` requires the password to be at least 8
 * characters! It fails badly otherwise. ::networking_wifi_ap_initialize does
 * handle this.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_PSK "foobar"

/**
 * The actual SSID of the project-specific access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_AP_SSID "krachkiste_ap"

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_NVS_KEY_SSID "net_ssid"

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_NVS_KEY_PSK "net_pass"

/**
 * Set a maximum number of connection attempts for station mode.
 *
 * After this number of (failed) connection attempts, the component should
 * consider the specified WiFi network as *not available* and launch the local
 * access point (see ::sta_wifi_event_handler).
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_STA_MAX_CONNECTION_ATTEMPTS 3

/**
 * Set a minimum required WiFi security while scanning for networks.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Include a dedicated warning about security!
 */
#define NETWORKING_WIFI_STA_THRESHOLD_AUTH WIFI_AUTH_WPA_PSK

/**
 * Set a minimum required WiFi signal strength while scanning for networks.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 */
#define NETWORKING_WIFI_STA_THRESHOLD_RSSI -127


esp_err_t networking_initialize(char* nvs_namespace);

/**
 * Handle the event, that the http server is ready to accept further
 * *URI handlers*.
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
 *                   event-related data. This handler assumes, that the
 *                   provided ``arg`` is an actual ``http_handle_t*`` to the
 *                   http server instance.
 */
void networking_web_attach_handlers(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);

#endif  // SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
