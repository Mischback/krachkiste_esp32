// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_
#define SRC_LIB_NETWORKING_INCLUDE_NETWORKING_NETWORKING_H_

/* This is ESP-IDF's event library.
 * - defines ``esp_event_base_t``
 */
#include "esp_event.h"


/**
 * The project-specific namespace to access the non-volatile storage.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``), consider
 *       ``ESP-IDF``'s ``NVS_KEY_NAME_SIZE``
 *       (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#_CPPv48nvs_openPKc15nvs_open_mode_tP12nvs_handle_t)
 */
#define PROJECT_NVS_STORAGE_NAMESPACE "krachkiste"

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


void networking_initialize(void);

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
