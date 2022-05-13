// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

#ifndef SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_
#define SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_

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
 * @todo The current implementation defines ``case`` statements for all events,
 *       that *might* happen. Probably many of them are not acutally required
 *       for the component to work. All ``case`` statements do provide a log
 *       message of level ``VERBOSE``.
 *       a) Can this code be kept without impacting the build size? Or in other
 *          words: Will these statements be *optimized away* if the minimum
 *          log level is greater than ``VERBOSE``?
 *       b) For actual *events in use* by the component, switch to log level
 *          ``DEBUG``.
 * @todo When ``ethernet`` networking is implemented, the related events must
 *       be included here!
 */
void mnet32_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);

#endif  // SRC_LIB_EMBEDDED_NETWORKING_ESP32_SRC_MNET32_INTERNAL_H_
