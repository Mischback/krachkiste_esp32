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

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 */
#include "freertos/FreeRTOS.h"

/**
 * The **freeRTOS**-specific priority for the player's control task.
 *
 * This is part of the component's configuration, but can only be adjusted by
 * modifying the actual header file ``map32.h``.
 *
 * @todo Determine a sane (default) value for this! Evaluate other (built-in)
 *       task priorities.
 */
#define MAP32_CTRL_TASK_PRIORITY 10


/**
 * The player can handle the following commands.
 *
 * Please note that the player may perform different actions on the actual
 * commands, depending on its internal state. But the command names are pretty
 * much self-explanatory and should describe the expected action well enough.
 */
typedef enum {
    MAP32_CMD_START,
    MAP32_CMD_PLAY,
    MAP32_CMD_PAUSE,
    MAP32_CMD_STOP,
    MAP32_CMD_PREV_SOURCE,
    MAP32_CMD_NEXT_SOURCE,
    MAP32_CMD_PREV_TRACK,
    MAP32_CMD_NEXT_TRACK,
    MAP32_CMD_VOLUP,
    MAP32_CMD_VOLDOWN,
} map32_command;

// TODO(mischback) Actually document this function, as soon as the prototype is
//                 stable.
BaseType_t map32_ctrl_command(map32_command cmd);

// TODO(mischback) Actually document this function, as soon as the prototype is
//                 stable.
// TODO(mischback) Include this function in ``docs/source/api/map32.rst``
esp_err_t map32_start(void);

// TODO(mischback) Actually document this function
// TODO(mischback) Include this function in ``docs/source/api/map32.rst``
esp_err_t map32_stop(void);

#endif  // SRC_LIB_EMBEDDED_AUDIOPLAYER_ESP32_INCLUDE_MAP32_MAP32_H_
