// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide the functions related to WiFi networking.
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *
 * @file   wifi.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header */
#include "wifi.h"

/* C standard libraries */
#include <string.h>

/* Other headers of the component. */
#include "networking_internal.h"

/* This is ESP-IDF's error handling library.
 * - defines the **type** ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
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

/* ESP-IDF's wifi library. */
#include "esp_wifi.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


/* ***** DEFINES *********************************************************** */

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 */
#define NETWORKING_WIFI_NVS_SSID "net_ssid"

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 */
#define NETWORKING_WIFI_NVS_PSK "net_psk"

/**
 * The maximum length of the ``char`` array to store SSID.
 *
 * IEEE 802.11 says, that the maximum length of an SSID is 32, which is also
 * the value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 32

/**
 * The maximum length of the ``char`` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE 801.11 says, that the maximum length of an PSK is 64, which is also the
 * value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 64


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";


/* ***** PROTOTYPES ******************************************************** */

static esp_err_t get_wifi_config_from_nvs(
    char *nvs_namespace,
    char *ssid,
    char *psk);
static esp_err_t wifi_init(char *nvs_namespace);
static esp_err_t wifi_deinit(void);
static esp_err_t wifi_ap_init(void);
static esp_err_t wifi_ap_deinit(void);
static esp_err_t wifi_sta_init(void);
static esp_err_t wifi_sta_deinit(void);

/* ***** FUNCTIONS ********************************************************* */

static esp_err_t get_wifi_config_from_nvs(
    char *nvs_namespace,
    char *ssid,
    char *psk) {
    ESP_LOGV(TAG, "get_wifi_config_from_nvs()");

    /* Open NVS storage handle. */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(nvs_namespace, NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        /* This might fail for different reasons, e.g. the NVS is not correctly
         * set up or initialized.
         * Assuming that the NVS **is** available, this will fail with
         * ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
         * the name ``nvs_namespace`` (yet).
         * This might happen during first start of the applications, as there
         * is no WiFi config yet, so the namespace was never used before.
         */
        ESP_LOGE(TAG, "Could not open NVS handle (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Handle '%s' successfully opened!", nvs_namespace);

    size_t req_size;
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_SSID, NULL, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for SSID!");
        return ESP_FAIL;
    }
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_SSID, ssid, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_SSID,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_PSK, NULL, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for PSK!");
        return ESP_FAIL;
    }
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_PSK, psk, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_PSK,
            esp_err_to_name(err));
        return ESP_FAIL;
    }


    return ESP_OK;
}

static esp_err_t wifi_init(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_init()");

    /* Initialization has only be performed once */
    if (networking_state_get_mode() != NETWORKING_MODE_NOT_APPLICABLE) {
        return ESP_OK;
    }

    esp_err_t esp_ret;

    /* Initialize the WiFi networking stack. */
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&init_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize WiFi!");
        ESP_LOGD(TAG, "'esp_wifi_init()' returned %d", esp_ret);
        return ESP_FAIL;
    }
    networking_state_set_medium(NETWORKING_MEDIUM_WIRELESS);

    /* Register the component's event handler for WIFI_EVENT. */

    /* Read WiFi configuration from non-volatile storage (NVS).
     * If the config can not be read, directly start in access point mode. If
     * there is a config, try station mode first.
     */
    char nvs_sta_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char nvs_sta_psk[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(nvs_sta_ssid, 0x00, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(nvs_sta_psk, 0x00, NETWORKING_WIFI_PSK_MAX_LEN);

    if (get_wifi_config_from_nvs(
        nvs_namespace,
        nvs_sta_ssid,
        nvs_sta_psk) != ESP_OK) {
        return wifi_ap_init();
    }

    return wifi_sta_init();
}

static esp_err_t wifi_deinit(void) {
    ESP_LOGV(TAG, "wifi_deinit()");

    esp_err_t esp_ret;

    esp_ret = esp_wifi_deinit();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinitialization of WiFi failed!");
        ESP_LOGD(TAG, "'esp_wifi_deinit() returned %d", esp_ret);
    }
    networking_state_set_medium(NETWORKING_MEDIUM_UNSPECIFIED);

    return ESP_OK;
}

static esp_err_t wifi_ap_init(void) {
    ESP_LOGV(TAG, "wifi_ap_init()");
    return ESP_OK;
}

static esp_err_t wifi_ap_deinit(void) {
    ESP_LOGV(TAG, "wifi_ap_deinit()");
    return ESP_OK;
}

static esp_err_t wifi_sta_init(void) {
    ESP_LOGV(TAG, "wifi_sta_init()");
    return ESP_OK;
}

static esp_err_t wifi_sta_deinit(void) {
    ESP_LOGV(TAG, "wifi_sta_deinit()");
    return ESP_OK;
}

esp_err_t wifi_start(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_start()");

    if (wifi_init(nvs_namespace) != ESP_OK) {
        wifi_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}
