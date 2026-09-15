// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_LOGGER_H
#define OPENSHOT_LOGGER_H

#include <atomic>
#include <fstream>
#include <mutex>
#include <string>

namespace openshot {
class Settings;
/// Independent file and stderr logging. No networking or worker thread.
class Logger {
public:
    enum Level { LevelDebug = 10, LevelInfo = 20, LevelWarning = 30, LevelError = 40, LevelCritical = 50, LevelOff = 100 };
    static Logger* Instance();
    /// Explicit configuration overrides environment defaults. Invalid levels throw.
    void SetFileLevel(std::string level);
    void SetConsoleLevel(std::string level);
    bool ShouldLog(Level level = LevelDebug) const;
    void Path(std::string path);
    void Close();
    void Log(std::string message, Level level = LevelDebug);
    /// Unfiltered raw output, retained for the existing crash handler.
    void LogToFile(std::string message);
    /// Compatibility: enable debug file output, or disable ordinary file output.
    void Enable(bool enabled);
    /// Deprecated compatibility no-op; logging no longer uses connections.
    void Connection(std::string connection) {}
    void AppendDebugMethod(std::string method_name,
        std::string arg1_name="", float arg1_value=-1.0,
        std::string arg2_name="", float arg2_value=-1.0,
        std::string arg3_name="", float arg3_value=-1.0,
        std::string arg4_name="", float arg4_value=-1.0,
        std::string arg5_name="", float arg5_value=-1.0,
        std::string arg6_name="", float arg6_value=-1.0);
private:
    Logger();
    Settings* legacy_settings;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    std::recursive_mutex loggerMutex;
    std::ofstream log_file;
    std::string file_path;
    std::atomic<int> file_level{LevelInfo};
    std::atomic<int> console_level{LevelInfo};
    std::atomic<bool> file_open{false};
    std::atomic<bool> console_configured{false};
    int ConsoleLevel() const;
};
}
#endif
