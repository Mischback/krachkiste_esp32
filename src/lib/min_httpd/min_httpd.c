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

/* C-standard for string operations */
#include <string.h>

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

/* This is EPS-IDF's http server library.
 * This header file also includes "http_parser.h", which provides the function
 * ``http_method_str``.
 */
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

/**
 * A reference to the running HTTP server.
 *
 * ``httpd_handle_t`` is in fact a pointer to the server's data structure as
 * created by ``httpd_start()``, which is called from this component's
 * ::min_httpd_server_start.
 */
static httpd_handle_t min_httpd_server = NULL;


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t min_httpd_server_start(void);
static esp_err_t min_httpd_server_stop(void);
static esp_err_t min_httpd_handler_404(
    httpd_req_t* request,
    httpd_err_code_t error_code);
static esp_err_t min_httpd_handler_favicon(httpd_req_t* request);
static esp_err_t min_httpd_handler_home(httpd_req_t* request);


/* ***** URI DEFINITIONS ***************************************************
 * (technically, these are ``variables``, but as the handler functions must be
 *  referenced, these must come after the ``prototypes``)
 */

/**
 * URI definition for the *homepage*, which will be served from ``/``.
 */
static const httpd_uri_t min_httpd_uri_home = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = min_httpd_handler_home,
    .user_ctx = NULL
};

/**
 * URI definition for the *favicon*, which will be served from ``/favicon.ico``.
 */
static const httpd_uri_t min_httpd_uri_favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = min_httpd_handler_favicon,
    .user_ctx = NULL
};


/* ***** FUNCTIONS ********************************************************* */

// Documentation in header file!
void min_httpd_external_event_handler_start(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "Entering min_httpd_external_event_handler_start()");

    if (min_httpd_server) {
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

    if (min_httpd_server) {
        ESP_LOGD(TAG, "Stopping server...");
        ESP_ERROR_CHECK_WITHOUT_ABORT(min_httpd_server_stop());
    } else {
        ESP_LOGE(TAG, "Server doesn't seem to be running!");
    }
}

/**
 * Provide helpful error message for missing/unavailable resources.
 *
 * @param request    The request that causes the execution of the function.
 * @param error_code The error code that causes the execution of the function.
 *                   As this function is only attached to ``404 Not Found``
 *                   errors, this is pretty much set.
 * @return           Always returns ``ESP_FAIL``, causing the underlying socket
 *                   to be closed.
 */
static esp_err_t min_httpd_handler_404(
    httpd_req_t* request,
    httpd_err_code_t error_code) {
    char* error_message;
    asprintf(&error_message, "Sorry, '%s' could not be found!", request->uri);
    httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, error_message);
    free(error_message);
    return ESP_FAIL;
}

/**
 * The handler for the favicon.
 *
 * The matching *URI definition* is ::min_httpd_uri_favicon.
 *
 * @param request The request that should be responded to with this function.
 * @return Always returns ``ESP_OK``.
 */
static esp_err_t min_httpd_handler_favicon(httpd_req_t* request) {
    // Access the embedded HTML file.
    // See this component's ``CMakeLists.txt`` for the actual embedding (in
    // ``idf_component_register()``) and see **ESP-IDF**'s documentation on how
    // to access it: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#embedding-binary-data
    extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = favicon_ico_end - favicon_ico_start;

    esp_err_t return_value = httpd_resp_send(
        request,
        (const char*) favicon_ico_start,
        favicon_ico_size);
    min_httpd_log_message(request, return_value);

    return return_value;
}

/**
 * The handler for the homepage.
 *
 * The matching *URI definition* is ::min_httpd_uri_home.
 *
 * @param request The request that should be responded to with this function.
 * @return Always returns ``ESP_OK``.
 */
static esp_err_t min_httpd_handler_home(httpd_req_t* request) {
    // Access the embedded HTML file.
    // See this component's ``CMakeLists.txt`` for the actual embedding (in
    // ``idf_component_register()``) and see **ESP-IDF**'s documentation on how
    // to access it: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#embedding-binary-data
    extern const uint8_t home_html_start[] asm("_binary_home_html_start");
    extern const uint8_t home_html_end[]   asm("_binary_home_html_end");
    const size_t home_html_length = home_html_end - home_html_start;

    esp_err_t return_value = httpd_resp_send(
        request,
        (const char*) home_html_start,
        home_html_length);
    min_httpd_log_message(request, return_value);

    return return_value;
}

// Documentation in header file!
void min_httpd_log_message(httpd_req_t* request, esp_err_t success) {
    char* log_message;

    if (success == ESP_OK) {
        asprintf(
            &log_message,
            "%s '%s' - OK",
            http_method_str(request->method),
            request->uri);
    } else {
        asprintf(
            &log_message,
            "%s '%s' - FAIL",
            http_method_str(request->method),
            request->uri);
    }

    ESP_LOGI(TAG, "%s", log_message);
    free(log_message);
}

/**
 * Apply configuration values and start the minimal HTTP server.
 *
 * This function is in fact just a very thin wrapper around **ESP-IDF**'s
 * ``httpd_start()``. It creates a default configuration with
 * ``HTTPD_DEFAULT_CONFIG``, changes some project-specific settings (see
 * min_httpd.h for available options) and then launches the server.
 *
 * @return ``ESP_OK`` (equals ``0``) on success, ``ESP_FAIL`` (equals ``-1``)
 *         on failure.
 */
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
    if (httpd_start(&min_httpd_server, &config) == ESP_OK) {
        ESP_LOGI(
            TAG,
            "Server successfully started, listening on %d",
            config.server_port);

        // Register default request handlers
        httpd_register_err_handler(
            min_httpd_server,
            HTTPD_404_NOT_FOUND,
            min_httpd_handler_404);
        httpd_register_uri_handler(min_httpd_server, &min_httpd_uri_home);
        httpd_register_uri_handler(min_httpd_server, &min_httpd_uri_favicon);

        // Return success
        return ESP_OK;
    }

    // Return ESP_FAIL (-1) if the server failed to start
    ESP_LOGE(TAG, "Error starting httpd!");
    min_httpd_server = NULL;
    return ESP_FAIL;
}

/**
 * Stop the running HTTP server.
 *
 * This is just a very thin wrapper around **ESP-IDF**'s ``httpd_stop()``.
 *
 * @return ``ESP_OK`` (equals ``0``) on success, ``ESP_FAIL`` (equals ``-1``)
 *         on failure.
 */
static esp_err_t min_httpd_server_stop(void) {
    ESP_LOGV(TAG, "Entering min_httpd_server_stop()");

    // Nothing fancy, just stop the server
    if (httpd_stop(min_httpd_server) == ESP_OK) {
        ESP_LOGI(TAG, "Server successfully stopped!");
        min_httpd_server = NULL;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to stop the server!");
    return ESP_FAIL;
}
