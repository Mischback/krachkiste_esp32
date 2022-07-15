#################
Development Setup
#################

**KrachkisteESP32** is available as open source software (
`MIT License <https://choosealicense.com/licenses/mit/>`_) and available
`@GitHub <https://github.com/Mischback/krachkiste_esp32>`_.

It is based on
`Espressif's ESP32 <https://www.espressif.com/en/products/socs/esp32>`_ micro
controller, which integrates a capable CPU with integrated WiFi and Bluetooth
capabilities.

Software-wise, this project is based on Espressif's
`Audio Development Framework (ADF) <https://github.com/espressif/esp-adf>`_ and
`IoT Development Framework (IDF) <https://github.com/espressif/esp-idf>`_.


***************
Getting Started
***************

The project tries to be as self-contained as possible, meaning it is intended
to setup a *development-ready* environment by itsself. However, some
pre-requisites have to be provided before working with the repository, see the
following step-by-step guide.

.. warning::
    The project is geared towards a *Linux-based* development environment and
    **does not support** Windows-based development machines out of the box.

It is highly recommended to read Espressif's documentation on how to
`get started with ESP-ADF <https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start>`_
and
`get started with ESP-IDF <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html>`_.


Repository Setup
================

(Embedded) Development Frameworks
---------------------------------

- **ESP-IDF**: Espressif's IoT Development Framework, which provide the baseline
  for any development of *ESP32* microcontrollers. It is actually structured in
  several different *components* that are included in the final build as
  required. It also provides the general toolchain for compiling a codebase for
  different *ESP32s*.
- **ESP-ADF**: Espressif's Audio Development Framework is provided to facilitate
  the development of audio applications based on *ESP32* microcontrollers. It is
  itsself based on *ESP-IDF* and uses the compilation toolchain provided by
  *ESP-IDF*. A compatible version/release of *ESP-IDF* is embedded in the *ADF*.
- **Python-based utilities**: The repository makes used of several Python-based
  tools, i.e. for linting the code base or generating the documentation. To
  circumvent a pollution of the development system, these tools are provided
  using a virtual environment (Python ``venv``) that is running ``tox``, which
  manages these tools.


Code Quality Tools
------------------

- **Formatters/Linters**: Several linters are prepared to be used on the
  codebase. The recommended way is to actually use ``pre-commit`` as described
  below to run the linters automatically during development. The following
  linters are included:

  - ``clang-format`` (automatically format ``C`` source files. A custom
    configuration is provided in ``.clang-format``, which is heavily based on
    *Google's C Style*)
  - ``cpplint`` (*Google's* linter for C/C++ codebases)
  - ``cppcheck`` (another linter for C/C++ codebases; **Needs to be installed
    locally**)
  - ``cmake-lint`` (linter for *CMake-related* code, including
    ``CMakeLists.txt``; configuration provided in ``.cmakelintrc``)
  - ``black`` (automatically format Python source files)
  - ``isort`` (automatically sort imports in Python source files; configuration
    provided in ``pyproject.toml``)
  - ``flake8`` (linter for Python source files; configuration provided in
    ``.flake8``)

- **Continuous Integration (GitHub Actions)**: There is a workflow in place to
  actually run the linters and build the documentation. This is mainly done to
  ensure a minimum coding standard for the repository.

  The workflow does perform an actual build of the project aswell, but this is
  not an actual integration testing approach (as there is no test suite in
  place), but may be considered another step to ensure code quality / the
  ability to compile the project.
- **pre-commit**: To make a long story short: Activate ``pre-commit`` and never
  forget to run linters before committing. This is a vital part in my own
  development workflow and really helps against messy code.

  ``pre-commit`` is set up to run the formatters and linters as described above
  (and some minor additional checks) against the code base on every commit.
- **Documentation Generation**: The documentation you're reading right now is
  generated using ``Sphinx``. However, ``Sphinx`` is mainly geared toward a
  Python codebase, so in order to make it shine with our **C** code, a
  toolchain is in place to extract the actual source documentation using
  ``doxygen`` and then make it usable using ``breathe`` (a ``Sphinx`` plugin).


Run Common Task using ``make``
------------------------------

.. note::
    *ESP-IDF*'s compilation toolchain is built around ``CMake`` and ``ninja``
    and is left mostly untouched in this repository. However, you may have
    noticed a ``Makefile`` in the repository root.

*Primarily*, ``make`` is used as a task runner. *ESP-IDF*'s compilation
toolchain requires the activation of a specific Python virtual environment and
the provision of certain environment variables, including the manipulation of
the system's ``PATH`` (if you have followed **ESP-IDF**'s Getting started guide,
this is what ``export.sh`` ensures).

On the other hand, the internal tools of the repository are provided using a
virtual environment, too. The ``Makefile`` ensures the activation of the
required environment and then runs the requested tools.

.. warning::
    As of now, not all possible commands of the several tools are actually
    exposed through the ``Makefile``, e.g. *Espressif*'s ``idf.py`` is the main
    interface to *ESP-IDF's* toolchain, but only a subset of the available
    commands is exposed.

    If a specific command is required, you can either add it to the ``Makefile``
    with the given internal logic (this is the recommended way) **or** you can
    simply activate *ESP-IDF's* required environment in the usual way, by
    sourcing ``export.sh`` in your shell.

*Internally*, ``make``'s capabilities as a *build system* are used to ensure the
setup of the tools and environments.
