// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Logger.h"
#include "ZmqLogger.h"
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace openshot;

TEST_CASE("Independent logging destinations and crash writes", "[logger]") {
    auto* logger = Logger::Instance();
    CHECK(logger == ZmqLogger::Instance());
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    auto path = directory.filePath(QString::fromUtf8("openshot-logging-\xc3\xa9.log"));
    logger->Path(path.toUtf8().toStdString());
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
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    std::string content = file.readAll().toStdString();
    CHECK(content.find("console-only-debug") == std::string::npos);
    CHECK(console.str().find("console-only-debug") != std::string::npos);
    CHECK(content.find("both-warning") != std::string::npos);
    CHECK(content.find("filtered-error") == std::string::npos);
    CHECK(content.find("crash-evidence") != std::string::npos);
    CHECK(content.find("libopenshot logging:") != std::string::npos);
    file.close();
}

TEST_CASE("Concurrent records remain complete and path can reopen", "[logger]") {
    auto* logger = Logger::Instance();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    auto path = directory.filePath("openshot-logging-threads.log");
    logger->Path(path.toUtf8().toStdString());
    logger->SetFileLevel("debug");
    logger->SetConsoleLevel("off");
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([logger, i]() {
        for (int j = 0; j < 100; ++j)
            logger->Log("record-" + std::to_string(i) + "-" + std::to_string(j));
    });
    for (auto& worker : workers) worker.join();
    logger->Close();
    logger->Path(path.toUtf8().toStdString());
    logger->Enable(true);
    logger->AppendDebugMethod("reopened", "frame", 42);
    logger->Close();
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    std::istringstream content(file.readAll().toStdString());
    std::string line;
    int records = 0;
    bool reopened = false;
    while (std::getline(content, line)) {
        if (line.find("record-") != std::string::npos) {
            ++records;
            CHECK(line.find("record-", line.find("record-") + 1) == std::string::npos);
        }
        if (line.find("reopened (frame=42.0000)") != std::string::npos) reopened = true;
    }
    CHECK(records == 400);
    CHECK(reopened);
    file.close();
}
