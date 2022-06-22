Abstract
========

This component implements basic networking functionality using the WiFi
capabilities of **ESP-IDF** in pure **C**.

It is meant to be reusable and provides a very minimal (public) interface for
integration in different **ESP32** projects.

It establishes and maintains the connection to a WiFi network, provides an
internal access point for fallback purposes, integrates into a project's
http server implementation and exposes most of its configuration options by
**ESP-IDF**'s ``menuconfig`` or by using the web interface while running.


WiFi Provisioning
=================

The component stores the credentials for the WiFi to be used on the **ESP32**'s
internal non-volatile storage (NVS).

If the specified network is not reachable (with a configurable number of
retries), the component launches an internal access point (with configurable
credentials) to make the configuration interface available.

If there are no credentials found, the access point is started immediatly.

**Please note** The component does not include an HTTP server implementation.
The project has to provide this functionality (e.g. by another component) in
order to make the web interface accessible.


Developer's Note
================

This component is implemented as **ESP-IDF** component while developing a web
radio player based on the **ESP32**,
`KrachkisteESP32 <https://github.com/Mischback/krachkiste_esp32>`_.

It is meant to be as pluggable as possible, but most likely it will require some
hacking to integrate it with other **ESP32**-based projects.
