#ifndef PARSERS_HPP
#define PARSERS_HPP

#include <string>
#include <vector>
#include <map>
#include "models.hpp"

namespace parsers {

// Parse key:value blocks
std::map<std::string, std::string> parse_key_value_block(const std::string& text);

// Parse CPU frequency output
std::map<std::string, int> parse_cpu_freq(const std::string& text);

// Parse detailed CPU frequencies
struct CPUFreqData {
    std::map<std::string, int> per_core;
    int min_khz;
    int max_khz;
    double min_mhz;
    double max_mhz;
    double avg_mhz;
    int core_count;
    bool error;
};
CPUFreqData parse_cpu_frequencies_detailed(const std::string& text);

// Parse thermal data from dumpsys
std::map<std::string, std::map<std::string, double>> parse_thermal_data(const std::string& text);

// Parse battery data
struct BatteryData {
    int level;
    std::string health;
    std::string status;
    int voltage_mv;
    double temperature_c;
    std::string technology;
    bool is_charging;
};
BatteryData parse_battery_level(const std::map<std::string, std::string>& data);

// Parse df output
std::vector<MountInfo> parse_df_output(const std::string& text);

// Parse CPU idle state output
std::map<std::string, std::vector<CPUIdleState>> parse_cpu_idle_output(const std::string& text);

// Parse path:value blocks
std::map<std::string, std::string> parse_path_value_block(const std::string& text);

// Unit conversions
double kb_to_mb(const std::string& value);
double kb_to_gb(int value);

// Parse power info
PowerInfo parse_power_info(const std::map<std::string, std::string>& battery_data);

} // namespace parsers

#endif // PARSERS_HPP
