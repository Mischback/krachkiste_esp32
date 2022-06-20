// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_

/**
 * This is the list of accepted notifications.
 */
typedef enum {
    MNET32_NOTIFICATION_BASE,
    MNET32_NOTIFICATION_CMD_NETWORKING_STOP,
    MNET32_NOTIFICATION_CMD_WIFI_START,
    MNET32_NOTIFICATION_CMD_WIFI_RESTART,
    MNET32_NOTIFICATION_EVENT_WIFI_AP_START,
    MNET32_NOTIFICATION_EVENT_WIFI_AP_STACONNECTED,
    MNET32_NOTIFICATION_EVENT_WIFI_AP_STADISCONNECTED,
    MNET32_NOTIFICATION_EVENT_WIFI_STA_START,
    MNET32_NOTIFICATION_EVENT_WIFI_STA_CONNECTED,
    MNET32_NOTIFICATION_EVENT_WIFI_STA_DISCONNECTED,
} mnet32_task_notification;


/**
 * Handle ``IP_EVENT`` and ``WIFI_EVENT`` occurences.
 *
 * @param arg
 * @param event_base The base of the actual event. The handler *can handle*
 *                   occurences of ``IP_EVENT`` and ``WIFI_EVENT``.
 * @param event_id   The actual event, as specified by its ``base`` and its
 *                   ``id``.
 * @param event_data
 *
 * @todo Complete this documentation block!
 */
void mnet32_event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data);

/**
 * Send a notification to the component's internal ``task``.
 *
 * Technically, this sends a notification to the task as specified by
 * ::mnet32_task , unblocking the task and triggering some action.
 *
 * The function sends a notification on the index specified by
 * ``MNET32_TASK_NOTIFICATION_INDEX`` with ``eAction`` set to
 * ``eSetValueWithOverwrite``, meaning: if the task was already notified, that
 * notification will be overwritten.
 *
 * @param notification
 */
void mnet32_notify(uint32_t notification);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_
