// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_NETWORKING_WIFI_H_
#define SRC_LIB_NETWORKING_SRC_NETWORKING_WIFI_H_

/**
 * Get the number of connected stations in access point mode.
 *
 * @return int8_t The number of connected stations or ``-1`` in case of error.
 */
int8_t wifi_ap_get_connected_stations(void);

/**
 * Initialize the WiFi for access point mode.
 *
 * Basically this creates the access point-specific configuration, applies it
 * to **ESP-IDF**'s wifi module and starts the wifi.
 *
 * It sets ``state->mode`` to ``NETWORKING_MODE_WIFI_AP`` and provides a
 * reference to the ``netif`` in ``state->interface``.
 *
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 *                   On ``ESP_OK`` the calling code may assume that the access
 *                   point is successfully started. Subsequent actions are
 *                   triggered by ::networking_event_handler and performed by
 *                   ::networking .
 */
esp_err_t wifi_ap_init(void);

/**
 * Start the access point's shutdown timer.
 *
 * The timer is created in ::wifi_ap_init and should be available here. There
 * is a basic check of the presence and a log message of level ``WARNING`` is
 * emitted, if the timer (``TimerHandle_t``) is not available.
 */
void wifi_ap_timer_start(void);

/**
 * Stop the access point's shutdown timer.
 *
 * The timer is created in ::wifi_ap_init and should be available here. There
 * is a basic check of the presence and a log message of level ``WARNING`` is
 * emitted, if the timer (``TimerHandle_t``) is not available.
 */
void wifi_ap_timer_stop(void);

/**
 * WiFi-specific deinitialization.
 *
 * Clean up for WiFi-related stuff, including deinitialization of the
 * ``netif``, unregistering ::networking_event_handler from ``WIFI_EVENT``
 * and resetting ``state->medium`` to ``NETWORKING_MEDIUM_UNSPECIFIED``.
 *
 * The function calls ::wifi_sta_deinit or ::wifi_ap_deinit, depending on the
 * value of ``state->mode``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
esp_err_t wifi_deinit(void);

/**
 * Actually connect to a WiFi access point.
 *
 * This function is called from ::networking to perform the actual connect.
 *
 * Please note: This function simply wraps **ESP-IDF**'s ``esp_wifi_connect()``
 * to catch internal errors. It does not actually track the success of the
 * connection. This has to be done while handling the events raised by
 * **ESP-IDF**'s ``wifi`` module.
 */
void wifi_sta_connect(void);

/**
 * Deinitialize WiFi (station mode).
 *
 * Disconnects from the WiFi and destroys the ``netif``.
 *
 * This function is meant to be called by ::wifi_deinit and performs only the
 * specific deinitialization steps of *station mode*, e.g. it does **not**
 * unregister ::networking_event_handler from ``WIFI_EVENT``.
 *
 * The function sets ``state->mode`` to ``NETWORKING_MODE_NOT_APPLICABLE`` and
 * ``state->interface`` to ``NULL``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
esp_err_t wifi_sta_deinit(void);

/**
 * Return the number of failed connection attempts.
 *
 * @return int8_t The number of failed connection attempts.
 */
int8_t wifi_sta_get_num_connection_attempts(void);

/**
 * Reset the number of failed connection attempts.
 */
void wifi_sta_reset_connection_counter(void);

/**
 * Starts a WiFi connection.
 *
 * This either connects to a given WiFi network, if credentials are available
 * in the given ``nvs_namespace`` and the specified network is reachable, or
 * launches the local access point.
 *
 * @param nvs_namespace The NVS namespace to read values from.
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 */
esp_err_t wifi_start(char *nvs_namespace);

#endif  // SRC_LIB_NETWORKING_SRC_NETWORKING_WIFI_H_
