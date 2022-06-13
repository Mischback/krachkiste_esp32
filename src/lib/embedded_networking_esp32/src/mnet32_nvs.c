// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide access to the non-volatile storage for the ``mnet32`` component.
 *
 * @file   mnet32_nvs.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header. */
#include "mnet32_nvs.h"

/* Other headers of the component. */
#include "mnet32/mnet32.h"  // The public header

/* This is ESP-IDF's error handling library. */
#include "esp_err.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "mnet32.nvs";


/* ***** FUNCTIONS ********************************************************* */

esp_err_t mnet32_get_nvs_handle(nvs_open_mode_t mode, nvs_handle_t* handle) {
    ESP_LOGV(TAG, "'mnet32_get_nvs_handle()'");

    esp_err_t esp_ret = nvs_open(MNET32_NVS_NAMESPACE, mode, handle);

    if (esp_ret != ESP_OK) {
        /* This might fail for different reasons, e.g. the NVS is not correctly
         * set up or initialized.
         * Assuming that the NVS **is** available, this will fail with
         * ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
         * the name ``MNET32_NVS_NAMESPACE`` (yet).
         * This might happen during first start of the applications, as there
         * is no WiFi config yet, so the namespace was never used before.
         */
        ESP_LOGE(TAG, "Could not open NVS handle '%s'!", MNET32_NVS_NAMESPACE);
        ESP_LOGD(TAG,
                 "'nvs_open()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

esp_err_t mnet32_get_string_from_nvs(nvs_handle_t handle,
                                     const char* key,
                                     char* ret_buffer,
                                     const size_t max_buf_size) {
    ESP_LOGV(TAG, "'mnet32_get_string_from_nvs()'");

    esp_err_t esp_ret;
    size_t req_size;

    esp_ret = nvs_get_str(handle, key, NULL, &req_size);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for %s!", key);
        ESP_LOGD(TAG,
                 "'nvs_get_str()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        return esp_ret;
    }

    if (req_size > max_buf_size) {
        ESP_LOGE(TAG, "Provided buffer has insufficient size!");
        ESP_LOGD(TAG, "Required: %d / available: %d", req_size, max_buf_size);
        return ESP_FAIL;
    }

    esp_ret = nvs_get_str(handle, key, ret_buffer, &req_size);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not read value of '%s'!", key);
        ESP_LOGD(TAG,
                 "'nvs_get_str()' returned %s [%d]",
                 esp_err_to_name(esp_ret),
                 esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

esp_err_t mnet32_nvs_write_string(nvs_handle_t handle,
                                  const char* key,
                                  const char* value) {
    ESP_LOGV(TAG, "mnet32_nvs_write_string()");

    return nvs_set_str(handle, key, value);
}
