// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
#define SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_

/**
 * This is the list of accepted notifications.
 */
typedef enum {
    NETWORKING_NOTIFICATION_BASE,
    NETWORKING_NOTIFICATION_CMD_WIFI_START,
} networking_notification;


/**
 * This method is used as interface to the dedicated networking task.
 *
 * @param notification
 * @todo Complete the documentation!
 */
void networking_notify(uint32_t notification);

#endif  // SRC_LIB_NETWORKING_SRC_NETWORKING_INTERNAL_H_
