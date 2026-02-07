#include "parsers.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <cmath>

namespace parsers {

std::map<std::string, std::string> parse_key_value_block(const std::string& text) {
    std::map<std::string, std::string> data;
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\n\r"));
            key.erase(key.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);
            
            if (!key.empty() && !value.empty()) {
                data[key] = value;
            }
        }
    }
    
    return data;
}

std::map<std::string, int> parse_cpu_freq(const std::string& text) {
    std::map<std::string, int> freqs;
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string path = line.substr(0, pos);
            std::string value_str = line.substr(pos + 1);
            
            // Find CPU part like cpu0, cpu1, etc.
            std::regex cpu_regex(R"(cpu(\d+))");
            std::smatch match;
            if (std::regex_search(path, match, cpu_regex)) {
                std::string cpu_part = "cpu" + match[1].str();
                try {
                    value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
                    int freq = std::stoi(value_str);
                    freqs[cpu_part] = freq;
                } catch (...) {
                    continue;
                }
            }
        }
    }
    
    return freqs;
}

CPUFreqData parse_cpu_frequencies_detailed(const std::string& text) {
    CPUFreqData result;
    result.error = false;
    
    result.per_core = parse_cpu_freq(text);
    
    if (result.per_core.empty()) {
        result.error = true;
        return result;
    }
    
    std::vector<int> freq_values;
    for (const auto& [_, freq] : result.per_core) {
        freq_values.push_back(freq);
    }
    
    if (!freq_values.empty()) {
        result.min_khz = *std::min_element(freq_values.begin(), freq_values.end());
        result.max_khz = *std::max_element(freq_values.begin(), freq_values.end());
        result.min_mhz = std::round((result.min_khz / 1000.0) * 100) / 100;
        result.max_mhz = std::round((result.max_khz / 1000.0) * 100) / 100;
        
        double sum = 0;
        for (int f : freq_values) sum += f;
        result.avg_mhz = std::round((sum / freq_values.size() / 1000.0) * 100) / 100;
        result.core_count = freq_values.size();
    }
    
    return result;
}

std::map<std::string, std::map<std::string, double>> parse_thermal_data(const std::string& text) {
    std::map<std::string, std::map<std::string, double>> temps;
    std::regex temp_regex(R"(Temperature\{([^}]+)\})");
    std::sregex_iterator iter(text.begin(), text.end(), temp_regex);
    std::sregex_iterator end;
    
    while (iter != end) {
        std::string content = (*iter)[1].str();
        std::map<std::string, std::string> temp_data;
        
        std::regex item_regex(R"((\w+)=([^,}]+))");
        std::sregex_iterator item_iter(content.begin(), content.end(), item_regex);
        
        while (item_iter != end) {
            std::string key = (*item_iter)[1].str();
            std::string value = (*item_iter)[2].str();
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            temp_data[key] = value;
            ++item_iter;
        }
        
        if (temp_data.count("mName")) {
            std::string name = temp_data["mName"];
            try {
                double value = std::stod(temp_data["mValue"]);
                temps[name]["value"] = value;
                if (temp_data.count("mType")) {
                    temps[name]["type"] = std::stod(temp_data["mType"]);
                }
                if (temp_data.count("mStatus")) {
                    temps[name]["status"] = std::stod(temp_data["mStatus"]);
                }
            } catch (...) {
                continue;
            }
        }
        
        ++iter;
    }
    
    return temps;
}

BatteryData parse_battery_level(const std::map<std::string, std::string>& data) {
    BatteryData result;
    
    result.level = 0;
    result.health = "unknown";
    result.status = "unknown";
    result.voltage_mv = 0;
    result.temperature_c = 0.0;
    result.technology = "unknown";
    result.is_charging = false;
    
    if (data.count("level")) {
        try { result.level = std::stoi(data.at("level")); } catch (...) {}
    }
    if (data.count("health")) {
        result.health = data.at("health");
    }
    if (data.count("status")) {
        result.status = data.at("status");
    }
    if (data.count("voltage")) {
        try { result.voltage_mv = std::stoi(data.at("voltage")); } catch (...) {}
    }
    if (data.count("temperature")) {
        try {
            int temp = std::stoi(data.at("temperature"));
            result.temperature_c = std::round((temp / 10.0) * 10) / 10;
        } catch (...) {}
    }
    if (data.count("technology")) {
        result.technology = data.at("technology");
    }
    
    bool ac_powered = data.count("AC powered") && data.at("AC powered") == "true";
    bool usb_powered = data.count("USB powered") && data.at("USB powered") == "true";
    result.is_charging = ac_powered || usb_powered;
    
    return result;
}

std::vector<MountInfo> parse_df_output(const std::string& text) {
    std::vector<MountInfo> mounts;
    std::istringstream iss(text);
    std::string line;
    
    bool first = true;
    while (std::getline(iss, line)) {
        if (first) {
            first = false;
            continue;
        }
        
        std::istringstream line_stream(line);
        std::string filesystem, size_str, used_str, avail_str, use_percent_str;
        std::string mountpoint;
        
        if (!(line_stream >> filesystem >> size_str >> used_str >> avail_str >> use_percent_str)) {
            continue;
        }
        
        std::string mp;
        while (line_stream >> mp) {
            if (!mountpoint.empty()) mountpoint += " ";
            mountpoint += mp;
        }
        
        try {
            MountInfo m;
            m.filesystem = filesystem;
            m.size_kb = std::stoi(size_str);
            m.used_kb = std::stoi(used_str);
            m.available_kb = std::stoi(avail_str);
            m.use_percent = std::stoi(use_percent_str.substr(0, use_percent_str.find('%')));
            m.mountpoint = mountpoint;
            mounts.push_back(m);
        } catch (...) {
            continue;
        }
    }
    
    return mounts;
}

std::map<std::string, std::vector<CPUIdleState>> parse_cpu_idle_output(const std::string& text) {
    std::map<std::string, std::vector<CPUIdleState>> per_core;
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        std::istringstream line_stream(line);
        std::string cpu, state, name;
        int64_t time_us;
        int usage;
        
        if (!(line_stream >> cpu >> state >> name >> time_us >> usage)) {
            continue;
        }
        
        CPUIdleState idle_state;
        idle_state.state = state;
        idle_state.name = name;
        idle_state.time_us = time_us;
        idle_state.usage = usage;
        
        per_core[cpu].push_back(idle_state);
    }
    
    return per_core;
}

std::map<std::string, std::string> parse_path_value_block(const std::string& text) {
    std::map<std::string, std::string> values;
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string path = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Find CPU part like cpu0, cpu1, etc.
            std::regex cpu_regex(R"(cpu(\d+))");
            std::smatch match;
            if (std::regex_search(path, match, cpu_regex)) {
                std::string cpu_part = "cpu" + match[1].str();
                value.erase(0, value.find_first_not_of(" \t\n\r"));
                value.erase(value.find_last_not_of(" \t\n\r") + 1);
                values[cpu_part] = value;
            }
        }
    }
    
    return values;
}

double kb_to_mb(const std::string& value) {
    try {
        return std::round((std::stoi(value) / 1024.0) * 100) / 100;
    } catch (...) {
        return 0.0;
    }
}

double kb_to_gb(int value) {
    return std::round((value / (1024.0 * 1024.0)) * 100) / 100;
}

PowerInfo parse_power_info(const std::map<std::string, std::string>& battery_data) {
    PowerInfo info;
    info.current_ma = 0;
    info.charge_counter = std::nullopt;
    info.max_charging_current = std::nullopt;
    info.charging_status = "unknown";
    
    // Extract current_now (in mA)
    if (battery_data.count("current now")) {
        try {
            info.current_ma = std::stoi(battery_data.at("current now"));
        } catch (...) {}
    }
    
    // Extract charge_counter
    if (battery_data.count("Charge counter")) {
        try {
            info.charge_counter = std::stoi(battery_data.at("Charge counter"));
        } catch (...) {}
    }
    
    // Extract max_charging_current
    if (battery_data.count("Max charging current")) {
        try {
            info.max_charging_current = std::stoi(battery_data.at("Max charging current"));
        } catch (...) {}
    }
    
    // Determine charging status
    if (battery_data.count("status")) {
        std::string status = battery_data.at("status");
        std::string status_lower = status;
        std::transform(status_lower.begin(), status_lower.end(), status_lower.begin(), ::tolower);

        if (status_lower == "charging" || status == "2") {
            info.charging_status = "charging";
        } else if (status_lower == "discharging" || status == "3") {
            info.charging_status = "discharging";
        } else if (status_lower == "not charging" || status == "4") {
            info.charging_status = "not_charging";
        } else if (status_lower == "full" || status == "5") {
            info.charging_status = "full";
        }
    }
    
    return info;
}

} // namespace parsers
