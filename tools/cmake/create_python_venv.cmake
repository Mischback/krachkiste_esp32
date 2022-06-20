# SPDX-FileCopyrightText: 2022 Mischback
# SPDX-License-Identifier: MIT
# SPDX-FileType: SOURCE
#
# Create a Python virtual environment.
#
# WARNING: This function may only be called one time for any given TARGET!
#
# @param TARGET            The name of the virtual environment
# @param REQUIREMENTS_FILE Specify a requirements file to be installed by pip
# @param OUT_PYTHON_EXE    Variable to store the path of the Python executable
# @param OUT_VENV_DIR      Variable to store the path of the virtual environment
# @param REQUIREMENTS      Specify requirements one by one (may be specified
#                          multiple times)
#
# This is actually based on a blog post by Charis Marangos
# (https://schemingdeveloper.com/2020/07/02/how-to-create-a-new-python-virtual-environment-in-cmake/),
# slightly stripped down to the actual needs of the project.
function(create_python_venv TARGET)

  # Configure keyword arguments
  set(KEYWORD_ARGS REQUIREMENTS_FILE OUT_PYTHON_EXE OUT_BIN_DIR OUT_VENV_DIR)
  set(MULTI_ARGS SOURCES REQUIREMENTS)
  # Actually parse the arguments
  cmake_parse_arguments(ARG "" "${KEYWORD_ARGS}" "${MULTI_ARGS}" ${ARGN})

  # Retrieve the system's Python executable.
  # Will be used to create the virtual environment using Python's ``venv``
  # module.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  # Determine the full path of the virtual environment directory.
  # The virtual environment will actually be created in the project's build
  # directory.
  set(VENV_FULL_PATH ${CMAKE_BINARY_DIR}/${TARGET})

  # At least try to work "cross platform". Python's ``venv`` module has subtle
  # differences on Windows.
  if (WIN32)
    set(BIN_DIR ${VENV_FULL_PATH}/Scripts)
    set(PYTHON ${BIN_DIR}/python.exe)
  else ()
    set(BIN_DIR ${VENV_FULL_PATH}/bin)
    set(PYTHON ${BIN_DIR}/python)
  endif ()

  # Parse the provided requirements and prepare the installation command.
  if (ARG_REQUIREMENTS_FILE)
    set(REQUIREMENTS -r ${ARG_REQUIREMENTS_FILE})
  endif()

  set(REQUIREMENTS ${REQUIREMENTS} "${ARG_REQUIREMENTS}")

  if (REQUIREMENTS)
    set(INSTALL_CMD ${BIN_DIR}/pip install --disable-pip-version-check)
    set(INSTALL_CMD ${INSTALL_CMD} ${REQUIREMENTS})
  else()
    set(INSTALL_CMD "")
  endif()

  # This is the actual creation of the virtual environment.
  # It uses a custom target and two custom commands:
  #   - the custom target will be identified by the provided TARGET and may be
  #     used to indicate a dependency on the virtual environment for other
  #     "rules";
  #   - the first custom command creates the virtual environment and has the
  #     ``pyvenv.cfg`` file as its output; this file is automatically created
  #     by Python's ``venv`` module;
  #   - the second custom command installs the requirements into the virtual
  #     environment; to track the success of this operation, ``pip freeze`` is
  #     used to create an output file.
  #
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
    set(CFG_FILE ${VENV_FULL_PATH}/pyvenv.cfg)
    add_custom_command(
      OUTPUT ${CFG_FILE}
      COMMAND ${Python3_EXECUTABLE} -m venv ${VENV_FULL_PATH}
    )
    set(OUTPUT_FILE ${VENV_FULL_PATH}/environment.txt)
    add_custom_command(
      OUTPUT ${OUTPUT_FILE}
      COMMAND ${INSTALL_CMD}
      COMMAND ${BIN_DIR}/pip freeze > ${OUTPUT_FILE}
      DEPENDS ${CFG_FILE} ${ARG_SOURCES} ${ARG_REQUIREMENTS_FILE}
    )

    add_custom_target(${TARGET} DEPENDS ${OUTPUT_FILE})
  endif()

  # Make some relevant things available to the parent/calling scope. Which of
  # these things will be available is determined with (optional) parameters.
  if (ARG_OUT_PYTHON_EXE)
    set(${ARG_OUT_PYTHON_EXE} ${PYTHON} PARENT_SCOPE)
  endif ()

  if (ARG_OUT_BIN_DIR)
    set(${ARG_OUT_BIN_DIR} ${BIN_DIR} PARENT_SCOPE)
  endif ()

  if (ARG_OUT_VENV_DIR)
    set(${ARG_OUT_VENV_DIR} ${VENV_FULL_PATH} PARENT_SCOPE)
  endif ()

endfunction()
