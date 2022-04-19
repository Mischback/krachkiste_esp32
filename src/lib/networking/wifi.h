// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_WIFI_H_
#define SRC_LIB_NETWORKING_WIFI_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 */
#include "esp_err.h"

/**
 * Initializes and starts an access point.
 *
 * This function is responsible for the whole setup process of the access
 * point, including its initial configuration, setting up the required network
 * interface (``netif``), registering the event handler
 * ::networking_wifi_ap_event_handler and preparing the required timer
 * to shutdown the access point after a given time.
 *
 * If the configured password ::NETWORKING_WIFI_AP_PSK is shorter than ``8``
 * characters, ``esp_wifi``'s internal functions will fail badly. Because of
 * this, this function will not apply the password but instead switch the
 * access point's ``authmode`` to ``WIFI_AUTH_OPEN``, effectivly providing an
 * open WiFi. A log message of level ``WARN`` will be emitted.
 *
 * The function will provide a log message of level ``INFO`` containing the
 * SSID and - if set - password/PSK of the access point's network.
 *
 * @return ``ESP_OK`` (= ``0``) on success, non-zero error code on error
 */
esp_err_t networking_wifi_ap_initialize(void);

#endif  // SRC_LIB_NETWORKING_WIFI_H_
