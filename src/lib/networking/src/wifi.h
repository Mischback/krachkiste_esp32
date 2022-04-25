// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_WIFI_H_
#define SRC_LIB_NETWORKING_SRC_WIFI_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 */
#include "esp_err.h"


#define NETWORKING_WIFI_RET_OK   ESP_OK  // Rely on ESP-IDF's general ``OK``
#define NETWORKING_WIFI_RET_FAIL ESP_FAIL  // Rely on ESP-IDF's general ``FAIL``
#define NETWORKING_WIFI_RET_ALREADY_STARTED 0xf001
#define NETWORKING_WIFI_RET_INIT_FAILED     0xf002
#define NETWORKING_WIFI_RET_DEINIT_FAILED   0xf003


/**
 * Initializes the WiFi connectivity of the device.
 *
 * The function will take care of reading and existing WiFi configuration from
 * the non-volatile storage, establishing and maintaining connection if the s
 * pecified WiFi network is available or launching an internal access point to
 * make the configuration web interface available.
 *
 * @param nvs_namespace Namespace to be used to retrieve an existing Wifi
 *                      configuration. Must be specified by the calling code
 *                      (it is assumed, that this component is used by some
 *                      actual application, which manages the NVS namespaces of
 *                      the project).
 * @return              ``ESP_OK`` (= ``0``) on success, ``ESP_FAIL`` (= ``-1``)
 *                      otherwise.
 */
esp_err_t wifi_initialize(char* nvs_namespace);

esp_err_t wifi_start(char* nvs_namespace);

esp_err_t wifi_stop(void);

#endif  // SRC_LIB_NETWORKING_SRC_WIFI_H_
