#ifndef ADB_UTILS_HPP
#define ADB_UTILS_HPP

#include <string>
#include <vector>
#include <optional>

namespace adb {

// Helper function to escape strings for shell
std::string shell_escape(const std::string& str);

/**
 * Execute an adb shell command and return stdout.
 * Throws std::runtime_error if command fails.
 */
std::string shell(const std::string& cmd, bool throw_on_error = true);

/**
 * Check connected ADB devices.
 * Returns raw output from "adb devices".
 */
std::optional<std::string> devices();

/**
 * Execute multiple adb shell commands efficiently.
 * Uses marker lines to split outputs.
 */
std::vector<std::string> shell_multi(const std::vector<std::string>& cmds);

} // namespace adb

#endif // ADB_UTILS_HPP
