# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# The project does not follow the pre-defined directory structure of the ESP-IDF
# framework. Thus, the actual "components" of the project have to be specified
# manually, including the project's "main" component ("src/app") and
# project-specific libraries ("src/lib").
# See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#renaming-main-component
set(EXTRA_COMPONENT_DIRS "src/app src/lib")

# By default, all available components are built, including all components of
# ESP-IDF and the project.
# By trimming this down, CMake will actually resolve dependencies on components
# during the build process, allowing to only build the components that are
# actually used, minimizing build time.
# This relies on ``REQUIRES`` and ``PRIV_REQUIRES`` in the actual components.
# See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#optional-project-variables
# and https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#component-requirements
#
# Best case: It is sufficient to specify just the project's main component,
# which is "app" in "src/app" (see EXTRA_COMPONENT_DIRS above).
set(COMPONENTS "app")

# This pulls in the ESP-IDF CMake stuff to let the magic happen.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# The actual name of the project
project(KrachkisteESP32)
