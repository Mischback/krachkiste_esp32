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

/* C's standard libraries. */
#include <stdlib.h>
#include <string.h>

/* Other headers of the component */
#include "mnet32/mnet32.h"
#include "mnet32_wifi.h"

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

static esp_err_t mnet32_web_get_value(char *key, char *raw, char **value) {
    /* 1. step: find the key... */
    char *key_offset = strstr(raw, key);
    if (key_offset == NULL) {
        ESP_LOGE(TAG, "Could not find '%s' in '%s'!", key, raw);
        return ESP_FAIL;
    }

    /* 2. step: move from key to the actual value... */
    char *value_begin = key_offset + strlen(key) + 1;

    /* 3. step: find the end of the value... */
    char *value_end = strstr(value_begin, "&");
    size_t value_len;
    if (value_end == NULL) {
        value_len = (raw + strlen(raw)) - value_begin;
    } else {
        value_len = value_end - value_begin;
    }
    ESP_LOGV(TAG, "value_len: %d", value_len);

    /* 4. step: get the unescaped value... */
    char *esc_value = calloc(sizeof(char), value_len + 1);
    strncpy(esc_value, value_begin, value_len);
    ESP_LOGD(TAG, "Found value '%s' (unescaped) for key '%s'.", esc_value, key);

    // This is loosely based on
    // https://github.com/abejfehr/URLDecode/blob/master/urldecode.c
    int i = 0;
    char estr[] = "00";
    int64_t converted;
    while (esc_value[i] != '\0') {
        if (esc_value[i] == '%') {
            if (isxdigit(esc_value[i+1]) && isxdigit(esc_value[i+2])) {
                estr[0] = esc_value[i+1];
                estr[1] = esc_value[i+2];

                converted = strtol(estr, NULL, 16);
                memmove(
                    &esc_value[i+1],
                    &esc_value[i+3],
                    strlen(&esc_value[i+3]) + 1);

                esc_value[i] = converted;
            }
        }
        i++;
    }

    ESP_LOGD(TAG, "Found value '%s' (escaped) for key '%s'.", esc_value, key);

    /* 5. step: Write the value back and clean up. */
    strcpy((char *)value, esc_value);  // NOLINT(runtime/printf)

    free(esc_value);
    return ESP_OK;
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

    /* Receive POST body */
    char *buf = calloc(sizeof(char), request->content_len + 1);
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
    ESP_LOGD(TAG, "received [%s]", buf);

    /* Parse POST body */
    char ssid[MNET32_WIFI_SSID_MAX_LEN];
    char psk[MNET32_WIFI_PSK_MAX_LEN];
    memset(ssid, 0x00, MNET32_WIFI_SSID_MAX_LEN);
    memset(psk, 0x00, MNET32_WIFI_PSK_MAX_LEN);

    mnet32_web_get_value("ssid", buf, (char **)&ssid);
    mnet32_web_get_value("psk", buf, (char **)&psk);
    free(buf);

    ESP_LOGD(TAG, "SSID: %s", ssid);
    ESP_LOGD(TAG, "PSK:  %s", psk);

    /* Write new credentials to NVS */
    // TODO(mischback) Write credentials to NVS

    /* Trigger restart of WiFi */
    // TODO(mischback) Actually restart WiFi to apply new credentials

    /* Provide a HTTP response */
    httpd_resp_set_status(request, "204 No Response");
    return httpd_resp_send(request, "", HTTPD_RESP_USE_STRLEN);
}
