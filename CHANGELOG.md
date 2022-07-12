# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- ``ESP-IDF``/``ESP-ADF`` are integrated into the repository (Git submodule): [#25](https://github.com/Mischback/krachkiste_esp32/issues/25)
  - actually only the ``ADF`` is specified as Git submodule, as this embeds a
    given ``IDF`` (as another Git submodule)
  - ``ESP-IDF`` and ``ESP-ADF`` are integrated into the repository's
    ``Makefile`` and ``ESP-IDF``'s relevant management commands (as provided by
    ``idf.py`` are exposed as ``make`` recipes)
  - integration into VSCode is provided using VSCode's ``tasks.json``
  - optional integration by using VSCode's extension ``fabiospampinato.vscode-commands``



## 0.1.0-alpha

### Added

- Basic, WiFi-based networking implemented (``mnet32``, [#5](https://github.com/Mischback/krachkiste_esp32/issues/5))
  - ESP32 launches Access Point, if there are no (stored) WiFi credentials or
    the (stored) WiFi network is unreachable
  - ESP32 connects to available (stored) WiFi network
  - minimal web interface to configure WiFi credentials
  - contribution to binary (total): 7675 bytes

<!--
### Added
### Changed
### Deprecated
### Removed
### Fixed
### Security
-->
