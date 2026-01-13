# blind-record

Accessible voice recorder (dictaphone) for blind/low-vision users, built on ESP32.

## Getting Started

### Prerequisites

- ESP-IDF v5.x
- Python 3.8+
- clang-format (for code formatting)

### Setup for Development

After cloning the repository, run:

```bash
pip install pre-commit
pre-commit install
```

This installs git hooks that automatically format C/C++ code before each commit.

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

This project follows the ESP-IDF (Espressif IoT Development Framework) standard structure:

### 📁 Directory Organization

#### `components/`

Contains **reusable components** of the project:

- Each component is a separate folder with its own `CMakeLists.txt`
- Modular libraries (drivers, utilities, middleware)
- Code that can be reused across different projects
- **Example**: `components/my_driver/`, `components/wifi_manager/`

#

#### `include/`

Contains **public header files (.h)**:

- APIs exported to other modules
- Interfaces for other components
- Public data structures and constants
- Must have include guards or `#pragma once`

#### `main/`

**Main application component**:

- Entry point (`app_main()`)
- Core business logic of the application
- Has its own `CMakeLists.txt`
- Private headers can be stored here
- **Required directory** for ESP-IDF projects

#### `src/`

Not standard for ESP-IDF, but can be used for:

- Alternative code organization (non-ESP-IDF standard)
- Subfolder within `main/` or `components/`
- Generally, code is placed in `main/` or `components/` instead

### 🎯 Recommended Structure

```
blind-record/
├── components/          # Reusable modules
│   ├── audio_codec/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── audio_codec.h
│   │   └── audio_codec.c
│   └── storage/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── storage.h
│       └── storage.c
├── main/                # Main application
│   ├── CMakeLists.txt
│   ├── main.c          # app_main() goes here
│   └── config.h        # Private headers
└── CMakeLists.txt       # Root build file
```
