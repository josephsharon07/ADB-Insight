#include "adb_utils.hpp"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <array>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <unistd.h>

namespace adb {

std::string shell(const std::string& cmd, bool throw_on_error) {
    std::array<char, 256> buffer;
    std::string result;
    
    // Write command to temp file to avoid shell interpretation
    std::string temp_file = "/tmp/adb_cmd_" + std::to_string(getpid()) + ".sh";
    std::ofstream out(temp_file);
    out << cmd << "\n";
    out << "exit\n";
    out.close();
    
    std::string full_cmd = "adb shell < " + temp_file;
    
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        std::remove(temp_file.c_str());
        if (throw_on_error) {
            throw std::runtime_error("Failed to execute adb shell command");
        }
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int status = pclose(pipe);
    std::remove(temp_file.c_str());
    
    if (status != 0 && throw_on_error) {
        throw std::runtime_error("ADB command failed: " + cmd);
    }
    
    // Remove trailing whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    
    return result;
}

std::string shell_escape(const std::string& str) {
    std::string escaped = "'";
    for (char c : str) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

std::optional<std::string> devices() {
    try {
        std::array<char, 256> buffer;
        std::string result;
        
        FILE* pipe = popen("adb devices", "r");
        if (!pipe) {
            std::cerr << "Failed to check devices\n";
            return std::nullopt;
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        
        pclose(pipe);
        return result;
    } catch (...) {
        std::cerr << "Failed to check devices\n";
        return std::nullopt;
    }
}

std::vector<std::string> shell_multi(const std::vector<std::string>& cmds) {
    if (cmds.empty()) {
        return {};
    }
    
    std::string marker = "__ADB_MULTI__";
    std::string combined;
    
    for (size_t i = 0; i < cmds.size(); ++i) {
        combined += "echo " + marker + std::to_string(i) + "; " + cmds[i] + "; ";
    }
    
    std::string output;
    try {
        output = shell(combined, false);
    } catch (...) {
        return std::vector<std::string>(cmds.size(), "");
    }
    
    std::vector<std::string> results(cmds.size(), "");
    int current = -1;
    std::stringstream buffer;
    
    for (const auto& line : [&output]() {
        std::vector<std::string> lines;
        std::istringstream iss(output);
        std::string l;
        while (std::getline(iss, l)) {
            lines.push_back(l);
        }
        return lines;
    }()) {
        if (line.find(marker) == 0) {
            if (current >= 0) {
                std::string buffered = buffer.str();
                if (!buffered.empty() && buffered.back() == '\n') {
                    buffered.pop_back();
                }
                results[current] = buffered;
                buffer.str("");
                buffer.clear();
            }
            try {
                current = std::stoi(line.substr(marker.length()));
            } catch (...) {
                current++;
            }
        } else if (current >= 0) {
            buffer << line << "\n";
        }
    }
    
    if (current >= 0) {
        std::string buffered = buffer.str();
        if (!buffered.empty() && buffered.back() == '\n') {
            buffered.pop_back();
        }
        results[current] = buffered;
    }
    
    return results;
}

} // namespace adb
