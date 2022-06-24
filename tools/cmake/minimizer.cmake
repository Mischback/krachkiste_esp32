# SPDX-FileCopyrightText: 2022 Mischback
# SPDX-License-Identifier: MIT
# SPDX-FileType: SOURCE

set(MINIMIZER_SCRIPT_DIR ${PROJECT_DIR}/tools/minimizer)
set(MINIMIZER_SCRIPT_HTML ${MINIMIZER_SCRIPT_DIR}/html.py)
set(MINIMIZER_STAMP ${CMAKE_BINARY_DIR}/minimizer.stamp)

# Minimize HTML source code with minify-html.
#
# Basically this is a wrapper around CMake's ``add_custom_command``, calling
# the project's custom ``tools/minimizer/html.py`` script, with is just a thin
# wrapper around ``minify-html``.
#
# The script will be run from a dedicated virtual environment
# ``minimizer_venv``.
#
# @param MIN_SOURCE      The filename of the source (HTML) file
# @param MIN_DESTINATION The filename of the minified result
function(minimize_html MIN_SOURCE MIN_DESTINATION)
  # Please note that this whole block is wrapped in an ``if()`` statement.
  #
  # Though the referenced variable starts with ``CMAKE_``, it is not a CMake
  # feature, but provided/set/used by **ESP-IDF**'s build process.
  #
  # During "early expansion" all of the project's ``CMakeLists.txt`` files
  # are processed to fetch a list of all components to be build. During this
  # process, no custom targets/commands should be provided (see
  # https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/build-system.html#enumeration and
  # https://github.com/espressif/esp-idf/issues/4540)
  if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MIN_DESTINATION}
      COMMAND ${MINIMIZER_PYTHON} ${MINIMIZER_SCRIPT_HTML} ${CMAKE_CURRENT_SOURCE_DIR}/${MIN_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/${MIN_DESTINATION}
      DEPENDS ${MINIMIZER_STAMP} ${CMAKE_CURRENT_SOURCE_DIR}/${MIN_SOURCE}
    )
  endif()
endfunction()

# The following block is used to create and populate the required Python
# virtual environment.
#
# As it creates a custom target, this block may be executed only once during the
# build process. It is guarded against multiple execution by setting/checking a
# global CMake variable ``MINIMIZER_PYTHON``.
#
# This variable is in fact more than just a tracker, but it is actively used in
# the function ``minimizeHTML`` (defined above!) to access the virtual
# environment's Python executable.
if(NOT MINIMIZER_PYTHON)
  include(${PROJECT_DIR}/tools/cmake/create_python_venv.cmake)

  create_python_venv(
    minimizer_venv
    REQUIREMENTS_FILE ${PROJECT_DIR}/requirements/python/minimizer.txt
    OUT_PYTHON_EXE VENV_PYTHON_EXE
  )

  set(
    MINIMIZER_PYTHON
    ${VENV_PYTHON_EXE}
    CACHE INTERNAL
    "make minimizer's python available"
    FORCE)

  if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    add_custom_command(
        OUTPUT ${MINIMIZER_STAMP}
        COMMAND cmake -E echo "stamp" > ${MINIMIZER_STAMP}
        DEPENDS minimizer_venv ${MINIMIZER_SCRIPT_HTML}
    )

    add_custom_target(
      minimizer_init
      DEPENDS ${MINIMIZER_STAMP}
    )
  endif()
endif()
