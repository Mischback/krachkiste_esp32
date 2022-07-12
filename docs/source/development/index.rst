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

- **ESP-IDF**
- **ESP-ADF**
- Python-based utilities


Code Quality Tools
------------------

- Linters
- Continuous Integration (GitHub Actions)
- ``pre-commit``
- Documentation Generation
