
# minify HTML files of the component
include(${PROJECT_DIR}/tools/cmake/minimizer.cmake)

minimizeHTML(home.src.html home.html)

# Register this as an ESP-IDF component
# For details on REQUIRES/PRIV_REQUIRES see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-requirements
# Please note: several ESP-IDF components are explicitly listed here, though
# they are included by default, see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#common-component-requirements
idf_component_register(SRCS "min_httpd.c" "home.html"
                       INCLUDE_DIRS "."
                       REQUIRES "esp_common esp_http_server"
                       PRIV_REQUIRES "esp_event")
