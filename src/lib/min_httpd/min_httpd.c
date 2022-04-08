// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide a minimal webserver for the project.
 *
 * This file is the actual implementation of the component. For a detailed
 * description of the actual usage, refer to min_httpd.h .
 *
 * **Resources:**
 *   - https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server
 *
 * @file   min_httpd.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This files header */
#include "min_httpd.h"

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ``ESP_ERROR_CHECK``
 * - defines the return values ``ESP_OK`` (0) and ``ESP_FAIL`` (-1)
 */
#include "esp_err.h"

/* This is ESP-IDF's event library.
 * - defines ``esp_event_base_t``
 */
#include "esp_event.h"

/* This is EPS-IDF's http server library. */
#include "esp_http_server.h"

/* This is ESP-IDF's logging library.
 * - ESP_LOGE(TAG, "Error");
 * - ESP_LOGW(TAG, "Warning");
 * - ESP_LOGI(TAG, "Info");
 * - ESP_LOGD(TAG, "Debug");
 * - ESP_LOGV(TAG, "Verbose");
 */
#include "esp_log.h"

/* ***** VARIABLES ********************************************************* */
/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "krachkiste.httpd";

static httpd_handle_t server = NULL;


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t min_httpd_server_start(void);


/* ***** FUNCTIONS ********************************************************* */

// Documentation in header file!
void min_httpd_external_event_handler_start(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "Entering min_httpd_external_event_handler_start()");

    if (server) {
        ESP_LOGE(TAG, "Server seems to be already running!");
    } else {
        ESP_LOGD(TAG, "Starting server...");
        ESP_ERROR_CHECK_WITHOUT_ABORT(min_httpd_server_start());
    }
}

// Documentation in header file!
void min_httpd_external_event_handler_stop(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "Entering min_httpd_external_event_handler_stop()");
}

static esp_err_t min_httpd_server_start(void) {
    ESP_LOGV(TAG, "Entering min_httpd_server_start()");

    // Create the config
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;  // from ESP-IDF's example code
    config.server_port = MIN_HTTPD_HTTP_PORT;
    config.max_uri_handlers = MIN_HTTPD_MAX_URI_HANDLERS;

    ESP_LOGD(TAG, "task_priority: %d", config.task_priority);        // 5
    ESP_LOGD(TAG, "server_port: %d", config.server_port);            // 80
    ESP_LOGD(TAG, "max_open_sockets: %d", config.max_open_sockets);  // 7
    ESP_LOGD(TAG, "max_uri_handlers: %d", config.max_uri_handlers);  // 8

    // Start the server
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(
            TAG,
            "Server successfully started, listening on %d",
            config.server_port);
        return ESP_OK;
    }

    // Return ESP_FAIL (-1) if the server failed to start
    ESP_LOGE(TAG, "Error starting httpd!");
    server = NULL;
    return ESP_FAIL;
}
