# Register this as an ESP-IDF component
# For details on REQUIRES/PRIV_REQUIRES see
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-requirements
# Please note: several ESP-IDF components are explicitly listed here, though
# they are included by default, see
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#common-component-requirements
idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "."
  PRIV_REQUIRES "esp_event log min_httpd nvs_flash embedded_networking_esp32"
)
