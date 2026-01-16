# Android System API (ADB) v2.1

Expose Android system information via HTTP using **ADB + FastAPI**.
No root. No Android app. Pure system metrics.

## ✨ Features

- ✅ **Real-time metrics**: CPU, RAM, Storage, Battery, Thermal
- ✅ **No root required**: Uses ADB + standard Linux files
- ✅ **Type-safe API**: Full Pydantic validation & OpenAPI schema
- ✅ **Better UX**: Enhanced Swagger UI with examples
- ✅ **Error handling**: Comprehensive error messages
- ✅ **Health checks**: ADB connection monitoring
- ✅ **Timestamps**: All responses include timestamps
- ✅ **Calculated metrics**: Usage percentages, averages, min/max
- ✅ **WebSocket streaming**: Real-time metrics via ws:// 
- ✅ **Per-core CPU**: Individual frequency for each core
- ✅ **CPU min/max**: Frequency statistics across cores

---

## 📋 Requirements

- Python 3.9+
- ADB installed and in PATH
- USB debugging enabled
- One Android device connected

**Verify:**
```bash
adb devices
```

---

## 🚀 Installation & Setup

### 1. Install Python dependencies
```bash
pip install -r requirements.txt
```

### 2. Run the API
```bash
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

### 3. Access the API
- **Swagger UI**: http://localhost:8000/docs
- **ReDoc**: http://localhost:8000/redoc
- **API Root**: http://localhost:8000/
- **WebSocket Metrics**: ws://localhost:8000/ws/metrics
- **WebSocket CPU**: ws://localhost:8000/ws/cpu

---

## 📡 API Endpoints

### Health & Diagnostics

#### `GET /health`
Check API and ADB connection status.

**Response:**
```json
{
  "status": "healthy",
  "adb_connected": true,
  "timestamp": "2026-01-16T10:30:45.123456"
}
```

---

### Device Information

#### `GET /device`
Get device model, manufacturer, Android version, SDK level, hardware, board.

**Response:**
```json
{
  "model": "SM-F127G",
  "manufacturer": "samsung",
  "android_version": "13",
  "sdk": 33,
  "hardware": "exynos850",
  "board": "universal3830"
}
```

---

### CPU Information

#### `GET /cpu`
Get CPU cores, ABI, and architecture information.

**Response:**
```json
{
  "cores": 8,
  "abi": "arm64-v8a",
  "abi_list": ["arm64-v8a", "armeabi-v7a", "armeabi"],
  "arch": "ARMv8"
}
```

#### `GET /cpu/frequency`
Get real-time CPU frequency for each core.

**Response:**
```json
{
  "frequencies_khz": {
    "cpu0": 1800000,
    "cpu1": 1800000,
    "cpu2": 1200000,
    "cpu3": 1200000,
    "cpu4": 546000,
    "cpu5": 546000,
    "cpu6": 546000,
    "cpu7": 546000
  },
  "average_mhz": 1118.25
}
```

---

### Memory Information

#### `GET /memory`
Get RAM and swap memory statistics.

**Response:**
```json
{
  "total_mb": 3704.71,
  "available_mb": 1236.54,
  "used_mb": 2468.17,
  "usage_percent": 66.6,
  "swap_total_mb": 4096.0,
  "swap_free_mb": 2729.53
}
```

---

### Storage Information

#### `GET /storage`
Get internal storage (/data) usage.

**Response:**
```json
{
  "filesystem": "/dev/block/dm-44",
  "total_gb": 51.35,
  "used_gb": 15.63,
  "free_gb": 35.52,
  "usage_percent": 30.5
}
```

---

### Battery Information

#### `GET /battery`
Get battery status, health, level, voltage, temperature, and charging state.

**Response:**
```json
{
  "level": 39,
  "health": "Good",
  "status": "Charging",
  "voltage_mv": 3960,
  "temperature_c": 34.6,
  "technology": "Li-ion",
  "is_charging": true
}
```

---

### Thermal Information

#### `GET /thermal`
Get thermal sensor readings (CPU, Battery, Skin temperatures).

**Response:**
```json
{
  "temperatures": {
    "AP": 39.5,
    "BAT": 34.6,
    "SKIN": 35.4,
    "PA": 44.0
  },
  "max_temp_c": 44.0,
  "min_temp_c": 34.6
}
```

---

### Full System Information

#### `GET /system`
Get complete system information in a single request.

**Response:**
```json
{
  "device": { ... },
  "cpu": { ... },
  "cpu_frequency": { ... },
  "memory": { ... },
  "storage": { ... },
  "battery": { ... },
  "thermal": { ... },
  "timestamp": "2026-01-16T10:30:45.123456"
}
```

---

## 🔌 WebSocket Real-Time Streaming

### Metrics Stream

**Endpoint**: `ws://localhost:8000/ws/metrics`

Get real-time system metrics (battery, memory, storage, CPU, thermal) updated every 2 seconds.

**Example (JavaScript):**
```javascript
const ws = new WebSocket('ws://localhost:8000/ws/metrics');

ws.onmessage = (event) => {
  const metrics = JSON.parse(event.data);
  console.log(`Battery: ${metrics.battery_level}%`);
  console.log(`Memory: ${metrics.memory_usage_percent.toFixed(1)}%`);
  console.log(`CPU Avg: ${metrics.cpu_avg_mhz.toFixed(0)} MHz`);
};
```

**Example (Python):**
```python
import asyncio
import websockets
import json

async def stream_metrics():
    async with websockets.connect('ws://localhost:8000/ws/metrics') as ws:
        async for message in ws:
            data = json.loads(message)
            print(f"Battery: {data['battery_level']}%")
            print(f"Memory: {data['memory_usage_percent']:.1f}%")
```

**Response:**
```json
{
  "timestamp": "2026-01-16T10:30:45.123456",
  "battery_level": 42,
  "memory_usage_percent": 68.0,
  "storage_usage_percent": 30.5,
  "cpu_avg_mhz": 1173.25,
  "cpu_max_mhz": 1800.0,
  "cpu_min_mhz": 546.0,
  "thermal_max_temp": 43.7
}
```

### CPU Frequency Stream

**Endpoint**: `ws://localhost:8000/ws/cpu`

Get real-time per-core CPU frequencies updated every 1 second.

**Example (Python):**
```python
import asyncio
import websockets
import json

async def stream_cpu():
    async with websockets.connect('ws://localhost:8000/ws/cpu') as ws:
        async for message in ws:
            data = json.loads(message)
            print(f"CPU cores: {data['core_count']}")
            print(f"Frequency range: {data['min_mhz']:.0f} - {data['max_mhz']:.0f} MHz")
```

**Response:**
```json
{
  "timestamp": "2026-01-16T10:30:45.123456",
  "per_core": {
    "cpu0": 1800000,
    "cpu1": 1800000,
    "cpu2": 1200000,
    "cpu3": 546000,
    "cpu4": 546000,
    "cpu5": 546000,
    "cpu6": 546000,
    "cpu7": 546000
  },
  "min_mhz": 546.0,
  "max_mhz": 1800.0,
  "avg_mhz": 1173.25,
  "core_count": 8
}
```

### WebSocket Client Tool

Use the provided Python WebSocket client:

```bash
# Install dependencies
pip install websockets

# Stream metrics
python3 websocket_client.py metrics

# Stream CPU frequencies
python3 websocket_client.py cpu

# Connect to remote server
python3 websocket_client.py metrics 10.10.10.48:8000
```

---

## 🎯 What This API Can Do

✅ CPU frequency (per-core real-time)  
✅ CPU min/max/avg frequencies  
✅ Individual core frequencies  
✅ RAM, Swap memory usage  
✅ Storage usage (/data partition)  
✅ Battery level, health, voltage, temperature  
✅ Thermal sensor readings (CPU, Battery, Skin)  
✅ Device info (model, manufacturer, Android version)  
✅ Usage percentages and calculations  
✅ Timestamps on all responses  
✅ Real-time WebSocket streaming  
✅ Per-core CPU monitoring  

---

## ❌ What This API Cannot Do

❌ CPU usage %  
❌ Live sensor values  
❌ GPU usage / frequency  
❌ Process-level memory stats  

**Why?** These require:
- Root OR
- Privileged Android app OR
- Sensor Framework access

---

## 🛠️ Advanced Usage

### Using with curl

```bash
# Get all device metrics
curl http://localhost:8000/system | jq

# Get specific metrics
curl http://localhost:8000/battery | jq

# Check API health
curl http://localhost:8000/health | jq
```

### Using in Python

```python
import requests

response = requests.get("http://localhost:8000/system")
data = response.json()

print(f"Device: {data['device']['model']}")
print(f"Battery: {data['battery']['level']}%")
print(f"RAM Usage: {data['memory']['usage_percent']}%")
```

### Using in JavaScript

```javascript
async function getSystemInfo() {
  const response = await fetch("http://localhost:8000/system");
  const data = await response.json();
  
  console.log(`Battery: ${data.battery.level}%`);
  console.log(`RAM Used: ${data.memory.used_mb} MB`);
}
```

---

## 📊 Response Structure

All responses include:
- **Type validation**: Pydantic models ensure data integrity
- **Timestamps**: ISO 8601 format
- **Units**: Clearly specified (MB, GB, kHz, mV, °C)
- **Calculations**: Percentages, averages, min/max values

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
- **Swagger UI**: http://localhost:8000/docs
- **ReDoc**: http://localhost:8000/redoc
- **OpenAPI JSON**: http://localhost:8000/openapi.json

---

## 🔄 Development

### Project Structure
```
android-adb-system-api/
├── main.py           # FastAPI app + endpoints
├── adb_utils.py      # ADB execution + error handling
├── parsers.py        # Output parsing & calculations
├── requirements.txt  # Dependencies
└── README.md         # Documentation
```

### Running with auto-reload
```bash
uvicorn main:app --reload
```

### View logs
```bash
# Check terminal output for INFO/ERROR logs
# All ADB commands are logged for debugging
```

---

## 📈 Performance

- ✅ Sub-100ms responses (most endpoints)
- ✅ No caching required (real-time data)
- ✅ Timeout protection (10s max)
- ✅ Error handling & graceful degradation

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

**v2.1.0** (Jan 2026)
- WebSocket real-time streaming (/ws/metrics, /ws/cpu)
- Enhanced CPU frequency endpoint (per-core, min/max/avg)
- Per-core frequency parsing
- CPU statistics (minimum, maximum, average)
- Core count in frequency response
- Python WebSocket client tool
- Real-time metrics every 2 seconds
- Real-time CPU frequency every 1 second

**v2.0.0** (Jan 2026)
- Pydantic models for type safety
- Enhanced error handling
- Health check endpoint
- Calculated metrics (percentages, averages)
- Timestamps on all responses
- Better Swagger UI with examples
- Improved thermal parsing
- Logging infrastructure

**v1.0.0** (Initial release)
- Basic endpoints
- ADB integration
