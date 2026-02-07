#include <httplib.h>
#include <nlohmann/json.hpp>
#include "adb_utils.hpp"
#include "parsers.hpp"
#include "models.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// Cache with TTL
struct CacheEntry {
    std::string value;
    std::chrono::system_clock::time_point timestamp;
};

std::map<std::string, CacheEntry> cache;

std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(6) << ms.count() * 1000;
    return ss.str();
}

bool is_cached_valid(const std::string& key, int ttl_seconds) {
    auto it = cache.find(key);
    if (it == cache.end()) return false;
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - it->second.timestamp
    );
    return elapsed.count() < ttl_seconds;
}

std::string get_cached(const std::string& key) {
    auto it = cache.find(key);
    return it != cache.end() ? it->second.value : "";
}

void set_cached(const std::string& key, const std::string& value) {
    cache[key] = {value, std::chrono::system_clock::now()};
}

// Builder functions
DeviceInfo build_device_info() {
    auto results = adb::shell_multi({
        "getprop ro.product.model",
        "getprop ro.product.manufacturer",
        "getprop ro.build.version.release",
        "getprop ro.build.version.sdk",
        "getprop ro.hardware",
        "getprop ro.board.platform"
    });
    
    return DeviceInfo{
        results[0],
        results[1],
        results[2],
        results[3].empty() ? 0 : std::stoi(results[3]),
        results[4],
        results[5]
    };
}

OSInfo build_os_info() {
    auto results = adb::shell_multi({
        "getprop ro.build.version.release",
        "getprop ro.build.version.sdk",
        "getprop ro.build.version.security_patch",
        "getprop ro.build.display.id",
        "uname -r"
    });
    
    return OSInfo{
        results[0],
        results[1].empty() ? 0 : std::stoi(results[1]),
        results[2],
        results[3],
        results[4]
    };
}

CPUInfo build_cpu_info() {
    auto results = adb::shell_multi({
        "nproc",
        "getprop ro.product.cpu.abi",
        "getprop ro.product.cpu.abilist"
    });
    
    int cores = results[0].empty() ? 0 : std::stoi(results[0]);
    std::string abi = results[1];
    
    std::vector<std::string> abi_list;
    std::istringstream iss(results[2]);
    std::string item;
    while (std::getline(iss, item, ',')) {
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) abi_list.push_back(item);
    }
    
    std::map<std::string, std::string> arch_map = {
        {"arm64-v8a", "ARMv8"},
        {"armeabi-v7a", "ARMv7"},
        {"x86_64", "x86-64"},
        {"x86", "x86"}
    };
    
    std::string arch = arch_map.count(abi) ? arch_map[abi] : "Unknown";
    
    return CPUInfo{cores, abi, abi_list, arch};
}

CPUFrequency build_cpu_frequency() {
    std::string raw = adb::shell(
        "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; "
        "do echo $f: $(cat $f); done"
    );
    
    auto freq_data = parsers::parse_cpu_frequencies_detailed(raw);
    
    if (freq_data.error) {
        throw std::runtime_error("Failed to parse CPU frequencies");
    }
    
    int min_freq = freq_data.min_khz;
    int max_freq = freq_data.max_khz;
    
    try {
        std::string min_raw = adb::shell(
            "for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_min_freq; "
            "do cat $f; done"
        );
        std::vector<int> min_freqs;
        std::istringstream iss(min_raw);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && std::all_of(line.begin(), line.end(), ::isdigit)) {
                min_freqs.push_back(std::stoi(line));
            }
        }
        if (!min_freqs.empty()) {
            min_freq = *std::min_element(min_freqs.begin(), min_freqs.end());
        }
    } catch (...) {}
    
    try {
        std::string max_raw = adb::shell(
            "for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq; "
            "do cat $f; done"
        );
        std::vector<int> max_freqs;
        std::istringstream iss(max_raw);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && std::all_of(line.begin(), line.end(), ::isdigit)) {
                max_freqs.push_back(std::stoi(line));
            }
        }
        if (!max_freqs.empty()) {
            max_freq = *std::max_element(max_freqs.begin(), max_freqs.end());
        }
    } catch (...) {}
    
    return CPUFrequency{
        freq_data.per_core,
        min_freq,
        max_freq,
        std::round((min_freq / 1000.0) * 100) / 100,
        std::round((max_freq / 1000.0) * 100) / 100,
        freq_data.avg_mhz,
        freq_data.core_count
    };
}

CPUGovernorInfo build_cpu_governors() {
    std::string available_raw = adb::shell(
        "cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"
    );
    std::vector<std::string> available;
    std::istringstream iss(available_raw);
    std::string governor;
    while (iss >> governor) {
        available.push_back(governor);
    }
    
    std::string raw = adb::shell(
        "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; "
        "do echo $f: $(cat $f); done"
    );
    
    auto per_core = parsers::parse_path_value_block(raw);
    
    return CPUGovernorInfo{per_core, available};
}

CPUIdleInfo build_cpu_idle_info() {
    std::string raw = adb::shell(
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
    );
    
    auto per_core = parsers::parse_cpu_idle_output(raw);
    return CPUIdleInfo{per_core};
}

MemoryInfo build_memory_info() {
    std::string meminfo = adb::shell("cat /proc/meminfo");
    std::map<std::string, double> data;
    
    std::istringstream iss(meminfo);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("MemTotal") != std::string::npos ||
            line.find("MemAvailable") != std::string::npos ||
            line.find("SwapTotal") != std::string::npos ||
            line.find("SwapFree") != std::string::npos) {
            
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value_str = line.substr(pos + 1);
                value_str.erase(0, value_str.find_first_not_of(" \t"));
                std::istringstream val_stream(value_str);
                std::string val;
                val_stream >> val;
                
                if (!val.empty()) {
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    data[key] = parsers::kb_to_mb(val);
                }
            }
        }
    }
    
    double total = data.count("MemTotal") ? data["MemTotal"] : 0;
    double available = data.count("MemAvailable") ? data["MemAvailable"] : 0;
    double used = total - available;
    double usage_percent = total > 0 ? std::round((used / total * 100) * 100) / 100 : 0;
    
    return MemoryInfo{
        total,
        available,
        used,
        usage_percent,
        data.count("SwapTotal") ? data["SwapTotal"] : 0,
        data.count("SwapFree") ? data["SwapFree"] : 0
    };
}

StorageInfo build_storage_info() {
    std::string output = adb::shell("df /data | tail -1");
    std::istringstream iss(output);
    
    std::string filesystem;
    int total_kb, used_kb, free_kb;
    
    if (!(iss >> filesystem >> total_kb >> used_kb >> free_kb)) {
        throw std::runtime_error("Failed to parse storage info");
    }
    
    double usage_percent = total_kb > 0 ? std::round((used_kb / (double)total_kb * 100) * 100) / 100 : 0;
    
    return StorageInfo{
        filesystem,
        parsers::kb_to_gb(total_kb),
        parsers::kb_to_gb(used_kb),
        parsers::kb_to_gb(free_kb),
        usage_percent
    };
}

std::vector<MountInfo> build_storage_mounts() {
    std::string raw = adb::shell("df -k");
    return parsers::parse_df_output(raw);
}

BatteryInfo build_battery_info() {
    std::string raw = adb::shell("dumpsys battery");
    auto battery_data = parsers::parse_key_value_block(raw);
    auto battery = parsers::parse_battery_level(battery_data);
    
    return BatteryInfo{
        battery.level,
        battery.health,
        battery.status,
        battery.voltage_mv,
        battery.temperature_c,
        battery.technology,
        battery.is_charging
    };
}

PowerInfo build_power_info() {
    std::string raw = adb::shell("dumpsys battery");
    auto battery_data = parsers::parse_key_value_block(raw);
    return parsers::parse_power_info(battery_data);
}

ThermalInfo build_thermal_info() {
    std::string raw = adb::shell("dumpsys thermalservice");
    auto temps = parsers::parse_thermal_data(raw);
    
    if (temps.empty()) {
        throw std::runtime_error("Failed to parse thermal data");
    }
    
    std::map<std::string, double> simple_temps;
    std::vector<double> temp_values;
    
    for (const auto& [name, data] : temps) {
        double value = data.at("value");
        simple_temps[name] = value;
        temp_values.push_back(value);
    }
    
    double max_temp = !temp_values.empty() ? *std::max_element(temp_values.begin(), temp_values.end()) : 0;
    double min_temp = !temp_values.empty() ? *std::min_element(temp_values.begin(), temp_values.end()) : 0;
    
    return ThermalInfo{simple_temps, max_temp, min_temp};
}

CoreTemperatures build_core_temperatures() {
    std::string raw = adb::shell("dumpsys thermalservice");
    auto temps = parsers::parse_thermal_data(raw);
    
    std::map<std::string, double> per_core;
    std::regex cpu_regex(R"(^cpu\d+$)");
    
    for (const auto& [name, data] : temps) {
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        if (std::regex_match(lower_name, cpu_regex)) {
            per_core[lower_name] = data.at("value");
        }
    }
    
    return CoreTemperatures{
        per_core,
        "thermalservice",
        !per_core.empty()
    };
}

NetworkInfo build_network_info() {
    auto results = adb::shell_multi({
        "getprop net.hostname",
        "getprop dhcp.wlan0.ipaddress",
        "getprop gsm.operator.alpha",
        "getprop gsm.network.type",
        "getprop gsm.data.state"
    });
    
    std::string wifi_ip = results[1];
    if (wifi_ip.empty()) {
        try {
            std::string ip_out = adb::shell("ip -f inet addr show wlan0 | grep inet | awk '{print $2}' | head -n 1");
            size_t slash_pos = ip_out.find('/');
            if (slash_pos != std::string::npos) {
                wifi_ip = ip_out.substr(0, slash_pos);
            }
        } catch (...) {}
    }
    
    return NetworkInfo{
        results[0].empty() ? "android" : results[0],
        wifi_ip.empty() ? std::nullopt : std::optional<std::string>(wifi_ip),
        std::nullopt,
        results[2].empty() ? std::nullopt : std::optional<std::string>(results[2]),
        results[3].empty() ? std::nullopt : std::optional<std::string>(results[3]),
        results[4].empty() ? std::nullopt : std::optional<std::string>(results[4])
    };
}

DisplayInfo build_display_info() {
    std::string size_out = adb::shell("wm size | head -n 1");
    std::string density_out = adb::shell("wm density | head -n 1");
    
    std::string size_px = "unknown";
    if (size_out.find(':') != std::string::npos) {
        size_px = size_out.substr(size_out.find(':') + 1);
        size_px.erase(0, size_px.find_first_not_of(" \t"));
    }
    
    int density_dpi = 0;
    if (density_out.find(':') != std::string::npos) {
        std::string density_str = density_out.substr(density_out.find(':') + 1);
        density_str.erase(0, density_str.find_first_not_of(" \t"));
        std::istringstream iss(density_str);
        iss >> density_dpi;
    }
    
    return DisplayInfo{size_px, density_dpi};
}

UptimeInfo build_uptime_info() {
    std::string result = adb::shell("cat /proc/uptime");
    int uptime_seconds = (int)std::stod(result.substr(0, result.find(' ')));
    
    int days = uptime_seconds / 86400;
    int hours = (uptime_seconds % 86400) / 3600;
    int minutes = (uptime_seconds % 3600) / 60;
    int seconds = uptime_seconds % 60;
    
    std::string formatted;
    if (days > 0) {
        formatted = std::to_string(days) + "d " + std::to_string(hours) + "h " +
                   std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    } else if (hours > 0) {
        formatted = std::to_string(hours) + "h " + std::to_string(minutes) + "m " +
                   std::to_string(seconds) + "s";
    } else {
        formatted = std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
    
    auto now = std::chrono::system_clock::now();
    auto boot_time = now - std::chrono::seconds(uptime_seconds);
    auto boot_time_t = std::chrono::system_clock::to_time_t(boot_time);
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(boot_time.time_since_epoch()) % 1000000;
    
    std::ostringstream boot_ss;
    boot_ss << std::put_time(std::localtime(&boot_time_t), "%Y-%m-%dT%H:%M:%S");
    boot_ss << "." << std::setfill('0') << std::setw(6) << ms.count();
    
    return UptimeInfo{uptime_seconds, formatted, boot_ss.str()};
}

int main() {
    httplib::Server svr;
    
    // CORS middleware
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Content-Type", "application/json");
    });
    
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
    });
    
    // ============ HEALTH ============
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto devices_output = adb::devices();
            bool is_connected = false;
            
            if (devices_output) {
                std::istringstream iss(*devices_output);
                std::string line;
                while (std::getline(iss, line)) {
                    std::istringstream line_stream(line);
                    std::string device, status;
                    if (line_stream >> device >> status && status == "device") {
                        is_connected = true;
                        break;
                    }
                }
            }
            
            json response;
            response["status"] = is_connected ? "healthy" : "degraded";
            response["adb_connected"] = is_connected;
            response["timestamp"] = get_iso_timestamp();
            
            res.set_content(response.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 503;
        }
    });
    
    // ============ ROOT ============
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        json response;
        response["app"] = "DroidMetrics";
        response["by"] = "bluecape";
        response["version"] = "2.0.0";
        response["endpoints"] = {
            {"health", "/health"},
            {"device", "/device"},
            {"os", "/os"},
            {"cpu", "/cpu"},
            {"cpu_frequency", "/cpu/frequency"},
            {"cpu_governors", "/cpu/governors"},
            {"cpu_idle", "/cpu/idle"},
            {"memory", "/memory"},
            {"storage", "/storage"},
            {"mounts", "/storage/mounts"},
            {"battery", "/battery"},
            {"power", "/power"},
            {"thermal", "/thermal"},
            {"core_temperatures", "/thermal/cores"},
            {"network", "/network"},
            {"display", "/display"},
            {"uptime", "/uptime"},
            {"system", "/system"}
        };
        response["timestamp"] = get_iso_timestamp();
        
        res.set_content(response.dump(2), "application/json");
    });
    
    // ============ DEVICE ============
    svr.Get("/device", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("device_info", 300)) {
                res.set_content(get_cached("device_info"), "application/json");
                return;
            }
            
            auto device = build_device_info();
            json j = device;
            std::string content = j.dump(2);
            set_cached("device_info", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ OS ============
    svr.Get("/os", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("os_info", 300)) {
                res.set_content(get_cached("os_info"), "application/json");
                return;
            }
            
            auto os = build_os_info();
            json j = os;
            std::string content = j.dump(2);
            set_cached("os_info", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ CPU ============
    svr.Get("/cpu", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("cpu_info", 300)) {
                res.set_content(get_cached("cpu_info"), "application/json");
                return;
            }
            
            auto cpu = build_cpu_info();
            json j = cpu;
            std::string content = j.dump(2);
            set_cached("cpu_info", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ CPU FREQUENCY ============
    svr.Get("/cpu/frequency", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto cpu_freq = build_cpu_frequency();
            json j = cpu_freq;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ CPU GOVERNORS ============
    svr.Get("/cpu/governors", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("cpu_governors", 300)) {
                res.set_content(get_cached("cpu_governors"), "application/json");
                return;
            }
            
            auto cpu_gov = build_cpu_governors();
            json j = cpu_gov;
            std::string content = j.dump(2);
            set_cached("cpu_governors", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ CPU IDLE ============
    svr.Get("/cpu/idle", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto cpu_idle = build_cpu_idle_info();
            json j = cpu_idle;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ MEMORY ============
    svr.Get("/memory", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto memory = build_memory_info();
            json j = memory;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ STORAGE ============
    svr.Get("/storage", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto storage = build_storage_info();
            json j = storage;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ MOUNTS ============
    svr.Get("/storage/mounts", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("storage_mounts", 30)) {
                res.set_content(get_cached("storage_mounts"), "application/json");
                return;
            }
            
            auto mounts = build_storage_mounts();
            json j = mounts;
            std::string content = j.dump(2);
            set_cached("storage_mounts", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ BATTERY ============
    svr.Get("/battery", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto battery = build_battery_info();
            json j = battery;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ POWER ============
    svr.Get("/power", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto power = build_power_info();
            res.set_content(power.to_json().dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ THERMAL ============
    svr.Get("/thermal", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto thermal = build_thermal_info();
            json j = thermal;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ CORE TEMPERATURES ============
    svr.Get("/thermal/cores", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto core_temps = build_core_temperatures();
            json j = core_temps;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ NETWORK ============
    svr.Get("/network", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("network_info", 30)) {
                res.set_content(get_cached("network_info"), "application/json");
                return;
            }
            
            auto network = build_network_info();
            json j = network.to_json();
            std::string content = j.dump(2);
            set_cached("network_info", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ DISPLAY ============
    svr.Get("/display", [](const httplib::Request&, httplib::Response& res) {
        try {
            if (is_cached_valid("display_info", 300)) {
                res.set_content(get_cached("display_info"), "application/json");
                return;
            }
            
            auto display = build_display_info();
            json j = display;
            std::string content = j.dump(2);
            set_cached("display_info", content);
            res.set_content(content, "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ UPTIME ============
    svr.Get("/uptime", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto uptime = build_uptime_info();
            json j = uptime;
            res.set_content(j.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    // ============ SYSTEM ============
    svr.Get("/system", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto device = build_device_info();
            auto os = build_os_info();
            auto cpu = build_cpu_info();
            auto cpu_freq = build_cpu_frequency();
            auto cpu_gov = build_cpu_governors();
            auto cpu_idle = build_cpu_idle_info();
            auto memory = build_memory_info();
            auto storage = build_storage_info();
            auto mounts = build_storage_mounts();
            auto battery = build_battery_info();
            auto power = build_power_info();
            auto thermal = build_thermal_info();
            auto core_temps = build_core_temperatures();
            auto network = build_network_info();
            auto display = build_display_info();
            
            SystemInfo system{
                device, os, cpu, cpu_freq,
                std::optional<CPUGovernorInfo>(cpu_gov),
                std::optional<CPUIdleInfo>(cpu_idle),
                memory, storage,
                std::optional<std::vector<MountInfo>>(mounts),
                battery, power, thermal,
                std::optional<CoreTemperatures>(core_temps),
                network, display,
                get_iso_timestamp()
            };
            
            res.set_content(system.to_json().dump(2), "application/json");
        } catch (const std::exception& e) {
            json error;
            error["error"] = e.what();
            res.set_content(error.dump(2), "application/json");
            res.status = 500;
        }
    });
    
    std::cout << "DroidMetrics (by bluecape) listening on http://0.0.0.0:8000\n";
    std::cout << "API Root: http://localhost:8000/\n";
    
    svr.listen("0.0.0.0", 8000);
    
    return 0;
}
