# ADB-Insight v2.1 - New Features

## 🚀 Enhanced CPU Metrics

### CPU Frequency Endpoint Improvements

**GET /cpu/frequency** now returns detailed per-core data:

```json
{
  "per_core": {
    "cpu0": 1800000,
    "cpu1": 1800000,
    "cpu2": 1200000,
    "cpu3": 1200000,
    "cpu4": 546000,
    "cpu5": 546000,
    "cpu6": 546000,
    "cpu7": 546000
  },
  "min_khz": 546000,
  "max_khz": 1800000,
  "min_mhz": 546.0,
  "max_mhz": 1800.0,
  "avg_mhz": 1173.25,
  "core_count": 8
}
```

### New Data Points

- **per_core**: Individual frequency for each CPU core in kHz
- **min_khz/min_mhz**: Minimum frequency across all cores
- **max_khz/max_mhz**: Maximum frequency across all cores
- **avg_mhz**: Average frequency across all cores
- **core_count**: Total number of cores

---

## 📡 WebSocket Real-Time Streaming

### Real-time Metrics Stream

**WebSocket URL**: `ws://localhost:8000/ws/metrics`

Sends system metrics every 2 seconds:

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

### Real-time CPU Frequency Stream

**WebSocket URL**: `ws://localhost:8000/ws/cpu`

Sends per-core CPU frequencies every 1 second:

```json
{
  "timestamp": "2026-01-16T10:30:45.123456",
  "per_core": {
    "cpu0": 1800000,
    "cpu1": 1800000,
    "cpu2": 1200000,
    "cpu3": 1200000,
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

---

## 💻 WebSocket Client Examples

### Using Python (Provided)

```bash
# Stream general metrics
python3 websocket_client.py metrics

# Stream CPU frequencies
python3 websocket_client.py cpu

# Connect to remote server
python3 websocket_client.py metrics 10.10.10.48:8000
python3 websocket_client.py cpu 10.10.10.48:8000
```

### Using JavaScript/Node.js

```javascript
// Connect to metrics stream
const ws = new WebSocket('ws://localhost:8000/ws/metrics');

ws.onopen = () => {
  console.log('Connected to metrics stream');
};

ws.onmessage = (event) => {
  const metrics = JSON.parse(event.data);
  console.log(`Battery: ${metrics.battery_level}%`);
  console.log(`Memory: ${metrics.memory_usage_percent.toFixed(1)}%`);
  console.log(`CPU Avg: ${metrics.cpu_avg_mhz.toFixed(0)} MHz`);
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
};
```

### Using curl (with wscat)

```bash
# Install wscat
npm install -g wscat

# Connect to metrics stream
wscat -c ws://localhost:8000/ws/metrics

# Connect to CPU stream
wscat -c ws://localhost:8000/ws/cpu
```

---

## 📊 Updated Endpoints

### GET /cpu/frequency (IMPROVED)

Returns detailed per-core frequencies with statistics.

**Response Model:**
```python
{
  "per_core": Dict[str, int]      # Individual core frequencies in kHz
  "min_khz": int                   # Minimum frequency in kHz
  "max_khz": int                   # Maximum frequency in kHz
  "min_mhz": float                 # Minimum frequency in MHz
  "max_mhz": float                 # Maximum frequency in MHz
  "avg_mhz": float                 # Average frequency in MHz
  "core_count": int                # Total CPU cores
}
```

---

## 🔄 Use Cases

### Real-time Monitoring Dashboard

Use WebSocket streams to build live dashboards:

```javascript
const metricsSocket = new WebSocket('ws://api.example.com/ws/metrics');
const cpuSocket = new WebSocket('ws://api.example.com/ws/cpu');

metricsSocket.onmessage = (event) => {
  const data = JSON.parse(event.data);
  updateBatteryDisplay(data.battery_level);
  updateMemoryGauge(data.memory_usage_percent);
  updateCPUGauge(data.cpu_avg_mhz, data.cpu_max_mhz);
};

cpuSocket.onmessage = (event) => {
  const data = JSON.parse(event.data);
  updatePerCoreFrequencies(data.per_core);
};
```

### Performance Monitoring

Track CPU frequency scaling in real-time:

```python
import asyncio
import websockets
import json

async def monitor_cpu():
    async with websockets.connect('ws://localhost:8000/ws/cpu') as ws:
        async for message in ws:
            data = json.loads(message)
            print(f"CPU frequency range: {data['min_mhz']:.0f} - {data['max_mhz']:.0f} MHz")
            print(f"Average: {data['avg_mhz']:.0f} MHz")
            
            # Alert on thermal issues
            metrics = json.loads(await ws.recv())
            # ... process thermal data
```

---

## ⚡ Performance

- **CPU Frequency Stream**: Updates every 1 second (1000ms)
- **Metrics Stream**: Updates every 2 seconds (2000ms)
- **Per-core parsing**: <50ms
- **WebSocket overhead**: Minimal (<5ms per message)

---

## 🔧 Implementation Details

### Parser Updates

`parse_cpu_frequencies_detailed()` now extracts:
- Per-core frequencies as dictionary
- Minimum and maximum frequencies
- Average frequency calculation
- Core count

### WebSocket Models

**RealTimeMetrics** - For general system monitoring
- timestamp (ISO 8601)
- battery_level (0-100)
- memory_usage_percent (0-100)
- storage_usage_percent (0-100)
- cpu metrics (min/max/avg MHz)
- thermal max temperature

### Error Handling

WebSocket endpoints gracefully handle errors:
- Connection errors logged
- Disconnects tracked
- Error messages sent to clients
- Automatic reconnection support

---

## 📈 Future Enhancements

Potential additions:
- Historical data retention (caching)
- Per-app CPU usage (requires root)
- GPU frequency monitoring (if available)
- Power consumption tracking
- Thermal throttling detection
- Custom update intervals (configurable)

---

## Version Info

- **v2.0.0**: Type-safe API with Pydantic models
- **v2.1.0**: Enhanced CPU metrics + WebSocket real-time streaming
