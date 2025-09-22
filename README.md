# ESP32 with FreeRTOS on ESP-IDF

![ESP32](https://img.shields.io/badge/ESP32-FreeRTOS-blue)
![Language](https://img.shields.io/badge/Language-C-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## Overview

This repository is a personal learning project for **ESP32 development with FreeRTOS using ESP-IDF**. It contains example code, experiments, and notes to help understand:

- FreeRTOS concepts on ESP32
- Task creation, scheduling, and synchronization
- ESP32 peripherals (GPIO, I2C, SPI)
- Project structure in ESP-IDF
- Integration of external components

This repository is intended for educational purposes and hands-on practice.

---

## Getting Started

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed
- Python 3.7+
- Git

### Build and Flash

1. Clone the repository:

```bash
git clone https://github.com/yourusername/ESP32-with-FreeRTOS-on-ESP-IDF.git
cd ESP32-with-FreeRTOS-on-ESP-IDF
```
2. Set up the ESP-IDF environment

```bash
idf.py set-target esp32
. $IDF_PATH/export.sh   # Linux/macOS
# . $IDF_PATH\export.bat  # Windows
```
3. Build, flash, and monitor
```bash
idf.py build
idf.py -p <PORT> flash monitor
```
