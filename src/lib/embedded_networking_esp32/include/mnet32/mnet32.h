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
 */
#define MNET32_NVS_NAMESPACE CONFIG_MNET32_NVS_NAMESPACE

/**
 * The **freeRTOS**-specific priority for the component's task.
 *
 * @todo Determine a sane (default) value for this! Evaluate other (built-in)
 *       task priorities.
 */
#define MNET32_TASK_PRIORITY 10

/**
 * The component will automatically provide status information to other
 * components with this frequency.
 *
 * The value is given in milliseconds.
 */
#define MNET32_TASK_MONITOR_FREQUENCY CONFIG_MNET32_TASK_MONITOR_FREQUENCY

/**
 * The URI to serve the component's web interface from.
 */
#define MNET32_WEB_URL_CONFIG CONFIG_MNET32_WEB_URL_CONFIG

/**
 * The channel to be used while providing the project-specific access point.
 */
#define MNET32_WIFI_AP_CHANNEL CONFIG_MNET32_WIFI_AP_CHANNEL

/**
 * The maximum number of allowed clients while providing the project-specific
 * access point.
 */
#define MNET32_WIFI_AP_MAX_CONNS CONFIG_MNET32_WIFI_AP_MAX_CONNS

/**
 * Timespan to keep the project-specific access point available.
 *
 * The value is given in milliseconds.
 */
#define MNET32_WIFI_AP_LIFETIME CONFIG_MNET32_WIFI_AP_LIFETIME

/**
 * The password to access the project-specific access point.
 *
 * The component ``esp_wifi`` requires the password to be at least 8
 * characters! It fails badly otherwise. ::mnet32_wifi_ap_init does handle this.
 */
#define MNET32_WIFI_AP_PSK CONFIG_MNET32_WIFI_AP_PSK

/**
 * The actual SSID of the project-specific access point.
 */
#define MNET32_WIFI_AP_SSID CONFIG_MNET32_WIFI_AP_SSID

/**
 * The maximum number of connection attempts for station mode.
 *
 * After this number is reached, the component will launch the access point.
 */
#define MNET32_WIFI_STA_MAX_CONNECTION_ATTEMPTS CONFIG_MNET32_MAX_CON_ATTEMPTS

/**
 * Set a minimum required WiFi security while scanning for networks.
 *
 * This has to be a valid value of ``wifi_auth_mode_t``, as specified in
 * **ESP-IDF**'s ``esp_wifi_types.h``:
 *   - WIFI_AUTH_OPEN
 *   - WIFI_AUTH_WEP
 *   - WIFI_AUTH_WPA_PSK
 *   - WIFI_AUTH_WPA2_PSK
 *   - WIFI_AUTH_WPA_WPA2_PSK
 *   - WIFI_AUTH_WPA2_ENTERPRISE
 *   - WIFI_AUTH_WPA3_PSK
 *   - WIFI_AUTH_WPA2_WPA3_PSK
 *   - WIFI_AUTH_WAPI_PSK
 *   - WIFI_AUTH_MAX
 */
#define MNET32_WIFI_STA_THRESHOLD_AUTH WIFI_AUTH_WPA_PSK

/**
 * Set a minimum required WiFi signal strength while scanning for networks.
 *
 * Internally, **ESP-IDF** handle the RSSI as a *signed int8*, so ``-127`` is
 * the minimally available signal quality.
 */
#define MNET32_WIFI_STA_THRESHOLD_RSSI -127


/**
 * Declare the component-specific event base.
 */
ESP_EVENT_DECLARE_BASE(MNET32_EVENTS);

enum { MNET32_EVENT_UNAVAILABLE, MNET32_EVENT_READY };


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
 * @return esp_err_t ``ESP_OK`` if the network could be initialized,
 *                   ``ESP_FAIL`` in all other cases.
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
void mnet32_web_attach_handlers(void* arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void* event_data);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_INCLUDE_MNET32_MNET32_H_
