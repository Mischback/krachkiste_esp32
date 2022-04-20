// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide the functions related to the project-specific access point.
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *
 * @file   wifi_ap.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This files header */
#include "wifi.h"

/* C-standard for string operations */
#include <string.h>

/* Other headers of the component */
#include "networking/networking.h"

/* The FreeRTOS headers are required for timers */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ``ESP_ERROR_CHECK``
 * - defines the return values ``ESP_OK`` (0) and ``ESP_FAIL`` (-1)
 */
#include "esp_err.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ESP-IDF's network abstraction layer */
#include "esp_netif.h"

/* ESP-IDF's wifi library */
#include "esp_wifi.h"


/**
 * The maximum length of the ``char`` array to store SSID.
 *
 * IEEE 802.11 says, that the maximum length of an SSID is 32, so this is set
 * to 33 (to allow for the terminating ``"\0"``).
 *
 * @todo Should this be made configurable by project settings? 33 is in fact
 *       a value derived from IEEE 802.11, so allowing for manual configuration
 *       can *only* be used to minimize the required memory usage. But the
 *       saving would be minimal, probably not worth the effort.
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 33

/**
 * The maximum length of the ``char`` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE 801.11 says, that the maximum length of an PSK is 64, so this is set
 * to 65 (to allow for the terminating ``"\0"``).
 *
 * @todo Should this be made configurable by project settings? 65 is in fact
 *       a value derived from IEEE 802.11, so allowing for manual configuration
 *       can *only* be used to minimize the required memory usage. But the
 *       saving would be minimal, probably not worth the effort.
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 65


typedef struct {
    char wifi_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char wifi_password[NETWORKING_WIFI_PSK_MAX_LEN];
} networking_wifi_config_t;


/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "krachkiste.networking";

static networking_wifi_config_t project_wifi_config;

/**
 * Reference to the ``netif`` object for the access point.
 */
static esp_netif_t* networking_wifi_ap_netif = NULL;

/**
 * Reference to the freeRTOS' ``timer`` object that is used to shutdown the
 * access point.
 */
static TimerHandle_t networking_wifi_ap_shutdown_timer = NULL;


/* ***** PROTOTYPES ******************************************************** */
static void networking_wifi_ap_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);
static void networking_wifi_ap_shutdown_callback(TimerHandle_t xTimer);


/* ***** FUNCTIONS ********************************************************* */

/**
 * Handle events while the project-specific access point is running.
 *
 * During operation of the access point, several events must be monitored and
 * might trigger application- or project-specific actions.
 *
 * This event handler is registered in ::networking_wifi_ap_initialize. The
 * handler is attached to the ``default`` event loop, as provided by
 * ``esp_event`` (the loop is actually created/started in ::app_main).
 *
 * During the access point's shutdown (performed by
 * ::networking_wifi_ap_shutdown_callback), this event handler is unregistered.
 *
 * @param arg        Generic arguments.
 * @param event_base ``esp_event``'s ``EVENT_BASE``. Every event is specified
 *                   by the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_id   ``esp_event``'s ``EVENT_ID``. Every event is specified by
 *                   the ``EVENT_BASE`` and its ``EVENT_ID``.
 * @param event_data Events might provide a pointer to additional,
 *                   event-related data.
 */
static void networking_wifi_ap_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {

    switch (event_id) {
        // Please note, that all ``case`` statements have an empty statement
        // (``{}``) appended to enable the declaration of variables with the
        // correct scope. See https://stackoverflow.com/a/18496437
        // However, ``cpplint`` does not like the recommended ``;`` and
        // recommends an empty block ``{}``.
        case WIFI_EVENT_AP_START: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_START");

            // The access point got started: create a timer to shut it down
            // (eventually).
            xTimerStart(networking_wifi_ap_shutdown_timer, (TickType_t) 0);
            break;
        case WIFI_EVENT_AP_STACONNECTED: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STACONNECTED");

            wifi_event_ap_staconnected_t* connect =
                (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[connect] "MACSTR" (%d)",
                MAC2STR(connect->mac),
                connect->aid);

            // A client connected to the access point, stop the shutdown timer!
            if (xTimerIsTimerActive(
                networking_wifi_ap_shutdown_timer) == pdTRUE) {
                xTimerStop(networking_wifi_ap_shutdown_timer, (TickType_t) 0);
            }
            break;
        case WIFI_EVENT_AP_STADISCONNECTED: {}
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STADISCONNECTED");

            wifi_event_ap_stadisconnected_t* disconnect =
                (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGD(
                TAG,
                "[disconnect] "MACSTR" (%d)",
                MAC2STR(disconnect->mac),
                disconnect->aid);
            break;
        default:
            break;
    }
}

esp_err_t networking_wifi_ap_initialize(void) {
    ESP_LOGV(TAG, "Entering networking_wifi_ap_initialize()");

    networking_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    networking_wifi_ap_shutdown_timer = xTimerCreate(
        NULL,
        pdMS_TO_TICKS(NETWORKING_WIFI_AP_LIFETIME),
        pdFALSE,
        (void *) 0,
        networking_wifi_ap_shutdown_callback);

    wifi_init_config_t ap_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ap_init_cfg));

    // Register event handler for AP mode
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &networking_wifi_ap_event_handler,
        NULL,
        NULL));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = NETWORKING_WIFI_AP_SSID,
            .ssid_len = strlen(NETWORKING_WIFI_AP_SSID),
            .channel = NETWORKING_WIFI_AP_CHANNEL,
            .password = NETWORKING_WIFI_AP_PSK,
            .max_connection = NETWORKING_WIFI_AP_MAX_CONNS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    // esp_wifi requires PSKs to be at least 8 characters. Just switch to
    // an **open** WiFi, if the provided PSK has less than 8 characters.
    // TODO(mischback): Verify how that minimal password length is set. Is this
    //                  an ``#define`` that may be picked up in our code or is
    //                  it really hardcoded in the esp_wifi library/component?
    if (strlen(NETWORKING_WIFI_AP_PSK) <= 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
        ESP_LOGW(
            TAG,
            "The provided PSK for the access point has less than 8 characters, "
            "switching to an open WiFi. No password will be required to "
            "connect to the access point.");
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(
        TAG,
        "Launched Access Point! SSID: '%s', PSK: '%s'",
        ap_config.ap.ssid,
        ap_config.ap.password);

    return ESP_OK;
}

/**
 * Shut down the access point and clean up.
 *
 * This function is a callback to freeRTOS' ``timer`` implementation. The
 * actual ``TimerHandle_t`` is ::networking_wifi_ap_shutdown_timer,
 * which is created/initialized in ::networking_wifi_ap_initialize and
 * started in ::networking_wifi_ap_event_handler.
 *
 * @param xTimer Reference to the freeRTOS' ``timer`` that triggered this
 *               callback.
 */
static void networking_wifi_ap_shutdown_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "Access Point is shutting down...");

    // Unregister the event handler for AP mode
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &networking_wifi_ap_event_handler));

    // Stop the access point and remove its netif
    ESP_ERROR_CHECK(esp_wifi_stop());
    esp_netif_destroy_default_wifi(networking_wifi_ap_netif);
    networking_wifi_ap_netif = NULL;

    // Stop and remove the timer
    xTimerStop(xTimer, (TickType_t) 0);
    xTimerDelete(xTimer, (TickType_t) 0);
    networking_wifi_ap_shutdown_timer = NULL;

    ESP_LOGI(TAG, "Access Point is shut down!");
}

esp_err_t wifi_initialize(char* nvs_namespace) {
    ESP_LOGV(TAG, "Entering wifi_initialize()");

    // This is the entry point of the wifi-related source code. First of all,
    // initialize the module's variables.
    memset(&project_wifi_config, 0x00, sizeof(networking_wifi_config_t));
    ESP_LOGD(
        TAG,
        "sizeof(networking_wifi_config_t) = %d",
        sizeof(networking_wifi_config_t));

    // Read WiFi credentials from non-volatile storage (NVS)


    return ESP_OK;
}
