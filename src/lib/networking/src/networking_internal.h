// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
#define SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_

/**
 * The index to be used by ``xTaskNotifyWaitIndex``.
 *
 * **freeRTOS** does support tasks with with multiple *notifications*, however,
 * **ESP-IDF** limits this to just one, which is placed at index ``0`` (multiple
 * notification values are supported starting at 10.4.0).
 *
 * The component however uses the generic versions of ``xTaskNotifyWait()`` and
 * ``xTaskNotify()``, so this constant ensures, that the notifications will be
 * send to the right *receiver*.
 */
#define NETWORKING_NOTIFICATION_TASK_INDEX 0

#define NETWORKING_NOTIFICATION_COMMAND_DESTROY         0x01
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT       0x02
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_DEINIT     0x03
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_INIT_RETRY 0x04
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_STA_INIT   0x05
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_STA_DEINIT 0x06
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_AP_INIT    0x07
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_AP_DEINIT  0x08
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_START      0x09
#define NETWORKING_NOTIFICATION_COMMAND_WIFI_STOP       0x0A

#endif  // SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
