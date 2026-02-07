# Android System API (ADB) - C++ Version

A high-performance C++ implementation of the Android System API using ADB, with the same endpoints as the Python version.

## Features

- Fast native performance (C++17)
- Single-threaded HTTP server
- Full ADB command support
- JSON serialization via nlohmann/json
- In-memory caching with TTL
- All endpoints from Python version

## Requirements

- C++17 compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.10+
- ADB installed and in PATH
- USB debugging enabled
- One Android device connected

## Build

```bash
cd Cpp
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Run

```bash
./adb_insight
```

The API will be available at `http://0.0.0.0:8000`

## Endpoints

Same as Python version:

- `/health` - Health status
- `/device` - Device info
- `/os` - OS info
- `/cpu` - CPU info
- `/cpu/frequency` - CPU frequencies
- `/cpu/governors` - CPU governors
- `/cpu/idle` - CPU idle states
- `/memory` - Memory info
- `/storage` - Storage info
- `/storage/mounts` - Mount breakdown
- `/battery` - Battery info
- `/thermal` - Thermal info
- `/thermal/cores` - Core temperatures
- `/network` - Network info
- `/display` - Display info
- `/uptime` - Uptime info
- `/system` - Complete system info (all above)
- `/` - API root with endpoint list

## Performance

C++ version provides:
- Faster startup time
- Lower memory footprint
- Better CPU efficiency
- Native command execution

## Notes

- HTTP server is single-threaded (suitable for embedded/mobile dev environments)
- Caching TTLs match Python version
- Same ADB shell command compatibility
- JSON output format identical to Python version
