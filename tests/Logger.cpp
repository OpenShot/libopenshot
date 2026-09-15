// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Logger.h"
#include "ZmqLogger.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>

using namespace openshot;

TEST_CASE("Independent logging destinations and crash writes", "[logger]") {
    auto* logger = Logger::Instance();
    CHECK(logger == ZmqLogger::Instance());
    auto path = std::filesystem::temp_directory_path() /
        std::filesystem::u8path("openshot-logging-\xc3\xa9-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
    logger->Path(path.u8string());
    logger->SetFileLevel("warning");
    logger->SetConsoleLevel("debug");
    CHECK(logger->ShouldLog(Logger::LevelDebug));
    std::ostringstream console;
    auto* previous = std::clog.rdbuf(console.rdbuf());
    logger->Log("console-only-debug", Logger::LevelDebug);
    logger->Log("both-warning", Logger::LevelWarning);
    std::clog.rdbuf(previous);
    logger->SetFileLevel("off");
    logger->SetConsoleLevel("off");
    CHECK_FALSE(logger->ShouldLog(Logger::LevelCritical));
    logger->Log("filtered-error", Logger::LevelError);
    logger->LogToFile("---- Unhandled Exception: Stack Trace ----\ncrash-evidence\n---- End of Stack Trace ----\n");
    CHECK_THROWS_AS(logger->SetFileLevel("not-a-level"), std::invalid_argument);
    logger->Close();
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), {});
    CHECK(content.find("console-only-debug") == std::string::npos);
    CHECK(console.str().find("console-only-debug") != std::string::npos);
    CHECK(content.find("both-warning") != std::string::npos);
    CHECK(content.find("filtered-error") == std::string::npos);
    CHECK(content.find("crash-evidence") != std::string::npos);
    CHECK(content.find("libopenshot logging:") != std::string::npos);
    file.close();
    std::filesystem::remove(path);
}

TEST_CASE("Concurrent records remain complete and path can reopen", "[logger]") {
    auto* logger = Logger::Instance();
    auto path = std::filesystem::temp_directory_path() /
        ("openshot-logging-threads-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
    logger->Path(path.u8string());
    logger->SetFileLevel("debug");
    logger->SetConsoleLevel("off");
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([logger, i]() {
        for (int j = 0; j < 100; ++j)
            logger->Log("record-" + std::to_string(i) + "-" + std::to_string(j));
    });
    for (auto& worker : workers) worker.join();
    logger->Close();
    logger->Path(path.u8string());
    logger->Enable(true);
    logger->AppendDebugMethod("reopened", "frame", 42);
    logger->Close();
    std::ifstream file(path);
    std::string line;
    int records = 0;
    bool reopened = false;
    while (std::getline(file, line)) {
        if (line.find("record-") != std::string::npos) {
            ++records;
            CHECK(line.find("record-", line.find("record-") + 1) == std::string::npos);
        }
        if (line.find("reopened (frame=42.0000)") != std::string::npos) reopened = true;
    }
    CHECK(records == 400);
    CHECK(reopened);
    file.close();
    std::filesystem::remove(path);
}
