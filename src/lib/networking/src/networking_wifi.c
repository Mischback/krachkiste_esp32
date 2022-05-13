// SPDX-FileCopyrightText: 2022 Mischback
// SPDX-License-Identifier: MIT
// SPDX-FileType: SOURCE

/**
 * WiFi-related functions of the ``networking`` component.
 *
 * @file   networking_wifi.c
 * @author Mischback
 * @bug    Bugs are tracked with the
 *         [issue tracker](https://github.com/Mischback/krachkiste_esp32/issues)
 *         at GitHub.
 */

/* ***** INCLUDES ********************************************************** */
/* ***** DEFINES *********************************************************** */
/* ***** TYPES ************************************************************* */
/* ***** VARIABLES ********************************************************* */
/* ***** PROTOTYPES ******************************************************** */

static esp_err_t wifi_start(char *nvs_namespace);
static esp_err_t wifi_init(char *nvs_namespace);
static esp_err_t wifi_deinit(void);
static esp_err_t wifi_ap_init(void);
static esp_err_t wifi_ap_deinit(void);
static void wifi_ap_timed_shutdown(TimerHandle_t timer);
static int8_t wifi_ap_get_connected_stations(void);
static void wifi_ap_timer_start(void);
static void wifi_ap_timer_stop(void);
static esp_err_t wifi_sta_init(char **sta_ssid, char **sta_psk);
static esp_err_t wifi_sta_deinit(void);
static void wifi_sta_connect(void);
static int8_t wifi_sta_get_num_connection_attempts(void);
static void wifi_sta_reset_connection_counter(void);


/* ***** FUNCTIONS ********************************************************* */


/**
 * WiFi-specific initialization.
 *
 * Perform the required steps to use the controller's WiFi interface. This
 * includes the initialization of the WiFi interface, registering of
 * ::networking_event_handler for ``WIFI_EVENT`` occurences and reading an
 * optionally available configuration (for station mode) from the non-volatile
 * storage.
 *
 * The actual, *mode-specific* initialization is done in ::wifi_sta_init and
 * ::wifi_ap_init, depending on the availability of credentials for station
 * mode.
 *
 * This function will set ``state->medium`` to ``NETWORKING_MEDIUM_WIRELESS``
 * (``state->mode`` will be set in the specific initialization function).
 *
 * @param nvs_namespace The NVS namespace to read values from.
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure; This
 *                      function may return ``ESP_FAIL`` by itsself or by the
 *                      specific initialization functions ::wifi_sta_init and
 *                      ::wifi_ap_init, see the provided log messages (of
 *                      level ``ERROR`` and ``DEBUG``) for the actual reason
 *                      of failure. **This function does return** ``ESP_OK``
 *                      in case that ``state->mode`` **is not**
 *                      ``NETWORKING_MODE_APPLICABLE``, because it assumes,
 *                      that there has been a previous call to ``wifi_init``.
 */
static esp_err_t wifi_init(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_init()");

    /* Initialization has only be performed once */
    if (state->mode != NETWORKING_MODE_NOT_APPLICABLE) {
        ESP_LOGW(TAG, "WiFi seems to be already initialized!");
        return ESP_OK;
    }

    esp_err_t esp_ret;

    /* Initialize the WiFi networking stack. */
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&init_cfg);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize WiFi!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_init()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
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
        (void **)&(state->medium_event_handler));
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not attach WIFI_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_register()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    /* Read WiFi configuration from non-volatile storage (NVS).
     * If the config can not be read, directly start in access point mode. If
     * there is a config, try station mode first.
     */
    char nvs_sta_ssid[NETWORKING_WIFI_SSID_MAX_LEN];
    char nvs_sta_psk[NETWORKING_WIFI_PSK_MAX_LEN];
    memset(nvs_sta_ssid, 0x00, NETWORKING_WIFI_SSID_MAX_LEN);
    memset(nvs_sta_psk, 0x00, NETWORKING_WIFI_PSK_MAX_LEN);

    esp_ret = get_wifi_config_from_nvs(
        nvs_namespace,
        (char **)&nvs_sta_ssid,
        (char **)&nvs_sta_psk);
    if (esp_ret != ESP_OK) {
        ESP_LOGI(TAG, "Could not read credentials, starting access point!");
        return wifi_ap_init();
    }

    ESP_LOGD(TAG, "Retrieved SSID.. '%s'", nvs_sta_ssid);
    ESP_LOGD(TAG, "Retrieved PSK... '%s'", nvs_sta_psk);

    esp_ret = wifi_sta_init((char **)&nvs_sta_ssid, (char **)&nvs_sta_psk);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start WiFi station mode!");
        ESP_LOGI(TAG, "Starting access point!");
        wifi_sta_deinit();
        return wifi_ap_init();
    }

    /* At this point, the WiFi is started in station mode. All further
     * actions will be triggered by ::networking_event_handler
     */
    return ESP_OK;
}

/**
 * WiFi-specific deinitialization.
 *
 * Clean up for WiFi-related stuff, including deinitialization of the
 * ``netif``, unregistering ::networking_event_handler from ``WIFI_EVENT``
 * and resetting ``state->medium`` to ``NETWORKING_MEDIUM_UNSPECIFIED``.
 *
 * The function calls ::wifi_sta_deinit or ::wifi_ap_deinit, depending on the
 * value of ``state->mode``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
static esp_err_t wifi_deinit(void) {
    ESP_LOGV(TAG, "wifi_deinit()");

    /* Unregister the WIFI_EVENT event handler. */
    esp_err_t esp_ret = esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        state->medium_event_handler);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not unregister WIFI_EVENT event handler!");
        ESP_LOGD(
            TAG,
            "'esp_event_handler_instance_unregister()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    if (state->mode == NETWORKING_MODE_WIFI_AP)
        wifi_ap_deinit();

    if (state->mode == NETWORKING_MODE_WIFI_STA)
        wifi_sta_deinit();

    esp_ret = esp_wifi_deinit();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinitialization of WiFi failed!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_deinit() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
    }
    state->medium = NETWORKING_MEDIUM_UNSPECIFIED;

    return ESP_OK;
}

/**
 * Initialize the WiFi for access point mode.
 *
 * Basically this creates the access point-specific configuration, applies it
 * to **ESP-IDF**'s wifi module and starts the wifi.
 *
 * It sets ``state->mode`` to ``NETWORKING_MODE_WIFI_AP`` and provides a
 * reference to the ``netif`` in ``state->interface``.
 *
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 *                   On ``ESP_OK`` the calling code may assume that the access
 *                   point is successfully started. Subsequent actions are
 *                   triggered by ::networking_event_handler and performed by
 *                   ::networking .
 */
static esp_err_t wifi_ap_init(void) {
    ESP_LOGV(TAG, "wifi_ap_init()");

    /* Create a network interface for access point mode. */
    state->interface = esp_netif_create_default_wifi_ap();
    if (state->interface == NULL) {
        ESP_LOGE(TAG, "Could not create network interface for AP!");
        return ESP_FAIL;
    }

    /* Allocate memory for the specific state information. */
    state->medium_state = calloc(1, sizeof(struct medium_state_wifi_ap));

    /* Create the timer to eventually shut down the access point. */
    ((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer =
        xTimerCreate(
            NULL,
            pdMS_TO_TICKS(NETWORKING_WIFI_AP_LIFETIME),
            pdFALSE,
            (void *) 0,
            wifi_ap_timed_shutdown);

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

    /* Apply configuration and start WiFi interface in access point mode. */
    esp_ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi mode to AP!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_mode() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    state->mode = NETWORKING_MODE_WIFI_AP;

    esp_ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi config for AP!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_config()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start wifi in AP mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_start()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

/**
 * Deinitialize WiFi (access point mode).
 *
 * Stops the access point and destroys the ``netif``.
 *
 * This function is meant to be called by ::wifi_deinit and performs only the
 * specific deinitialization steps of *access point mode*, e.g. it does **not**
 * unregister ::networking_event_handler from ``WIFI_EVENT``.
 *
 * The function sets ``state->mode`` to ``NETWORKING_MODE_NOT_APPLICABLE`` and
 * ``state->interface`` to ``NULL``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
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
        ESP_LOGD(
            TAG,
            "'esp_wifi_stop()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_netif_destroy_default_wifi(state->interface);
    state->interface = NULL;

    if (state->medium_state != NULL) {
        free(state->medium_state);
        state->medium_state = NULL;
    }

    state->mode = NETWORKING_MODE_NOT_APPLICABLE;

    return ESP_OK;
}

static void wifi_ap_timed_shutdown(TimerHandle_t timer) {
    ESP_LOGV(TAG, "wifi_ap_timed_shutdown()");

    if (state->status != NETWORKING_STATUS_IDLE) {
        ESP_LOGW(TAG, "Access Point is not idle! Skipping shutdown!");
        return;
    }

    xTimerStop(timer, (TickType_t) 0);
    xTimerDelete(timer, (TickType_t) 0);

    networking_notify(NETWORKING_NOTIFICATION_CMD_NETWORKING_STOP);
}

/**
 * Get the number of connected stations in access point mode.
 *
 * @return int8_t The number of connected stations or ``-1`` in case of error.
 */
static int8_t wifi_ap_get_connected_stations(void) {
    ESP_LOGV(TAG, "wifi_ap_get_connected_stations()");

    wifi_sta_list_t tmp;

    if (esp_wifi_ap_get_sta_list(&tmp) != ESP_OK) {
        ESP_LOGW(TAG, "Could not determine number of connected stations!");
        return -1;  // indicate error with a totally unexpected value!
    }
    ESP_LOGD(TAG, "Connected stations: %d", tmp.num);

    return tmp.num;
}

/**
 * Start the access point's shutdown timer.
 *
 * The timer is created in ::wifi_ap_init and should be available here. There
 * is a basic check of the presence and a log message of level ``WARNING`` is
 * emitted, if the timer (``TimerHandle_t``) is not available.
 */
static void wifi_ap_timer_start(void) {
    ESP_LOGV(TAG, "wifi_ap_timer_start()");

    if (((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer == NULL) {  // NOLINT(whitespace/line_length)
        ESP_LOGW(TAG, "The ap_shutdown_timer is not available!");
        return;  // fail silently
    }

    if (xTimerIsTimerActive(
        ((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer)  // NOLINT(whitespace/line_length)
        == pdTRUE) {
        ESP_LOGW(TAG, "Access point's shutdown timer is already running!");
        return;  // fail silently
    }

    xTimerStart(
        ((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer,  // NOLINT(whitespace/line_length)
        (TickType_t) 0);
    ESP_LOGD(TAG, "Access point's shutdown timer started!");
}

/**
 * Stop the access point's shutdown timer.
 *
 * The timer is created in ::wifi_ap_init and should be available here. There
 * is a basic check of the presence and a log message of level ``WARNING`` is
 * emitted, if the timer (``TimerHandle_t``) is not available.
 */
static void wifi_ap_timer_stop(void) {
    ESP_LOGV(TAG, "wifi_ap_timer_stop()");

    if (((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer == NULL) {  // NOLINT(whitespace/line_length)
        ESP_LOGW(TAG, "The ap_shutdown_timer is not available!");
        return;  // fail silently
    }

    if (xTimerIsTimerActive(
        ((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer)  // NOLINT(whitespace/line_length)
        != pdTRUE) {
        ESP_LOGW(TAG, "Access point's shutdown timer is not running!");
        return;  // fail silently
    }

    xTimerStop(
        ((struct medium_state_wifi_ap *)(state->medium_state))->ap_shutdown_timer,  // NOLINT(whitespace/line_length)
        (TickType_t) 0);
    ESP_LOGD(TAG, "Access point's shutdown timer stopped!");
}

/**
 * Initialize the WiFi for station mode.
 *
 * Basically this creates the station-specific configuration, applies it to
 * **ESP-IDF**'s wifi module and starts the wifi.
 *
 * It sets ``state->mode`` to ``NETWORKING_MODE_WIFI_STA`` and provides a
 * reference to the ``netif`` in ``state->interface``.
 *
 * @param sta_ssid   The SSID of the WiFi network to connect to.
 * @param sta_psk    The pre-shared key (PSK) of the WiFi network to connect to.
 * @return esp_err_t ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 *                   On ``ESP_OK`` the calling code may assume that the access
 *                   point is successfully started. Subsequent actions are
 *                   triggered by ::networking_event_handler and performed by
 *                   ::networking .
 */
static esp_err_t wifi_sta_init(char **sta_ssid, char **sta_psk) {
    ESP_LOGV(TAG, "wifi_sta_init()");

    /* Create a network interface for Access Point mode. */
    state->interface = esp_netif_create_default_wifi_sta();
    if (state->interface == NULL) {
        ESP_LOGE(TAG, "Could not create network interface for station mode!");
        return ESP_FAIL;
    }

    /* Allocate memory for the specific state information. */
    state->medium_state = calloc(1, sizeof(struct medium_state_wifi_sta));
    wifi_sta_reset_connection_counter();

    /* Setup the configuration for station mode.
     * Some of the settings may be configured by ``menuconfig`` / ``sdkconfig``,
     * the ``SSID`` and ``PSK`` are read from the non-volatile storage (NVS) and
     * passed into this function.
     */
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
            .threshold.rssi = NETWORKING_WIFI_STA_THRESHOLD_RSSI,
            .threshold.authmode = NETWORKING_WIFI_STA_THRESHOLD_AUTH,
        },
    };
    // Inject the SSID / PSK as fetched from the NVS.
    // ``memcpy`` feels like *force*, but it works.
    memcpy(
        sta_config.sta.ssid,
        (char *)sta_ssid,
        strlen((char *)sta_ssid));
    memcpy(
        sta_config.sta.password,
        (char *)sta_psk,
        strlen((char *)sta_psk));

    esp_err_t esp_ret;

    /* Apply configuration and start WiFi interface in station mode .*/
    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi mode to STA!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_mode() returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    state->mode = NETWORKING_MODE_WIFI_STA;

    esp_ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not set wifi config for station mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_set_config()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }
    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start wifi in station mode!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_start()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        return esp_ret;
    }

    return ESP_OK;
}

/**
 * Deinitialize WiFi (station mode).
 *
 * Disconnects from the WiFi and destroys the ``netif``.
 *
 * This function is meant to be called by ::wifi_deinit and performs only the
 * specific deinitialization steps of *station mode*, e.g. it does **not**
 * unregister ::networking_event_handler from ``WIFI_EVENT``.
 *
 * The function sets ``state->mode`` to ``NETWORKING_MODE_NOT_APPLICABLE`` and
 * ``state->interface`` to ``NULL``.
 *
 * @return esp_err_t This function always returns ``ESP_OK``, all potentially
 *                   failing calls are catched and silenced, though log
 *                   messages of level ``ERROR`` and ``DEBUG`` are emitted.
 */
static esp_err_t wifi_sta_deinit(void) {
    ESP_LOGV(TAG, "wifi_sta_deinit()");

    if (state->mode == NETWORKING_MODE_NOT_APPLICABLE) {
        ESP_LOGE(TAG, "WiFi is not initialized!");
        ESP_LOGD(TAG, "Current WiFi mode is %d", state->mode);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_err_t esp_ret = esp_wifi_stop();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not stop WiFi (station mode)!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_stop()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
        ESP_LOGW(TAG, "Continuing with de-initialization...");
    }

    esp_netif_destroy_default_wifi(state->interface);
    state->interface = NULL;

    state->mode = NETWORKING_MODE_NOT_APPLICABLE;

    return ESP_OK;
}

/**
 * Actually connect to a WiFi access point.
 *
 * This function is called from ::networking to perform the actual connect.
 *
 * Please note: This function simply wraps **ESP-IDF**'s ``esp_wifi_connect()``
 * to catch internal errors. It does not actually track the success of the
 * connection. This has to be done while handling the events raised by
 * **ESP-IDF**'s ``wifi`` module.
 */
static void wifi_sta_connect(void) {
    ESP_LOGV(TAG, "wifi_sta_connect()");

    ((struct medium_state_wifi_sta *)(state->medium_state))->num_connection_attempts++;  // NOLINT(whitespace/line_length)

    esp_err_t esp_ret = esp_wifi_connect();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Connect command failed!");
        ESP_LOGD(
            TAG,
            "'esp_wifi_connect()' returned %s [%d]",
            esp_err_to_name(esp_ret),
            esp_ret);
    }
}

/**
 * Return the number of failed connection attempts.
 *
 * @return int8_t The number of failed connection attempts.
 */
static int8_t wifi_sta_get_num_connection_attempts(void) {
    return ((struct medium_state_wifi_sta *)(state->medium_state))->num_connection_attempts;  // NOLINT(whitespace/line_length)
}

/**
 * Reset the number of failed connection attempts.
 *
 */
static void wifi_sta_reset_connection_counter(void) {
    ((struct medium_state_wifi_sta *)(state->medium_state))->num_connection_attempts = 0;  // NOLINT(whitespace/line_length)
}

/**
 * Starts a WiFi connection.
 *
 * This either connects to a given WiFi network, if credentials are available
 * in the given ``nvs_namespace`` and the specified network is reachable, or
 * launches the local access point.
 *
 * @param nvs_namespace The NVS namespace to read values from.
 * @return esp_err_t    ``ESP_OK`` on success, ``ESP_FAIL`` on failure.
 */
static esp_err_t wifi_start(char *nvs_namespace) {
    ESP_LOGV(TAG, "wifi_start()");

    if (wifi_init(nvs_namespace) != ESP_OK) {
        wifi_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}
