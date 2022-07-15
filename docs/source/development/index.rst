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

- Linters
- Continuous Integration (GitHub Actions)
- ``pre-commit``
- Documentation Generation

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
