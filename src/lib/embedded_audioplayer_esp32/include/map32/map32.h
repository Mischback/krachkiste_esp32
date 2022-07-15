// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_AUDIOPLAYER_ESP32_INCLUDE_MAP32_MAP32_H_
#define SRC_LIB_EMBEDDED_AUDIOPLAYER_ESP32_INCLUDE_MAP32_MAP32_H_

/* This is ESP-IDF's error handling library.
 * - defines ``esp_err_t``
 * - provided by ESP-IDF's component ``esp_common``
 */
#include "esp_err.h"

// TODO(mischback) Actually document this function, as soon as the prototype is
//                 stable.
// TODO(mischback) Include this function in ``docs/source/api/map32.rst``
esp_err_t map32_start(void);

#endif  // SRC_LIB_EMBEDDED_AUDIOPLAYER_ESP32_INCLUDE_MAP32_MAP32_H_
