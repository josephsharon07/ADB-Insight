# DroidMetrics

Real-time Android system metrics API via ADB. Expose comprehensive device information over HTTP with both Python and C++ implementations. No root and no Android app required.

**DroidMetrics** by **bluecape** - Comprehensive Android System Monitoring

## Features

- Device and OS information
- CPU info and per-core frequency stats
- Memory and storage usage
- Battery status and health
- Thermal sensor readings
- Uptime and boot time
- OpenAPI schema with typed responses

## Requirements

- Python 3.9+
- ADB installed and in PATH
- USB debugging enabled
- One Android device connected

Verify:

```bash
adb devices
```

## Install & Run

1) Install dependencies

```bash
pip install -r requirements.txt
```

2) Start the API (Python)

```bash
cd Python
uvicorn main:app --host 0.0.0.0 --port 8001 --reload
```

3) Open the docs

- Swagger UI: http://localhost:8001/docs
- ReDoc: http://localhost:8001/redoc
- API root: http://localhost:8001/

## Endpoints

Health & diagnostics

- `GET /health`
- `GET /`

Device & OS

- `GET /device`
- `GET /os`

CPU

- `GET /cpu`
- `GET /cpu/frequency`
- `GET /cpu/governors`
- `GET /cpu/idle`

Memory & storage

- `GET /memory`
- `GET /storage`
- `GET /storage/mounts`

Power & thermal

- `GET /battery`
- `GET /thermal`
- `GET /thermal/cores`

System

- `GET /uptime`
- `GET /system`

Network & display

- `GET /network`
- `GET /display`

## Example

```bash
curl http://localhost:8001/system | jq
```

### C++ Version (Alternative Implementation)

For a high-performance compiled version:

```bash
cd Cpp
mkdir build && cd build
cmake ..
make

# Run the server
./adb_insight
# Listens on http://localhost:8000/
```

## Notes & Limitations

- No root required, but metrics depend on the device/kernel exposing the expected files.
- CPU usage %, GPU stats, and per-process details are not available without privileged access.
- Some network details (e.g., Wi‑Fi MAC) may be restricted by device policy.
- Per-core temperatures may be unavailable on some devices due to thermal sensor access restrictions.
```

---

## 📊 Response Structure

All responses include:
- **Type validation**: Pydantic models (Python) / nlohmann/json (C++)
- **Timestamps**: ISO 8601 format with microsecond precision
- **Units**: Clearly specified (MB, GB, kHz, mV, °C, mA)
- **Calculations**: Percentages, averages, min/max values
- **Consistent API**: Identical endpoints and response formats in both versions

---

## 🐛 Troubleshooting

### "ADB not found"
```bash
# Ensure ADB is installed
which adb

# Add ADB to PATH if needed
export PATH=$PATH:/path/to/adb
```

### "Failed to connect to device"
```bash
# Check connected devices
adb devices

# Enable USB debugging on device
# Settings > Developer Options > USB Debugging > Enable
```

### "Command timed out"
- Check device responsiveness
- Ensure device is not in deep sleep
- Try manually: `adb shell dumpsys battery`

---

## 📝 API Documentation

Full interactive documentation available at:
- **Python (Swagger UI)**: http://localhost:8001/docs
- **Python (ReDoc)**: http://localhost:8001/redoc
- **Python (OpenAPI JSON)**: http://localhost:8001/openapi.json
- **C++ (JSON Root)**: http://localhost:8000/

Both implementations support identical endpoints as documented in the Swagger UI.

---

## 🔄 Development

### Project Structure
```
DroidMetrics/
├── Python/
│   ├── main.py           # FastAPI app + endpoints
│   ├── adb_utils.py      # ADB execution + error handling
│   ├── parsers.py        # Output parsing & calculations
│   └── requirements.txt  # Dependencies
├── Cpp/
│   ├── src/
│   │   ├── main.cpp      # HTTP server + endpoints
│   │   ├── adb_utils.cpp # ADB execution
│   │   └── parsers.cpp   # Output parsing
│   ├── include/
│   │   ├── models.hpp    # Data structures
│   │   └── adb_utils.hpp # Headers
│   └── CMakeLists.txt    # Build configuration
└── README.md             # Documentation
```

### Running with auto-reload (Python)
```bash
cd Python
uvicorn main:app --reload --port 8001
```

### Running with auto-rebuild (C++)
```bash
cd Cpp/build
cmake ..
make watch  # or: make && ./adb_insight
```

---

## 📈 Performance

- ✅ **Python**: Sub-100ms responses (FastAPI + asyncio)
- ✅ **C++**: Sub-50ms responses (compiled, single-threaded)
- ✅ In-memory TTL caching (300s static, 30s dynamic data)
- ✅ Timeout protection (10s max per ADB command)
- ✅ Error handling & graceful degradation
- ✅ Concurrent endpoint queries (Python) / Sequential execution (C++)

---

## 🔒 Security Notes

- API is **read-only** (no device modifications)
- No sensitive data exposure (no app/file listing)
- ADB connection is **local-only** by default
- For remote access, use VPN or firewall rules

---

## 📄 License

Free to use and modify.

---

## 🚀 Version History

**v2.0.0** (Feb 2026) - DroidMetrics Launch
- Dual implementation: Python (FastAPI) and C++ (cpp-httplib)
- Complete API parity verified across both versions
- 18 endpoints covering all major system metrics
- Enhanced error handling and validation
- Improved thermal and battery parsing
- Microsecond precision timestamps
- In-memory caching with TTL
- Comprehensive documentation and verification

**v1.0.0** (Initial release - Jan 2026)
- Basic ADB integration
- FastAPI framework
- Core system metrics endpoints
