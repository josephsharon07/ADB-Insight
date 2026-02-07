from fastapi import FastAPI, HTTPException, status
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi.concurrency import run_in_threadpool
from pydantic import BaseModel, Field
from typing import Dict, List, Optional, Any
from datetime import datetime, timedelta
import logging
import asyncio
import re

from adb_utils import adb_shell, adb_devices, adb_shell_multi
from parsers import (
    parse_key_value_block, parse_cpu_freq, parse_cpu_frequencies_detailed, parse_thermal_data,
    parse_battery_level, kb_to_mb, kb_to_gb, parse_path_value_block, parse_df_output,
    parse_cpu_idle_output
)

logger = logging.getLogger(__name__)

# ============ SIMPLE IN-MEMORY CACHE ============
_cache: Dict[str, Any] = {}


def _get_cached(key: str, ttl_seconds: int) -> Optional[Any]:
    entry = _cache.get(key)
    if not entry:
        return None
    cached_at, value = entry
    if (datetime.now() - cached_at).total_seconds() > ttl_seconds:
        return None
    return value


def _set_cached(key: str, value: Any) -> None:
    _cache[key] = (datetime.now(), value)


async def _get_cached_or_run(key: str, ttl_seconds: int, func):
    cached = _get_cached(key, ttl_seconds)
    if cached is not None:
        return cached
    result = await run_in_threadpool(func)
    _set_cached(key, result)
    return result

# ============ PYDANTIC MODELS ============
class DeviceInfo(BaseModel):
    model: str = Field(..., example="SM-F127G")
    manufacturer: str = Field(..., example="samsung")
    android_version: str = Field(..., example="13")
    sdk: int = Field(..., example=33)
    hardware: str = Field(..., example="exynos850")
    board: str = Field(..., example="universal3830")

    class Config:
        json_schema_extra = {"example": {
            "model": "SM-F127G",
            "manufacturer": "samsung",
            "android_version": "13",
            "sdk": 33,
            "hardware": "exynos850",
            "board": "universal3830"
        }}


class OSInfo(BaseModel):
    android_version: str = Field(..., example="13")
    sdk: int = Field(..., example=33)
    security_patch: str = Field(..., example="2024-12-01")
    build_id: str = Field(..., example="TP1A.220624.014")
    kernel_version: str = Field(..., example="5.10.0-android13-123456")

    class Config:
        json_schema_extra = {"example": {
            "android_version": "13",
            "sdk": 33,
            "security_patch": "2024-12-01",
            "build_id": "TP1A.220624.014",
            "kernel_version": "5.10.0-android13-123456"
        }}


class CPUInfo(BaseModel):
    cores: int = Field(..., example=8)
    abi: str = Field(..., example="arm64-v8a")
    abi_list: List[str] = Field(..., example=["arm64-v8a", "armeabi-v7a"])
    arch: str = Field(..., example="ARMv8")


class CPUFrequency(BaseModel):
    per_core: Dict[str, int] = Field(..., example={"cpu0": 1800000, "cpu1": 1800000})
    min_khz: int = Field(..., example=546000)
    max_khz: int = Field(..., example=1800000)
    min_mhz: float = Field(..., example=546.0)
    max_mhz: float = Field(..., example=1800.0)
    avg_mhz: float = Field(..., example=1173.25)
    core_count: int = Field(..., example=8)


class CPUGovernorInfo(BaseModel):
    per_core: Dict[str, str] = Field(..., example={"cpu0": "schedutil", "cpu1": "schedutil"})
    available_governors: List[str] = Field(..., example=["schedutil", "performance", "powersave"])


class CPUIdleState(BaseModel):
    state: str = Field(..., example="state0")
    name: str = Field(..., example="WFI")
    time_us: int = Field(..., example=31574305828)
    usage: int = Field(..., example=123456)


class CPUIdleInfo(BaseModel):
    per_core: Dict[str, List[CPUIdleState]] = Field(...)


class MemoryInfo(BaseModel):
    total_mb: float = Field(..., example=3704.71)
    available_mb: float = Field(..., example=1236.54)
    used_mb: float = Field(..., example=2468.17)
    usage_percent: float = Field(..., example=66.6)
    swap_total_mb: float = Field(..., example=4096.0)
    swap_free_mb: float = Field(..., example=2729.53)


class StorageInfo(BaseModel):
    filesystem: str = Field(..., example="/dev/block/dm-44")
    total_gb: float = Field(..., example=51.35)
    used_gb: float = Field(..., example=15.63)
    free_gb: float = Field(..., example=35.52)
    usage_percent: float = Field(..., example=30.5)


class MountInfo(BaseModel):
    filesystem: str = Field(..., example="/dev/block/dm-44")
    size_kb: int = Field(..., example=53819392)
    used_kb: int = Field(..., example=17993848)
    available_kb: int = Field(..., example=35694472)
    use_percent: int = Field(..., example=34)
    mountpoint: str = Field(..., example="/data")


class BatteryInfo(BaseModel):
    level: int = Field(..., example=39, ge=0, le=100)
    health: str = Field(..., example="Good")
    status: str = Field(..., example="Charging")
    voltage_mv: int = Field(..., example=3960)
    temperature_c: float = Field(..., example=34.6)
    technology: str = Field(..., example="Li-ion")
    is_charging: bool = Field(..., example=True)


class PowerInfo(BaseModel):
    current_ma: int = Field(..., example=1022, description="Current draw in milliamps")
    charge_counter: Optional[int] = Field(None, example=1865000, description="Charge counter value")
    max_charging_current: Optional[int] = Field(None, example=0, description="Max charging current in mA")
    charging_status: str = Field(..., example="discharging", description="charging, discharging, not_charging, or full")


class ThermalInfo(BaseModel):
    temperatures: Dict[str, float] = Field(..., example={"AP": 39.5, "BAT": 34.6, "SKIN": 35.4})
    max_temp_c: float = Field(..., example=39.5)
    min_temp_c: float = Field(..., example=34.6)


class CoreTemperatures(BaseModel):
    per_core: Dict[str, float] = Field(..., example={"cpu0": 40.1, "cpu1": 41.3})
    source: str = Field(..., example="thermalservice")
    available: bool = Field(..., example=False)


class NetworkInfo(BaseModel):
    hostname: str = Field(..., example="android")
    wifi_ip: Optional[str] = Field(None, example="192.168.1.50")
    wifi_mac: Optional[str] = Field(None, example="aa:bb:cc:dd:ee:ff")
    carrier: Optional[str] = Field(None, example="ExampleCarrier")
    network_type: Optional[str] = Field(None, example="LTE")
    data_state: Optional[str] = Field(None, example="CONNECTED")


class DisplayInfo(BaseModel):
    size_px: str = Field(..., example="1080x2408")
    density_dpi: int = Field(..., example=420)


class HealthStatus(BaseModel):
    status: str = Field(..., example="healthy")
    adb_connected: bool = Field(..., example=True)
    timestamp: datetime = Field(...)


class RealTimeMetrics(BaseModel):
    """Real-time system metrics for streaming."""
    timestamp: datetime
    battery_level: int
    memory_usage_percent: float
    storage_usage_percent: float
    cpu_avg_mhz: float
    cpu_max_mhz: float
    cpu_min_mhz: float
    thermal_max_temp: float


class UptimeInfo(BaseModel):
    uptime_seconds: int = Field(..., example=86400)
    uptime_formatted: str = Field(..., example="1d 2h 30m 45s")
    boot_time: datetime = Field(...)


class SystemInfo(BaseModel):
    device: DeviceInfo
    os: OSInfo
    cpu: CPUInfo
    cpu_frequency: CPUFrequency
    cpu_governors: Optional[CPUGovernorInfo] = None
    cpu_idle: Optional[CPUIdleInfo] = None
    memory: MemoryInfo
    storage: StorageInfo
    mounts: Optional[List[MountInfo]] = None
    battery: BatteryInfo
    power: PowerInfo
    thermal: ThermalInfo
    core_temperatures: Optional[CoreTemperatures] = None
    network: NetworkInfo
    display: DisplayInfo
    timestamp: datetime = Field(...)


# ============ FastAPI APP ============
app = FastAPI(
    title="DroidMetrics",
    description="Real-time Android system metrics via ADB (no root, no app required) - by bluecape",
    version="2.0.0",
    docs_url="/docs",
    redoc_url="/redoc",
    openapi_url="/openapi.json"
)

# ============ CORS MIDDLEWARE ============
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allow all origins
    allow_credentials=True,
    allow_methods=["*"],  # Allow all methods
    allow_headers=["*"],  # Allow all headers
    expose_headers=["*"],
)


# ============ ERROR HANDLERS ============
@app.exception_handler(RuntimeError)
async def runtime_error_handler(request, exc):
    return JSONResponse(
        status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
        content={"detail": str(exc), "error": "ADB Error"}
    )


# ============ HEALTH & DIAGNOSTICS ============
@app.get("/health", response_model=HealthStatus, tags=["System"])
async def health_check():
    """Check API health and ADB connection status."""
    try:
        devices = adb_devices()
        is_connected = False
        if devices:
            lines = devices.splitlines()
            for line in lines[1:]:
                parts = line.split()
                if len(parts) >= 2 and parts[1] == "device":
                    is_connected = True
                    break
        status_str = "healthy" if is_connected else "degraded"
        
        return HealthStatus(
            status=status_str,
            adb_connected=is_connected,
            timestamp=datetime.now()
        )
    except Exception as e:
        logger.error(f"Health check failed: {e}")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="ADB connection failed"
        )


@app.get("/", tags=["Root"])
async def root():
    """API root endpoint with available endpoints."""
    return {
        "app": "Android System API (ADB)",
        "version": "2.0.0",
        "endpoints": {
            "health": "/health",
            "device": "/device",
            "os": "/os",
            "cpu": "/cpu",
            "cpu_frequency": "/cpu/frequency",
            "cpu_governors": "/cpu/governors",
            "cpu_idle": "/cpu/idle",
            "memory": "/memory",
            "storage": "/storage",
            "mounts": "/storage/mounts",
            "battery": "/battery",
            "power": "/power",
            "thermal": "/thermal",
            "core_temperatures": "/thermal/cores",
            "network": "/network",
            "display": "/display",
            "uptime": "/uptime",
            "system": "/system"
        },
        "docs": "/docs",
        "timestamp": datetime.now()
    }


# ============ DEVICE ENDPOINT ============
@app.get("/device", response_model=DeviceInfo, tags=["Device"])
async def device_info():
    """
    Get device information (model, manufacturer, Android version, etc).
    
    No root required. Uses system properties.
    """
    try:
        return await _get_cached_or_run("device_info", 300, _build_device_info)
    except Exception as e:
        logger.error(f"Device info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/os", response_model=OSInfo, tags=["Device"])
async def os_info():
    """Get operating system information (Android version, SDK, patch level, build, kernel)."""
    try:
        return await _get_cached_or_run("os_info", 300, _build_os_info)
    except Exception as e:
        logger.error(f"OS info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ CPU ENDPOINTS ============
@app.get("/cpu", response_model=CPUInfo, tags=["CPU"])
async def cpu_info():
    """
    Get CPU information (cores, architecture, ABI).
    
    Returns: core count, ABI, and supported architectures.
    """
    try:
        return await _get_cached_or_run("cpu_info", 300, _build_cpu_info)
    except Exception as e:
        logger.error(f"CPU info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/cpu/frequency", response_model=CPUFrequency, tags=["CPU"])
async def cpu_frequency():
    """
    Get real-time CPU frequency for each core with statistics.
    
    Returns: per-core frequencies in kHz, plus min, max, and average calculations.
    Uses cpuinfo_min_freq and cpuinfo_max_freq from all cores for accurate limits.
    """
    try:
        return await run_in_threadpool(_build_cpu_frequency)
    except Exception as e:
        logger.error(f"CPU frequency error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ CPU GOVERNORS ENDPOINT ============
@app.get("/cpu/governors", response_model=CPUGovernorInfo, tags=["CPU"])
async def cpu_governors():
    """
    Get CPU governor information per core.

    Returns: per-core governor and list of available governors.
    """
    try:
        return await _get_cached_or_run("cpu_governors", 300, _build_cpu_governors)
    except Exception as e:
        logger.error(f"CPU governors error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ CPU IDLE STATES ENDPOINT ============
@app.get("/cpu/idle", response_model=CPUIdleInfo, tags=["CPU"])
async def cpu_idle_states():
    """
    Get CPU idle state statistics per core.
    """
    try:
        return await run_in_threadpool(_build_cpu_idle_info)
    except Exception as e:
        logger.error(f"CPU idle error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ MEMORY ENDPOINT ============
@app.get("/memory", response_model=MemoryInfo, tags=["Memory"])
async def memory_info():
    """
    Get RAM and swap memory information.
    
    Returns: total, available, used memory in MB, plus swap info.
    """
    try:
        return await run_in_threadpool(_build_memory_info)
    except Exception as e:
        logger.error(f"Memory info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ STORAGE ENDPOINT ============
@app.get("/storage", response_model=StorageInfo, tags=["Storage"])
async def storage_info():
    """
    Get internal storage (/data) information.
    
    Returns: total, used, free space in GB with usage percentage.
    """
    try:
        return await run_in_threadpool(_build_storage_info)
    except Exception as e:
        logger.error(f"Storage info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ MOUNT BREAKDOWN ENDPOINT ============
@app.get("/storage/mounts", response_model=List[MountInfo], tags=["Storage"])
async def storage_mounts():
    """
    Get mount breakdown for all filesystems.

    Returns: list of mounts with size/usage.
    """
    try:
        return await _get_cached_or_run("storage_mounts", 30, _build_storage_mounts)
    except Exception as e:
        logger.error(f"Storage mounts error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ BATTERY ENDPOINT ============
@app.get("/battery", response_model=BatteryInfo, tags=["Battery"])
async def battery_info():
    """
    Get battery status and health.
    
    Returns: charge level, health, status, voltage, temperature, and charging state.
    """
    try:
        return await run_in_threadpool(_build_battery_info)
    except Exception as e:
        logger.error(f"Battery info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ POWER ENDPOINT ============
@app.get("/power", response_model=PowerInfo, tags=["Battery"])
async def power_info():
    """
    Get power consumption and charging info.
    
    Returns: current draw in mA, charge counter, and charging status.
    """
    try:
        return await run_in_threadpool(_build_power_info)
    except Exception as e:
        logger.error(f"Power info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ THERMAL ENDPOINT ============
@app.get("/thermal", response_model=ThermalInfo, tags=["Thermal"])
async def thermal_info():
    """
    Get thermal sensor readings.
    
    Returns: CPU, battery, and skin temperatures with min/max values.
    """
    try:
        return await run_in_threadpool(_build_thermal_info)
    except Exception as e:
        logger.error(f"Thermal info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ CORE TEMPERATURES ENDPOINT ============
@app.get("/thermal/cores", response_model=CoreTemperatures, tags=["Thermal"])
async def core_temperatures():
    """
    Get per-core temperatures if available.

    Returns: per-core temperatures and availability flag.
    """
    try:
        return await run_in_threadpool(_build_core_temperatures)
    except Exception as e:
        logger.error(f"Core temperatures error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ NETWORK ENDPOINT ==========
@app.get("/network", response_model=NetworkInfo, tags=["Network"])
async def network_info():
    """
    Get network information.

    Returns: hostname, wifi IP/MAC, carrier, network type, and data state.
    """
    try:
        return await _get_cached_or_run("network_info", 30, _build_network_info)
    except Exception as e:
        logger.error(f"Network info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ DISPLAY ENDPOINT ==========
@app.get("/display", response_model=DisplayInfo, tags=["Display"])
async def display_info():
    """
    Get display metrics.

    Returns: screen size and density.
    """
    try:
        return await _get_cached_or_run("display_info", 300, _build_display_info)
    except Exception as e:
        logger.error(f"Display info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ UPTIME ENDPOINT ============
@app.get("/uptime", response_model=UptimeInfo, tags=["System"])
async def uptime_info():
    """
    Get system uptime information.
    
    Returns: Device uptime in seconds and formatted string.
    """
    try:
        return await run_in_threadpool(_build_uptime_info)
    except Exception as e:
        logger.error(f"Uptime error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ FULL SYSTEM ENDPOINT ============
@app.get("/system", response_model=SystemInfo, tags=["System"])
async def system_info():
    """
    Get complete system information.
    
    Returns: All device metrics (device, CPU, memory, storage, battery, thermal).
    Single endpoint for comprehensive system overview.
    """
    try:
        device_task = _get_cached_or_run("device_info", 300, _build_device_info)
        os_task = _get_cached_or_run("os_info", 300, _build_os_info)
        cpu_task = _get_cached_or_run("cpu_info", 300, _build_cpu_info)
        cpu_gov_task = _get_cached_or_run("cpu_governors", 300, _build_cpu_governors)
        cpu_freq_task = run_in_threadpool(_build_cpu_frequency)
        cpu_idle_task = run_in_threadpool(_build_cpu_idle_info)
        memory_task = run_in_threadpool(_build_memory_info)
        storage_task = run_in_threadpool(_build_storage_info)
        mounts_task = _get_cached_or_run("storage_mounts", 30, _build_storage_mounts)
        battery_task = run_in_threadpool(_build_battery_info)
        power_task = run_in_threadpool(_build_power_info)
        thermal_task = run_in_threadpool(_build_thermal_info)
        core_temps_task = run_in_threadpool(_build_core_temperatures)
        network_task = _get_cached_or_run("network_info", 30, _build_network_info)
        display_task = _get_cached_or_run("display_info", 300, _build_display_info)

        device, osinfo, cpu, cpu_gov, cpu_freq, cpu_idle, memory, storage, mounts, battery, power, thermal, core_temps, network, display = await asyncio.gather(
            device_task,
            os_task,
            cpu_task,
            cpu_gov_task,
            cpu_freq_task,
            cpu_idle_task,
            memory_task,
            storage_task,
            mounts_task,
            battery_task,
            power_task,
            thermal_task,
            core_temps_task,
            network_task,
            display_task
        )
        
        return SystemInfo(
            device=device,
            os=osinfo,
            cpu=cpu,
            cpu_frequency=cpu_freq,
            cpu_governors=cpu_gov,
            cpu_idle=cpu_idle,
            memory=memory,
            storage=storage,
            mounts=mounts,
            battery=battery,
            power=power,
            thermal=thermal,
            core_temperatures=core_temps,
            network=network,
            display=display,
            timestamp=datetime.now()
        )
    except Exception as e:
        logger.error(f"System info error: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# ============ INTERNAL BUILDERS (SYNC) ============
def _build_device_info() -> DeviceInfo:
    model, manufacturer, android_version, sdk, hardware, board = adb_shell_multi([
        "getprop ro.product.model",
        "getprop ro.product.manufacturer",
        "getprop ro.build.version.release",
        "getprop ro.build.version.sdk",
        "getprop ro.hardware",
        "getprop ro.board.platform"
    ])

    return DeviceInfo(
        model=model,
        manufacturer=manufacturer,
        android_version=android_version,
        sdk=int(sdk or 0),
        hardware=hardware,
        board=board,
    )


def _build_os_info() -> OSInfo:
    android_version, sdk, security_patch, build_id, kernel_version = adb_shell_multi([
        "getprop ro.build.version.release",
        "getprop ro.build.version.sdk",
        "getprop ro.build.version.security_patch",
        "getprop ro.build.display.id",
        "uname -r"
    ])

    return OSInfo(
        android_version=android_version,
        sdk=int(sdk or 0),
        security_patch=security_patch,
        build_id=build_id,
        kernel_version=kernel_version,
    )


def _build_cpu_info() -> CPUInfo:
    cores_str, abi, abi_list_raw = adb_shell_multi([
        "nproc",
        "getprop ro.product.cpu.abi",
        "getprop ro.product.cpu.abilist"
    ])
    cores = int(cores_str or 0)
    abi_list = [a.strip() for a in abi_list_raw.split(",") if a.strip()]

    arch_map = {
        "arm64-v8a": "ARMv8",
        "armeabi-v7a": "ARMv7",
        "x86_64": "x86-64",
        "x86": "x86"
    }
    arch = arch_map.get(abi, "Unknown")

    return CPUInfo(
        cores=cores,
        abi=abi,
        abi_list=abi_list,
        arch=arch
    )


def _build_cpu_governors() -> CPUGovernorInfo:
    available_raw = adb_shell("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors")
    available = [g.strip() for g in available_raw.split() if g.strip()]

    raw = adb_shell(
        "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; "
        "do echo $f: $(cat $f); done"
    )
    per_core = parse_path_value_block(raw)

    return CPUGovernorInfo(
        per_core=per_core,
        available_governors=available
    )


def _build_cpu_idle_info() -> CPUIdleInfo:
    raw = adb_shell(
        "for cpu in /sys/devices/system/cpu/cpu[0-9]*; do "
        "c=$(basename $cpu); "
        "for s in $cpu/cpuidle/state*; do "
        "st=$(basename $s); "
        "name=$(cat $s/name 2>/dev/null); "
        "time=$(cat $s/time 2>/dev/null); "
        "usage=$(cat $s/usage 2>/dev/null); "
        "echo $c $st $name $time $usage; "
        "done; "
        "done"
    )
    per_core = parse_cpu_idle_output(raw)
    return CPUIdleInfo(per_core=per_core)


def _build_cpu_frequency() -> CPUFrequency:
    raw = adb_shell(
        "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; "
        "do echo $f: $(cat $f); done"
    )
    freq_data = parse_cpu_frequencies_detailed(raw)

    if "error" in freq_data:
        raise HTTPException(status_code=500, detail="Failed to parse CPU frequencies")

    try:
        min_raw = adb_shell(
            "for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_min_freq; "
            "do cat $f; done"
        )
        min_freqs = [int(x.strip()) for x in min_raw.strip().split('\n') if x.strip().isdigit()]
        min_freq = min(min_freqs) if min_freqs else freq_data["min_khz"]
    except Exception:
        min_freq = freq_data["min_khz"]

    try:
        max_raw = adb_shell(
            "for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq; "
            "do cat $f; done"
        )
        max_freqs = [int(x.strip()) for x in max_raw.strip().split('\n') if x.strip().isdigit()]
        max_freq = max(max_freqs) if max_freqs else freq_data["max_khz"]
    except Exception:
        max_freq = freq_data["max_khz"]

    return CPUFrequency(
        per_core=freq_data["per_core"],
        min_khz=min_freq,
        max_khz=max_freq,
        min_mhz=round(min_freq / 1000, 2),
        max_mhz=round(max_freq / 1000, 2),
        avg_mhz=freq_data["avg_mhz"],
        core_count=freq_data["core_count"]
    )


def _build_memory_info() -> MemoryInfo:
    meminfo = adb_shell("cat /proc/meminfo")
    data = {}

    for line in meminfo.splitlines():
        if line.startswith(("MemTotal", "MemAvailable", "SwapTotal", "SwapFree")):
            key, val = line.split(":", 1)
            data[key] = kb_to_mb(val.strip().split()[0])

    total = data.get("MemTotal", 0)
    available = data.get("MemAvailable", 0)
    used = total - available
    usage_percent = round((used / total * 100), 2) if total > 0 else 0

    return MemoryInfo(
        total_mb=total,
        available_mb=available,
        used_mb=used,
        usage_percent=usage_percent,
        swap_total_mb=data.get("SwapTotal", 0),
        swap_free_mb=data.get("SwapFree", 0)
    )


def _build_storage_info() -> StorageInfo:
    out = adb_shell("df /data | tail -1").split()
    total_kb = int(out[1])
    used_kb = int(out[2])
    free_kb = int(out[3])

    usage_percent = round((used_kb / total_kb * 100), 2) if total_kb > 0 else 0

    return StorageInfo(
        filesystem=out[0],
        total_gb=kb_to_gb(total_kb),
        used_gb=kb_to_gb(used_kb),
        free_gb=kb_to_gb(free_kb),
        usage_percent=usage_percent
    )


def _build_storage_mounts() -> List[MountInfo]:
    raw = adb_shell("df -k")
    mounts = parse_df_output(raw)
    return [MountInfo(**m) for m in mounts]


def _build_battery_info() -> BatteryInfo:
    raw = adb_shell("dumpsys battery")
    battery_data = parse_key_value_block(raw)
    battery = parse_battery_level(battery_data)

    return BatteryInfo(
        level=battery["level"],
        health=battery["health"],
        status=battery["status"],
        voltage_mv=battery["voltage_mv"],
        temperature_c=battery["temperature_c"],
        technology=battery["technology"],
        is_charging=battery["is_charging"]
    )


def _build_power_info() -> PowerInfo:
    raw = adb_shell("dumpsys battery")
    battery_data = parse_key_value_block(raw)
    
    # Extract power consumption data
    current_ma = int(battery_data.get("current now", "0"))
    charge_counter = None
    max_charging_current = None
    charging_status = "unknown"
    
    if "Charge counter" in battery_data:
        try:
            charge_counter = int(battery_data["Charge counter"])
        except (ValueError, KeyError):
            pass
    
    if "Max charging current" in battery_data:
        try:
            max_charging_current = int(battery_data["Max charging current"])
        except (ValueError, KeyError):
            pass
    
    # Determine charging status
    if "status" in battery_data:
        status = battery_data["status"].lower()
        # Map numeric codes: 2=charging, 3=discharging, 4=not_charging, 5=full
        if status == "charging" or status == "2":
            charging_status = "charging"
        elif status == "discharging" or status == "3":
            charging_status = "discharging"
        elif status == "not charging" or status == "4":
            charging_status = "not_charging"
        elif status == "full" or status == "5":
            charging_status = "full"
    
    return PowerInfo(
        current_ma=current_ma,
        charge_counter=charge_counter,
        max_charging_current=max_charging_current,
        charging_status=charging_status
    )


def _build_thermal_info() -> ThermalInfo:
    raw = adb_shell("dumpsys thermalservice")
    temps = parse_thermal_data(raw)

    if not temps or "raw" in temps:
        raise HTTPException(status_code=500, detail="Failed to parse thermal data")

    temp_values = [t["value"] for t in temps.values()]
    max_temp = max(temp_values) if temp_values else 0
    min_temp = min(temp_values) if temp_values else 0

    simple_temps = {name: data["value"] for name, data in temps.items()}

    return ThermalInfo(
        temperatures=simple_temps,
        max_temp_c=max_temp,
        min_temp_c=min_temp
    )


def _build_core_temperatures() -> CoreTemperatures:
    raw = adb_shell("dumpsys thermalservice")
    temps = parse_thermal_data(raw)

    per_core = {}
    if temps and "raw" not in temps:
        for name, data in temps.items():
            if re.match(r"^cpu\d+$", name.lower()):
                per_core[name.lower()] = float(data.get("value", 0))

    available = bool(per_core)
    return CoreTemperatures(
        per_core=per_core,
        source="thermalservice",
        available=available
    )


def _build_uptime_info() -> UptimeInfo:
    result = adb_shell("cat /proc/uptime")
    uptime_seconds = int(float(result.split()[0]))

    days = uptime_seconds // 86400
    hours = (uptime_seconds % 86400) // 3600
    minutes = (uptime_seconds % 3600) // 60
    seconds = uptime_seconds % 60

    if days > 0:
        formatted = f"{days}d {hours}h {minutes}m {seconds}s"
    elif hours > 0:
        formatted = f"{hours}h {minutes}m {seconds}s"
    else:
        formatted = f"{minutes}m {seconds}s"

    now = datetime.now()
    boot_time = now - timedelta(seconds=uptime_seconds)

    return UptimeInfo(
        uptime_seconds=uptime_seconds,
        uptime_formatted=formatted,
        boot_time=boot_time
    )


def _build_network_info() -> NetworkInfo:
    hostname, wifi_ip, carrier, network_type, data_state = adb_shell_multi([
        "getprop net.hostname",
        "getprop dhcp.wlan0.ipaddress",
        "getprop gsm.operator.alpha",
        "getprop gsm.network.type",
        "getprop gsm.data.state"
    ])
    if not wifi_ip:
        try:
            ip_out = adb_shell("ip -f inet addr show wlan0 | grep inet | awk '{print $2}' | head -n 1")
            wifi_ip = ip_out.split("/")[0] if ip_out else ""
        except Exception:
            wifi_ip = ""

    try:
        wifi_mac = adb_shell("cat /sys/class/net/wlan0/address")
    except Exception:
        wifi_mac = ""

    return NetworkInfo(
        hostname=hostname or "android",
        wifi_ip=wifi_ip or None,
        wifi_mac=wifi_mac or None,
        carrier=carrier or None,
        network_type=network_type or None,
        data_state=data_state or None
    )


def _build_display_info() -> DisplayInfo:
    size_out = adb_shell("wm size | head -n 1")
    density_out = adb_shell("wm density | head -n 1")

    size_px = "unknown"
    if ":" in size_out:
        size_px = size_out.split(":", 1)[1].strip()

    density_dpi = 0
    if ":" in density_out:
        density_str = density_out.split(":", 1)[1].strip().split()[0]
        try:
            density_dpi = int(density_str)
        except ValueError:
            density_dpi = 0

    return DisplayInfo(
        size_px=size_px,
        density_dpi=density_dpi
    )
