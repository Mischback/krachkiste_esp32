#########################
embedded_networking_esp32
#########################

*******************
General Description
*******************

**********
Public API
**********

Defines
=======

.. doxygendefine:: MNET32_NVS_NAMESPACE

.. doxygendefine:: MNET32_TASK_PRIORITY

.. doxygendefine:: MNET32_TASK_MONITOR_FREQUENCY

.. doxygendefine:: MNET32_WEB_URL_CONFIG

.. doxygendefine:: MNET32_WIFI_AP_CHANNEL

.. doxygendefine:: MNET32_WIFI_AP_MAX_CONNS

.. doxygendefine:: MNET32_WIFI_AP_LIFETIME

.. doxygendefine:: MNET32_WIFI_AP_PSK

.. doxygendefine:: MNET32_WIFI_AP_SSID

.. doxygendefine:: MNET32_WIFI_STA_MAX_CONNECTION_ATTEMPTS

.. doxygendefine:: MNET32_WIFI_STA_THRESHOLD_AUTH

.. doxygendefine:: MNET32_WIFI_STA_THRESHOLD_RSSI


Functions
=========

.. doxygenfunction:: mnet32_start

.. doxygenfunction:: mnet32_stop

.. doxygenfunction:: mnet32_web_attach_handlers


************
Internal API
************

Internally, the component is split into several modules (combinations of source
and **internal** header files).

All of these modules are documented in the source code.
