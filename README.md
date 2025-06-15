# WiCA Cellular Automaton Simulator

Read in [Chinese](doc/README/zh_CN.md)

## Project Description

WiCA is a Cellular Automaton simulation program built using C++ and SDL2.

## Key Features

* **Rule Loading:** Reads state space and neighborhood definitions from JSON configuration files.
* **Plugin Support:** Rule logic can be implemented as shared libraries (DLLs).

## System Requirements

* CMake (version 3.16 or higher)
* SDL2, SDL2_image, SDL2_ttf
* nlohmann/json
* spdlog
* fmt
* A C++20 compatible compiler (MSVC, GCC, Clang)

## Commands and Controls

For a complete list of commands and keyboard shortcuts, please refer to the in-application help (`/help` command).
* **Space:** Toggle simulation pause
* **Esc:** Exit program or close command input
* **Mouse Wheel:** Zoom
* **Middle Mouse Drag:** Pan the view
* **Left Mouse:** Apply current brush state