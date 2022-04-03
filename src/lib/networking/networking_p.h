// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_NETWORKING_P_H_
#define SRC_LIB_NETWORKING_NETWORKING_P_H_

/* ESP-IDF's error handling library.
 * Defines the type ``esp_err_t``, which is used in this component's prototypes.
 */
#include "esp_err.h"

static esp_err_t networking_get_wifi_credentials(
    char* wifi_ssid,
    char* wifi_password);

static void wifi_scan_for_networks(void);

#endif  // SRC_LIB_NETWORKING_NETWORKING_P_H_
