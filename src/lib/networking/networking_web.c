// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * The web interface of the ``networking`` component.
 *
 * Basically this includes **URI definitions** and the respective
 * **URI handler** implementations.
 *
 * @file   networking_web.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This files header */
#include "networking_web.h"

/* This is ESP-IDF's error handling library.
 * - defines the type ``esp_err_t``
 * - defines the macro ``ESP_ERROR_CHECK``
 * - defines the return values ``ESP_OK`` (0) and ``ESP_FAIL`` (-1)
 */
#include "esp_err.h"

/* This is EPS-IDF's http server library.
 * This header file also includes "http_parser.h", which provides the function
 * ``http_method_str``.
 */
#include "esp_http_server.h"


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t networking_web_handler_get_config(httpd_req_t* request);


/* ***** URI DEFINITIONS ***************************************************
 * (technically, these are ``variables``, but as the handler functions must be
 *  referenced, these must come after the ``prototypes``)
 */

/**
 * URI definition for the *homepage*, which will be served from ``/``.
 *
 * @todo Make this configurable (pre-build with ``sdkconfig``)
 * @todo Move this to networking.h
 * @todo Add networking.h to this file's includes
 */
static const httpd_uri_t networking_web_uri_get_config = {
    .uri = "/config/wifi",
    .method = HTTP_GET,
    .handler = networking_web_handler_get_config,
    .user_ctx = NULL
};


/* ***** FUNCTIONS ********************************************************* */

/**
 * Show the WiFi configuration form.
 *
 * The matching *URI definition* is ::networking_web_uri_get_config.
 *
 * @param request The request that should be responded to with this function.
 * @return Always returns ``ESP_OK``.
 */
static esp_err_t networking_web_handler_get_config(httpd_req_t* request) {
    // Access the embedded HTML file.
    // See this component's ``CMakeLists.txt`` for the actual embedding (in
    // ``idf_component_register()``) and see **ESP-IDF**'s documentation on how
    // to access it: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#embedding-binary-data
    extern const uint8_t resource_start[] asm("_binary_wifi_config_html_start");
    extern const uint8_t resource_end[]   asm("_binary_wifi_config_html_end");
    const size_t resource_size = resource_end - resource_start;

    return(httpd_resp_send(
        request,
        (const char*) resource_start,
        resource_size));
}
