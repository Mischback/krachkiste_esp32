// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide basic management of the networking capabilities of the MCU.
 *
 * This file is the actual implementation of the component. For a detailed
 * description of the actual usage, including how the component may be reused
 * in some other codebase, refer to networking.h .
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *
 * @file   networking.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This file's header */
#include "networking/networking.h"

/* C-standard for string operations */
#include <string.h>

/* Other headers of the component */
#include "wifi.h"

/* The FreeRTOS headers are required for timers */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ``ESP_ERROR_CHECK``
 * - defines the return values ``ESP_OK`` (0) and ``ESP_FAIL`` (-1)
 */
#include "esp_err.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ESP-IDF's network abstraction layer */
#include "esp_netif.h"

/* ESP-IDF's wifi library */
#include "esp_wifi.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "krachkiste.networking";


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t networking_get_wifi_credentials(
    char* nvs_namespace,
    char* wifi_ssid,
    char* wifi_password);


/* ***** FUNCTIONS ********************************************************* */

/**
 * Retrieve credentials for a WiFi network from the NVS.
 *
 * The function opens the non-volatile storage namespace (as specified by
 * ::PROJECT_NVS_STORAGE_NAMESPACE) and retrieves the WiFi's ``SSID`` from
 * ::NETWORKING_WIFI_NVS_KEY_SSID and the ``PSK`` from
 * ::NETWORKING_WIFI_NVS_KEY_PSK.
 *
 * @param wifi_ssid     A pointer to a ``char`` array to store the ``SSID``.
 *                      The ``char`` array has to be defined outside of this
 *                      function before calling. It is recommended to declare
 *                      that ``char`` array with length
 *                      ::NETWORKING_WIFI_SSID_MAX_LEN.
 * @param wifi_password A pointer to a ``char`` array to store the ``PSK``. The
 *                      ``char`` array has to be defined outside of this
 *                      function before calling. It is recommended to declare
 *                      that ``char`` array with length
 *                      ::NETWORKING_WIFI_PSK_MAX_LEN.
 * @return              ``ESP_OK`` (equals ``0``) on success, ``ESP_FAIL``
 *                      (equals ``-1``) on failure. Every failed access to the
 *                      NVS will directly return a failure, meaning that the
 *                      credentials could not be retrieved. Every fail will
 *                      also cause emitting of a log message of level ``ERROR``.
 */
static esp_err_t networking_get_wifi_credentials(
    char* nvs_namespace,
    char* wifi_ssid,
    char* wifi_password) {
    ESP_LOGV(TAG, "Entering networking_get_wifi_credentials()");

    // Open NVS storage of the given namespace
    nvs_handle_t storage_handle;
    esp_err_t err = nvs_open(
        nvs_namespace,
        NVS_READONLY,
        &storage_handle);
    if (err != ESP_OK) {
        // This might fail for different reasons, e.g. the NVS is not correctly
        // set up or initialized.
        // Assuming that the NVS **is** available, this will fail with
        // ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
        // the name PROJECT_NVS_STORAGE_NAMESPACE (yet).
        // This might happen during first start of the applications, as there
        // is no WiFi config yet, so the namespace was never used before.
        ESP_LOGE(TAG, "Could not open NVS handle (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Got a handle to the NVS, no read the required values!
    ESP_LOGD(
        TAG,
        "NVS handle %s successfully opened.",
        nvs_namespace);

    // Read the SSID and password from NVS
    size_t required_size;
    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_NVS_KEY_SSID,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_NVS_KEY_SSID,
        wifi_ssid,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_SSID,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(
        nvs_get_str(
            storage_handle,
            NETWORKING_WIFI_NVS_KEY_PSK,
            NULL,
            &required_size));
    err = nvs_get_str(
        storage_handle,
        NETWORKING_WIFI_NVS_KEY_PSK,
        wifi_password,
        &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_KEY_PSK,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Close the handle to the NVS
    nvs_close(storage_handle);

    return ESP_OK;
}

esp_err_t networking_initialize(char* nvs_namespace) {
    // set log-level of our own code to VERBOSE (sdkconfig.defaults sets the
    // default log-level to INFO)
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ESP_LOGV(TAG, "Entering networking_initialize()");

    // Initialize the underlying TCP/IP stack
    // This should be done exactly once from application code, so this function
    // seems like a good enough place, as it is the starting point for all
    // networking-related code.
    ESP_ERROR_CHECK(esp_netif_init());

    // esp_err_t err = networking_get_wifi_credentials(
    //     nvs_namespace,
    //     wifi_ssid,
    //     wifi_password);
    // if (err != ESP_OK) {
    //     ESP_LOGI(TAG, "Could not read WiFi credentials from NVS");
    //     ESP_LOGD(TAG, "Trying to start access point now!");

    //     ESP_ERROR_CHECK(networking_wifi_ap_initialize());
    // } else {
    //     ESP_LOGD(TAG, "SSID: >%s<", wifi_ssid);
    //     ESP_LOGD(TAG, "Password: >%s<", wifi_password);
    // }

    return wifi_initialize(nvs_namespace);
}
