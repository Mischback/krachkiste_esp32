// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_

/* This is ESP-IDF's error handling library. */
#include "esp_err.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


/**
 * Get a handle to the component's non-volatile storage.
 *
 * Opens a handle to ::MNET32_NVS_NAMESPACE of the given ``mode`` and stores
 * the actual handle in ``handle``.
 *
 * Please note that the calling function will have to allocate memory for
 * ``handle``.
 *
 * @param mode   The mode to open the handle in.
 * @param handle The actual handle.
 * @return esp_err_t ``ESP_OK`` if the handle could be successfully opened.
 */
esp_err_t mnet32_nvs_get_handle(nvs_open_mode_t mode, nvs_handle_t* handle);

/**
 * Get a string value from the non-volatile storage as specified by ``key``.
 *
 * The ``handle`` must be established before calling this funcion (see
 * ::mnet32_nvs_get_handle ). The memory to store the retrieved value must be
 * allocated before calling this function.
 *
 * @param handle       The handle to use.
 * @param key          The key to retrieve.
 * @param ret_buffer   A pointer to the allocated memory to store the value.
 * @param max_buf_size Maximum length of the value to be retrieved. This should
 *                     match the allocated memory for the value.
 * @return esp_err_t   ``ESP_OK`` if the value could be retrieved, some
 *                     NVS-specific error code otherwise.
 */
esp_err_t mnet32_nvs_get_string(nvs_handle_t handle,
                                const char* key,
                                char* ret_buffer,
                                const size_t max_buf_size);

/**
 * Write a given ``value`` to the non-volatile storage.
 *
 * The NVS must be opened before calling this function to retrieve a handle,
 * see ::mnet32_nvs_get_handle .
 *
 * @param handle The handle to be used, see ::mnet32_get_nbs_handle.
 * @param key    The key to store the value to.
 * @param value  The actual value to store.
 * @return esp_err_t
 */
esp_err_t mnet32_nvs_write_string(nvs_handle_t handle,
                                  const char* key,
                                  const char* value);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_
