#ifndef MODELS_HPP
#define MODELS_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>

using json = nlohmann::json;
using timestamp_t = std::chrono::system_clock::time_point;

// Device info
struct DeviceInfo {
    std::string model;
    std::string manufacturer;
    std::string android_version;
    int sdk;
    std::string hardware;
    std::string board;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DeviceInfo, model, manufacturer, android_version, sdk, hardware, board)
};

// OS info
struct OSInfo {
    std::string android_version;
    int sdk;
    std::string security_patch;
    std::string build_id;
    std::string kernel_version;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OSInfo, android_version, sdk, security_patch, build_id, kernel_version)
};

// CPU info
struct CPUInfo {
    int cores;
    std::string abi;
    std::vector<std::string> abi_list;
    std::string arch;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CPUInfo, cores, abi, abi_list, arch)
};

// CPU frequency
struct CPUFrequency {
    std::map<std::string, int> per_core;
    int min_khz;
    int max_khz;
    double min_mhz;
    double max_mhz;
    double avg_mhz;
    int core_count;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CPUFrequency, per_core, min_khz, max_khz, min_mhz, max_mhz, avg_mhz, core_count)
};

// CPU Governor
struct CPUGovernorInfo {
    std::map<std::string, std::string> per_core;
    std::vector<std::string> available_governors;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CPUGovernorInfo, per_core, available_governors)
};

// CPU Idle State
struct CPUIdleState {
    std::string state;
    std::string name;
    int64_t time_us;
    int usage;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CPUIdleState, state, name, time_us, usage)
};

// CPU Idle Info
struct CPUIdleInfo {
    std::map<std::string, std::vector<CPUIdleState>> per_core;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CPUIdleInfo, per_core)
};

// Memory info
struct MemoryInfo {
    double total_mb;
    double available_mb;
    double used_mb;
    double usage_percent;
    double swap_total_mb;
    double swap_free_mb;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MemoryInfo, total_mb, available_mb, used_mb, usage_percent, swap_total_mb, swap_free_mb)
};

// Storage info
struct StorageInfo {
    std::string filesystem;
    double total_gb;
    double used_gb;
    double free_gb;
    double usage_percent;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StorageInfo, filesystem, total_gb, used_gb, free_gb, usage_percent)
};

// Mount info
struct MountInfo {
    std::string filesystem;
    int size_kb;
    int used_kb;
    int available_kb;
    int use_percent;
    std::string mountpoint;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MountInfo, filesystem, size_kb, used_kb, available_kb, use_percent, mountpoint)
};

// Battery info
struct BatteryInfo {
    int level;
    std::string health;
    std::string status;
    int voltage_mv;
    double temperature_c;
    std::string technology;
    bool is_charging;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BatteryInfo, level, health, status, voltage_mv, temperature_c, technology, is_charging)
};

// Thermal info
struct ThermalInfo {
    std::map<std::string, double> temperatures;
    double max_temp_c;
    double min_temp_c;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ThermalInfo, temperatures, max_temp_c, min_temp_c)
};

// Core temperatures
struct CoreTemperatures {
    std::map<std::string, double> per_core;
    std::string source;
    bool available;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CoreTemperatures, per_core, source, available)
};

// Network info
struct NetworkInfo {
    std::string hostname;
    std::optional<std::string> wifi_ip;
    std::optional<std::string> wifi_mac;
    std::optional<std::string> carrier;
    std::optional<std::string> network_type;
    std::optional<std::string> data_state;

    json to_json() const {
        json j;
        j["hostname"] = hostname;
        j["wifi_ip"] = wifi_ip.has_value() ? json(wifi_ip.value()) : json(nullptr);
        j["wifi_mac"] = wifi_mac.has_value() ? json(wifi_mac.value()) : json(nullptr);
        j["carrier"] = carrier.has_value() ? json(carrier.value()) : json(nullptr);
        j["network_type"] = network_type.has_value() ? json(network_type.value()) : json(nullptr);
        j["data_state"] = data_state.has_value() ? json(data_state.value()) : json(nullptr);
        return j;
    }
};

// Display info
struct DisplayInfo {
    std::string size_px;
    int density_dpi;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DisplayInfo, size_px, density_dpi)
};

// Power info
struct PowerInfo {
    int current_ma;
    std::optional<int> charge_counter;
    std::optional<int> max_charging_current;
    std::string charging_status;

    json to_json() const {
        json j;
        j["current_ma"] = current_ma;
        if (charge_counter) j["charge_counter"] = charge_counter.value();
        if (max_charging_current) j["max_charging_current"] = max_charging_current.value();
        j["charging_status"] = charging_status;
        return j;
    }
};

// Health status
struct HealthStatus {
    std::string status;
    bool adb_connected;
    std::string timestamp;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(HealthStatus, status, adb_connected, timestamp)
};

// Uptime info
struct UptimeInfo {
    int uptime_seconds;
    std::string uptime_formatted;
    std::string boot_time;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(UptimeInfo, uptime_seconds, uptime_formatted, boot_time)
};

// System info (aggregate)
struct SystemInfo {
    DeviceInfo device;
    OSInfo os;
    CPUInfo cpu;
    CPUFrequency cpu_frequency;
    std::optional<CPUGovernorInfo> cpu_governors;
    std::optional<CPUIdleInfo> cpu_idle;
    MemoryInfo memory;
    StorageInfo storage;
    std::optional<std::vector<MountInfo>> mounts;
    BatteryInfo battery;
    PowerInfo power;
    ThermalInfo thermal;
    std::optional<CoreTemperatures> core_temperatures;
    NetworkInfo network;
    DisplayInfo display;
    std::string timestamp;

    json to_json() const {
        json j;
        j["device"] = device;
        j["os"] = os;
        j["cpu"] = cpu;
        j["cpu_frequency"] = cpu_frequency;
        if (cpu_governors) j["cpu_governors"] = cpu_governors.value();
        if (cpu_idle) j["cpu_idle"] = cpu_idle.value();
        j["memory"] = memory;
        j["storage"] = storage;
        if (mounts) j["mounts"] = mounts.value();
        j["battery"] = battery;
        j["power"] = power.to_json();
        j["thermal"] = thermal;
        if (core_temperatures) j["core_temperatures"] = core_temperatures.value();
        j["network"] = network.to_json();
        j["display"] = display;
        j["timestamp"] = timestamp;
        return j;
    }
};

#endif // MODELS_HPP
