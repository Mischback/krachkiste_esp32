// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * The web interface of the ``mnet32`` component.
 *
 * Basically this includes **URI definitions** and the respective
 * **URI handler** implementations.
 *
 * @file   mnet32_web.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* This files header */
#include "mnet32_web.h"

/* Other headers of the component */
#include "mnet32/mnet32.h"

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
static const char* TAG = "mnet32.web";


/* ***** PROTOTYPES ******************************************************** */
static esp_err_t mnet32_web_handler_config_get(httpd_req_t* request);
static esp_err_t mnet32_web_handler_config_post(httpd_req_t *request);


/* ***** URI DEFINITIONS ***************************************************
 * (technically, these are ``variables``, but as the handler functions must be
 *  referenced, these must come after the ``prototypes``)
 */

/**
 * URI definition for the configuration page, serving the actual form.
 */
static const httpd_uri_t mnet32_web_uri_config_get = {
    .uri = MNET32_WEB_URL_CONFIG,
    .method = HTTP_GET,
    .handler = mnet32_web_handler_config_get,
    .user_ctx = NULL
};

/**
 * URI definition for the configuration page, processing the actual form.
 */
static const httpd_uri_t mnet32_web_uri_config_post = {
    .uri = MNET32_WEB_URL_CONFIG,
    .method = HTTP_POST,
    .handler = mnet32_web_handler_config_post,
    .user_ctx = NULL
};


/* ***** FUNCTIONS ********************************************************* */

void mnet32_web_attach_handlers(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    // Get the server from ``event_data``
    httpd_handle_t server = *((httpd_handle_t*) event_data);

    // Register this component's *URI handlers* with the server instance.
    httpd_register_uri_handler(server, &mnet32_web_uri_config_get);
    httpd_register_uri_handler(server, &mnet32_web_uri_config_post);
}

/**
 * Show the WiFi configuration form.
 *
 * The matching *URI definition* is ::mnet32_web_uri_config_get.
 *
 * @param request The request that should be responded to with this function.
 * @return Always returns ``ESP_OK``.
 */
static esp_err_t mnet32_web_handler_config_get(httpd_req_t *request) {
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

static esp_err_t mnet32_web_handler_config_post(httpd_req_t *request) {
    ESP_LOGV(TAG, "mnet32_web_handler_config_post()");

    char *buf = malloc(request->content_len + 1);
    size_t off = 0;

    while (off < request->content_len) {
        int ret = httpd_req_recv(
            request,
            buf + off,
            request->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(request);
            }
            free(buf);
            return ESP_FAIL;
        }
        off += ret;
        ESP_LOGV(TAG, "received %d bytes", ret);
    }
    buf[off] = '\0';
    ESP_LOGD(TAG, "received [%s]", buf);

    free(buf);

    return ESP_FAIL;
}
