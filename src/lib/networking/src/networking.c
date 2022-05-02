// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * Provide basic management of the networking capabilities of the MCU.
 *
 * This file is the actual implementation of the component. For a detailed
 * description of the actual usage, including how the component may be reused
 * in some other codebase, refer to networking.h .
 *
 * **Resources:**
 *   - https://github.com/nkolban/esp32-snippets/tree/master/networking/bootwifi
 *   - https://github.com/espressif/esp-idf/tree/master/examples/wifi/
 *   - https://github.com/tonyp7/esp32-wifi-manager/blob/master/src/wifi_manager.c
 *   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_eth.html
 *
 * @file   networking.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */

/* This file's header */
#include "networking/networking.h"

/* C's standard libraries. */
#include <string.h>

/* This is ESP-IDF's error handling library.
 * - defines the **type** ``esp_err_t``
 * - defines common return values (``ESP_OK``, ``ESP_FAIL``)
 */
#include "esp_err.h"

/* This is ESP-IDF's event library. */
#include "esp_event.h"

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

/* ESP-IDF's wifi library.
 * The header has to be included here, because the component uses one single
 * event handler function (::networking_event_handler), thus, the specific
 * WiFi-related events must be known.
 */
#include "esp_wifi.h"

/* FreeRTOS headers.
 * - the ``FreeRTOS.h`` is required
 * - ``task.h`` for task management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* This is ESP-IDF's library to interface the non-volatile storage (NVS). */
#include "nvs_flash.h"


/* ***** DEFINES *********************************************************** */

/**
 * The index to be used by ``xTaskNotifyWaitIndex``.
 *
 * **freeRTOS** does support tasks with with multiple *notifications*, however,
 * **ESP-IDF** limits this to just one, which is placed at index ``0`` (multiple
 * notification values are supported starting at 10.4.0).
 *
 * The component however uses the generic versions of ``xTaskNotifyWait()`` and
 * ``xTaskNotify()``, so this constant ensures, that the notifications will be
 * send to the right *receiver*.
 */
#define NETWORKING_TASK_NOTIFICATION_INDEX 0

/**
 * The component-specific key to access the NVS to set/get the stored SSID.
 */
#define NETWORKING_WIFI_NVS_SSID "net_ssid"

/**
 * The component-specific key to access the NVS to set/get the stored WiFi
 * password.
 */
#define NETWORKING_WIFI_NVS_PSK "net_psk"

/**
 * The maximum length of the ``char`` array to store SSID.
 *
 * IEEE 802.11 says, that the maximum length of an SSID is 32, which is also
 * the value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_SSID_MAX_LEN 32

/**
 * The maximum length of the ``char`` array to store the pre-shared key
 * for a WiFi connection.
 *
 * IEEE 801.11 says, that the maximum length of an PSK is 64, which is also the
 * value provided in **ESP-IDF**'s ``esp_wifi_types.h``.
 */
#define NETWORKING_WIFI_PSK_MAX_LEN 64

/* ***** TYPES ************************************************************* */

/**
 * This is the list of accepted notifications.
 */
typedef enum {
    NETWORKING_NOTIFICATION_BASE,
    NETWORKING_NOTIFICATION_CMD_WIFI_START,
} networking_notification;

/**
 * Specify the actual connection medium.
 *
 * This might be a wired connection (``NETWORKING_MEDIUM_ETHERNET``) or a
 * wireless connection (``NETWORKING_MEDIUM_WIRELESS``) when the component is
 * actually up and running.
 *
 * This is actually tracked in the component's ::state.
 */
typedef enum {
    NETWORKING_MEDIUM_UNSPECIFIED,
    NETWORKING_MEDIUM_ETHERNET,
    NETWORKING_MEDIUM_WIRELESS,
} networking_medium;

/**
 * Specify the mode of the wireless connection.
 *
 * This is only applicable for ``NETWORKING_MEDIUM_WIRELESS`` and will be set
 * to ``NETWORKING_MODE_NOT_APPLICABLE`` on initialization or if the medium
 * is set to ``NETWORKING_MEDIUM_ETHERNET``.
 */
typedef enum {
    NETWORKING_MODE_NOT_APPLICABLE,
    NETWORKING_MODE_WIFI_AP,
    NETWORKING_MODE_WIFI_STA,
} networking_mode;

/**
 * Specify the actual status of the connection.
 *
 * The connection status must be evaluated in the context of its ``medium`` -
 * and in case of a wireless connection - its ``mode``.
 */
typedef enum {
    NETWORKING_STATUS_DOWN,
    NETWORKING_STATUS_READY,
    NETWORKING_STATUS_UP,
} networking_status;

/**
 * A component-specific struct to keep track of the internal state.
 */
struct networking_state {
    networking_medium   medium;
    networking_mode     mode;
    networking_status   status;
    esp_netif_t         *interface;
    TaskHandle_t        task;
    esp_event_handler_t ip_event_handler;
    esp_event_handler_t wifi_event_handler;
};


/* ***** VARIABLES ********************************************************* */

/**
 * Set the module-specific ``TAG`` to be used with ESP-IDF's logging library.
 *
 * See
 * [its API documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html#how-to-use-this-library).
 */
static const char* TAG = "networking";

/**
 * Track the internal state of the component.
 */
static struct networking_state *state = NULL;


/* ***** PROTOTYPES ******************************************************** */

static esp_err_t get_wifi_config_from_nvs(
    char *nvs_namespace,
    char *ssid,
    char *psk);

static void networking(void *task_parameters);
static void networking_notify(uint32_t notification);
static void networking_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data);
static esp_err_t networking_deinit(void);
static esp_err_t networking_init(char* nvs_namespace);

static esp_err_t wifi_start(char *nvs_namespace);
static esp_err_t wifi_init(char *nvs_namespace);
static esp_err_t wifi_deinit(void);
static esp_err_t wifi_ap_init(void);
static esp_err_t wifi_ap_deinit(void);
static esp_err_t wifi_sta_init(void);
static esp_err_t wifi_sta_deinit(void);


/* ***** FUNCTIONS ********************************************************* */

static void networking(void *task_parameters) {
    ESP_LOGV(TAG, "networking()");
    ESP_LOGV(TAG, "This is the actual task function!");

    // TODO(mischback) Make this configurable by ``sdkconfig``!
    const TickType_t mon_freq = pdMS_TO_TICKS(5000);  // 5 sec
    BaseType_t notify_result;
    uint32_t notify_value = 0;

    ESP_LOGV(TAG, "networking(): Entering infinite loop...");
    for (;;) {
        /* Block until notification or ``mon_freq`` reached. */
        notify_result = xTaskNotifyWaitIndexed(
            NETWORKING_TASK_NOTIFICATION_INDEX,
            pdFALSE,
            ULONG_MAX,
            &notify_value,
            mon_freq);

        /* Notification or monitoring? */
        if (notify_result == pdPASS) {
            switch (notify_value) {
            case NETWORKING_NOTIFICATION_CMD_WIFI_START:
                ESP_LOGD(TAG, "CMD: WIFI_START");
                if (wifi_start("krachkiste") != ESP_OK) {
                    ESP_LOGE(TAG, "Could not start WiFi!");
                }
                break;
            default:
                ESP_LOGW(TAG, "Got unhandled notification: %d", notify_value);
                break;
            }
        } else {
            // TODO(mischback) The component should kind of *publish* relevant
            //                 informations of its internal state using
            //                 **EPS-IDF**'s event system. This will be the
            //                 place to actually emit the *event*.
            ESP_LOGV(TAG, "'mon_freq' reached...");
        }
    }

    /* This should probably not be reached!
     * ``freeRTOS`` requires the task functions *to never return*. Instead,
     * the common idiom is to delete the very own task at the end of these
     * functions.
     */
    vTaskDelete(NULL);
}

static void networking_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data) {
    ESP_LOGV(TAG, "networking_event_handler()");

    if (event_base == WIFI_EVENT) {
        /* All WIFI_EVENT event_ids
         * See [ESP-IDF documentation link]
         */
        switch (event_id) {
        case WIFI_EVENT_WIFI_READY:
            ESP_LOGV(TAG, "WIFI_EVENT_WIFI_READY");
            break;
        case WIFI_EVENT_SCAN_DONE:
            ESP_LOGV(TAG, "WIFI_EVENT_SCAN_DONE");
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_START");
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_STOP");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGV(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGED");
            break;
        case WIFI_EVENT_STA_WPS_ER_SUCCESS:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_SUCCESS");
            break;
        case WIFI_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_FAILED");
            break;
        case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_TIMEOUT");
            break;
        case WIFI_EVENT_STA_WPS_ER_PIN:
            ESP_LOGV(TAG, "WIFI_EVENT_WPS_ER_PIN");
            break;
        case WIFI_EVENT_AP_START:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_START");
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STOP");
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STACONNECTED");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            break;
        case WIFI_EVENT_AP_PROBEREQRECVED:
            ESP_LOGV(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
            break;
        default:
            ESP_LOGW(TAG, "Got unhandled WIFI_EVENT: '%d'", event_id);
            break;
        }
    }

    if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGV(TAG, "IP_EVENT_STA_GOT_IP");
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGV(TAG, "IP_EVENT_STA_LOST_IP");
            break;
        case IP_EVENT_AP_STAIPASSIGNED:
            ESP_LOGV(TAG, "IP_EVENT_AP_STAIPASSIGNED");
            break;
        case IP_EVENT_GOT_IP6:
            ESP_LOGV(TAG, "IP_EVENT_GOT_IP6");
            break;
        case IP_EVENT_ETH_GOT_IP:
            ESP_LOGV(TAG, "IP_EVENT_ETH_GOT_IP");
            break;
        case IP_EVENT_ETH_LOST_IP:
            ESP_LOGV(TAG, "IP_EVENT_ETH_LOST_IP");
            break;
        default:
            ESP_LOGW(TAG, "Got unhandled IP_EVENT: '%d'", event_id);
            break;
        }
    }
}

static esp_err_t networking_init(char* nvs_namespace) {
    // Set log-level of our own code to VERBOSE
    // FIXME: Final code should not do this, but respect the project's settings
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGV(TAG, "networking_init()");

    /* Check if this a recurrent call to the function. */
    if (state != NULL) {
        ESP_LOGE(TAG, "Internal state already initialized!");
        return ESP_FAIL;
    }

    esp_err_t esp_ret;

    /* Initialize the network stack.
     * This has to be done exactly one time.
     * While this is a mandatory requirement of this component, the actual
     * application may be functional without network access.
     */
    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize network stack!");
        ESP_LOGD(TAG, "'esp_netif_init()' returned %d", esp_ret);
        return ESP_FAIL;
    }

    /* Initialize internal state information */
    state = calloc(1, sizeof(*state));
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;
    state->mode = NETWORKING_MODE_NOT_APPLICABLE;
    state->status = NETWORKING_STATUS_DOWN;

    /* Register IP_EVENT event handler.
     * These events are required for any *medium*, so the handler can already
     * be registered at this point. The handlers for medium-specific events
     * are registered in the respective module.
     */
    esp_ret = esp_event_handler_instance_register(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        networking_event_handler,
        NULL,
        (void **)&(state->ip_event_handler));
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach IP_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %d",
            esp_ret);
        return ESP_FAIL;
    }

    /* Create the actual dedicated task for the component. */
    // TODO(mischback) Determine a usable ``usStackDepth``!
    //                 a) In **ESP-IDF** the ``usStackDepth`` is not based on
    //                    the actual stack width, but ``StackType_t`` is
    //                    ``uint_8`` (one byte).
    //                 b) Verify configMINIMAL_STACK_SIZE (``freeRTOS.h``)!
    //                 c) 4096 byte = 4KB. This seems quite high. Before this
    //                    component may be considered *completed*, this value
    //                    should be minimized (see freeRTOS'
    //                    ``uxTaskGetStackHighWaterMark()``).
    if (xTaskCreate(
            networking,
            "networking",
            4096,
            NULL,
            NETWORKING_TASK_PRIORITY,
            &(state->task)) != pdPASS) {
        ESP_LOGE(TAG, "Could not create task!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully initialized networking component.");
    ESP_LOGD(TAG, "state->medium.............. %d", state->medium);
    ESP_LOGD(TAG, "state->mode................ %d", state->mode);
    ESP_LOGD(TAG, "state->status.............. %d", state->status);
    ESP_LOGD(TAG, "state->task................ %p", state->task);
    ESP_LOGD(TAG, "state->ip_event_handler.... %p", state->ip_event_handler);
    ESP_LOGD(TAG, "state->wifi_event_handler.. %p", state->wifi_event_handler);

    /* Place the first command for the dedicated networking task. */
    networking_notify(NETWORKING_NOTIFICATION_CMD_WIFI_START);

    return ESP_OK;
}

static esp_err_t networking_deinit(void) {
    ESP_LOGV(TAG, "networking_deinit()");

    if (state == NULL) {
        ESP_LOGE(TAG, "No state information available!");
        return ESP_FAIL;
    }

    /* Unregister the IP_EVENT event handler. */
    if (esp_event_handler_instance_unregister(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        state->ip_event_handler) != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister IP_EVENT event handler!");
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    /* Stop and remove the dedicated networking task */
    if (state->task != NULL)
        vTaskDelete(state->task);

    /* Free internal state memory. */
    free(state);
    state = NULL;

    /* De-initialize the network stack.
     * This is actually not supported by **ESP-IDF**, but included here for
     * completeness and to be fully compatible, *if* **ESP-IDF** include this
     * in a future release.
     */
    if (esp_netif_deinit() != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(
            TAG,
            "'esp_netif_deinit' returned with an unexpected return code");
    }

    return ESP_OK;
}

static void networking_notify(uint32_t notification) {
    ESP_LOGV(TAG, "networking_notify()");

    xTaskNotifyIndexed(
        state->task,
        NETWORKING_TASK_NOTIFICATION_INDEX,
        notification,
        eSetValueWithOverwrite);
}

esp_err_t networking_start(char* nvs_namespace) {
    ESP_LOGV(TAG, "networking_start()");

    if (networking_init(nvs_namespace) != ESP_OK) {
        networking_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t get_wifi_config_from_nvs(
    char *nvs_namespace,
    char *ssid,
    char *psk) {
    ESP_LOGV(TAG, "get_wifi_config_from_nvs()");

    /* Open NVS storage handle. */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(nvs_namespace, NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        /* This might fail for different reasons, e.g. the NVS is not correctly
         * set up or initialized.
         * Assuming that the NVS **is** available, this will fail with
         * ESP_ERR_NVS_NOT_FOUND, which means that there is no namespace of
         * the name ``nvs_namespace`` (yet).
         * This might happen during first start of the applications, as there
         * is no WiFi config yet, so the namespace was never used before.
         */
        ESP_LOGE(TAG, "Could not open NVS handle (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Handle '%s' successfully opened!", nvs_namespace);

    size_t req_size;
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_SSID, NULL, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for SSID!");
        return ESP_FAIL;
    }
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_SSID, ssid, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_SSID,
            esp_err_to_name(err));
        return ESP_FAIL;
    }

    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_PSK, NULL, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not determine size for PSK!");
        return ESP_FAIL;
    }
    err = nvs_get_str(nvs_handle, NETWORKING_WIFI_NVS_PSK, psk, &req_size);
    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read value of %s (%s)",
            NETWORKING_WIFI_NVS_PSK,
            esp_err_to_name(err));
        return ESP_FAIL;
    }


    return ESP_OK;
}

static esp_err_t wifi_init(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_init()");

    /* Initialization has only be performed once */
    if (state->mode != NETWORKING_MODE_NOT_APPLICABLE) {
        return ESP_OK;
    }

    esp_err_t esp_ret;

    /* Initialize the WiFi networking stack. */
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&init_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize WiFi!");
        ESP_LOGD(TAG, "'esp_wifi_init()' returned %d", esp_ret);
        return ESP_FAIL;
    }
    state->medium = NETWORKING_MEDIUM_WIRELESS;

    /* Register WIFI_EVENT event handler.
     * These events are required for any *mode*, so the handler can already
     * be registered at this point.
     */
    esp_ret = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        networking_event_handler,
        NULL,
        (void **)&(state->wifi_event_handler));
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach WIFI_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %d",
            esp_ret);
        return ESP_FAIL;
    }

    /* Read WiFi configuration from non-volatile storage (NVS).
     * If the config can not be read, directly start in access point mode. If
     * there is a config, try station mode first.
     */
    char nvs_sta_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char nvs_sta_psk[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(nvs_sta_ssid, 0x00, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(nvs_sta_psk, 0x00, NETWORKING_WIFI_PSK_MAX_LEN);

    if (get_wifi_config_from_nvs(
        nvs_namespace,
        nvs_sta_ssid,
        nvs_sta_psk) != ESP_OK) {
        return wifi_ap_init();
    }

    return wifi_sta_init();
}

static esp_err_t wifi_deinit(void) {
    ESP_LOGV(TAG, "wifi_deinit()");

    esp_err_t esp_ret;

    /* Unregister the WIFI_EVENT event handler. */
    if (esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        state->wifi_event_handler) != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister WIFI_EVENT event handler!");
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    if (state->mode == NETWORKING_MODE_WIFI_AP)
        wifi_ap_deinit();

    if (state->mode == NETWORKING_MODE_WIFI_STA)
        wifi_sta_deinit();

    esp_ret = esp_wifi_deinit();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinitialization of WiFi failed!");
        ESP_LOGD(TAG, "'esp_wifi_deinit() returned %d", esp_ret);
    }
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;

    return ESP_OK;
}

static esp_err_t wifi_ap_init(void) {
    ESP_LOGV(TAG, "wifi_ap_init()");

    /* Create a network interface for Access Point mode. */
    state->interface = esp_netif_create_default_wifi_ap();
    if (state->interface == NULL) {
        ESP_LOGE(TAG, "Could not create network interface for AP!");
        return ESP_FAIL;
    }

    /* Setup the configuration for access point mode.
     * These values are based off project-specific settings, that may be
     * changed by ``menuconfig`` / ``sdkconfig`` during building the
     * application. They can not be changed through the webinterface.
     */
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
    /* esp_wifi requires PSKs to be at least 8 characters. Just switch to
     * an **open** WiFi, if the provided PSK has less than 8 characters.
     * TODO(mischback): Verify how that minimal password length is set. Is this
     *                  an ``#define`` that may be picked up in our code or is
     *                  it really hardcoded in the esp_wifi library/component?
     */
    if (strlen(NETWORKING_WIFI_AP_PSK) <= 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        memset(ap_config.ap.password, 0, sizeof(ap_config.ap.password));
        ESP_LOGW(
            TAG,
            "The provided PSK for the access point has less than 8 characters, "
            "switching to an open WiFi. No password will be required to "
            "connect to the access point.");
    }

    esp_err_t esp_ret;

    /* */
    esp_ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi mode to AP!");
        ESP_LOGD(TAG, "'esp_wifi_set_mode() returned %d", esp_ret);
        return ESP_FAIL;
    }
    state->mode = NETWORKING_MODE_WIFI_AP;

    esp_ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi config for AP!");
        ESP_LOGD(TAG, "'esp_wifi_set_config()' returned %d", esp_ret);
        return ESP_FAIL;
    }
    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start wifi in AP mode!");
        ESP_LOGD(TAG, "'esp_wifi_start()' returned %d", esp_ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t wifi_ap_deinit(void) {
    ESP_LOGV(TAG, "wifi_ap_deinit()");

    if (state->mode == NETWORKING_MODE_NOT_APPLICABLE) {
        ESP_LOGE(TAG, "WiFi is not initialized!");
        ESP_LOGD(TAG, "Current WiFi mode is %d", state->mode);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_err_t esp_ret = esp_wifi_stop();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not stop WiFi (AP mode)!");
        ESP_LOGD(TAG, "'esp_wifi_stop()' returned %d", esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_netif_destroy_default_wifi(state->interface);
    state->interface = NULL;

    state->mode = NETWORKING_MODE_NOT_APPLICABLE;

    return ESP_OK;
}

static esp_err_t wifi_sta_init(void) {
    ESP_LOGV(TAG, "wifi_sta_init()");
    return ESP_OK;
}

static esp_err_t wifi_sta_deinit(void) {
    ESP_LOGV(TAG, "wifi_sta_deinit()");
    return ESP_OK;
}

static esp_err_t wifi_start(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_start()");

    if (wifi_init(nvs_namespace) != ESP_OK) {
        wifi_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}
