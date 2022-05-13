
# minify HTML files of the component
include(${PROJECT_DIR}/tools/cmake/minimizer.cmake)

minimizeHTML(src/wifi_config.src.html src/wifi_config.html)

# Register this as an ESP-IDF component
# For details on REQUIRES/PRIV_REQUIRES see
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-requirements
# Please note: several ESP-IDF components are explicitly listed here, though
# they are included by default, see
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#common-component-requirements
idf_component_register(
  SRCS "src/mnet32.c" "src/mnet32_nvs.c" "src/mnet32_state.c" "src/mnet32_web.c" "src/mnet32_wifi.c" "src/wifi_config.html"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES "esp_common esp_event esp_http_server esp_netif esp_wifi log nvs_flash"
  EMBED_TXTFILES "src/wifi_config.html"
)
