// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_NVS_H_

/* This is ESP-IDF's error handling library. */
#include "esp_err.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


esp_err_t mnet32_get_nvs_handle(nvs_open_mode_t mode, nvs_handle_t* handle);

esp_err_t mnet32_get_string_from_nvs(nvs_handle_t handle,
                                     const char* key,
                                     char* ret_buffer,
                                     const size_t max_buf_size);
