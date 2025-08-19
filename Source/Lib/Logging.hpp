/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/common.h>

#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/null_sink.h>

#include "StdLib.hpp"

namespace Log {
    using SpdLogger = spdlog::logger;
    using SpdSink = spdlog::sinks::sink;
    using namespace StdLib;

    class ILoggingRegistryManagement {
    public:
        virtual ~ILoggingRegistryManagement() = default;
        virtual void RegisterModule(const String &moduleName) = 0;
        virtual SharedPtr<SpdLogger> Logger(const String &moduleName) = 0;
        virtual void AddLogSink(SharedPtr<SpdSink> sink) = 0;
    };

    class NullLoggerRegistryManagement : public ILoggingRegistryManagement {
    public:
        ~NullLoggerRegistryManagement() override = default;

        virtual void RegisterModule([[maybe_unused]] const String &) override {
        };

        virtual SharedPtr<SpdLogger> Logger([[maybe_unused]] const String &) override {
            static SharedPtr<SpdLogger> logger = std::make_shared<SpdLogger>("");
            return logger;
        };

        virtual void AddLogSink([[maybe_unused]] SharedPtr<SpdSink>) override { }
    };

    class LoggerRegistry : public ILoggingRegistryManagement {
    public:
        virtual ~LoggerRegistry() = default;

        LoggerRegistry(spdlog::sinks_init_list sinksList) : mSinksList(std::make_shared<spdlog::sinks::dist_sink_mt>()) {
            mSinksList->set_sinks(std::move(sinksList));
        }

        virtual void RegisterModule(const String &moduleName) override {
            mLoggerByModuleName[moduleName] = std::make_shared<SpdLogger>(moduleName, mSinksList);
        }

        virtual SharedPtr<SpdLogger> Logger(const String &moduleName) override {
            auto loggerIt = mLoggerByModuleName.find(moduleName);
            if (loggerIt == mLoggerByModuleName.end()) {
                // If currently the logger has not been registered yet then create new one and turn off logging possibility
                mLoggerByModuleName[moduleName] = std::make_shared<SpdLogger>(moduleName);
                loggerIt = mLoggerByModuleName.find(moduleName);
                loggerIt->second->set_level(spdlog::level::off);
            }

            return loggerIt->second;
        }

        virtual void AddLogSink(spdlog::sink_ptr sink) override {
            mSinksList->add_sink(sink);
        }

    private:
        Map<String, SharedPtr<SpdLogger>> mLoggerByModuleName;
        SharedPtr<spdlog::sinks::dist_sink_mt> mSinksList; // We use dist_sink to add "new sink" after creating logger
    };
} // namespace Log
