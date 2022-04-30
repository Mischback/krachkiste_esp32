// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_WIFI_H_
#define SRC_LIB_NETWORKING_SRC_WIFI_H_

/* This is ESP-IDF's error handling library.
 * - defines the **type** ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
 */
#include "esp_err.h"


esp_err_t wifi_start(void);

#endif  // SRC_LIB_NETWORKING_SRC_WIFI_H_
