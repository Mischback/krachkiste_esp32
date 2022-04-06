// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_NETWORKING_H_
#define SRC_LIB_NETWORKING_NETWORKING_H_

/**
 * The project-specific namespace to access the non-volatile storage.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``), consider
 *       ``ESP-IDF``'s ``NVS_KEY_NAME_SIZE``
 *       (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#_CPPv48nvs_openPKc15nvs_open_mode_tP12nvs_handle_t)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define PROJECT_NVS_STORAGE_NAMESPACE "krachkiste"

/**
 * The channel to be used while providing the project-specific access point.
 *
 * @todo Is there a nice way to provide a **dynamic** channel?
 * @todo Make this configurable (pre-build with ``sdkconfig``)!
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_AP_CHANNEL 5

/**
 * Timespan to keep the project-specific access point available.
 *
 * The value is specified in milliseconds, default value of ``120000`` are
 * *120 seconds*.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_AP_LIFETIME 120000

/**
 * The maximum number of allowed clients while providing the project-specific
 * access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
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
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_AP_PSK "foobar"

/**
 * The actual SSID of the project-specific access point.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_AP_SSID "krachkiste_ap"

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_NVS_KEY_SSID "net_ssid"

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to header file, because configuration options must be
 *       considered as part of the public API
 */
#define NETWORKING_WIFI_NVS_KEY_PSK "net_pass"


void networking_initialize(void);

#endif  // SRC_LIB_NETWORKING_NETWORKING_H_
