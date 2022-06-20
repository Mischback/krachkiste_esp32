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
 * Main purpose of this event handler is to make the system's events, as
 * provided by **ESP-IDF**'s modules/components, available to this component.
 *
 * Basically it *translates* the events to actions by using ::mnet32_notify and
 *  that are to be taken by ::mnet32_task and are executed within this
 * component's specific task/thread.
 *
 * @param arg        Arguments to be passed to the event handler. This is a
 *                   ``void *`` to the arguments, the event handler is
 *                   responsible for making any sense of this.
 *                   As of now, the function does not use / process this
 *                   argument.
 * @param event_base The base of the actual event. The handler *can handle*
 *                   occurences of ``IP_EVENT`` and ``WIFI_EVENT``.
 * @param event_id   The actual event, as specified by its ``base`` and its
 *                   ``id``.
 * @param event_data Additional data of the event, provided as a ``void *``. The
 *                   event handler is responsible for making any sense of this.
 *                   As of now, the function does not use / process this
 *                   argument.
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
 * @param notification As specified in ::mnet32_task_notification.
 */
void mnet32_notify(uint32_t notification);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_
