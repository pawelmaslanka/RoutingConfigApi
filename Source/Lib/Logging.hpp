#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/common.h>

#include "StdLib.hpp"

namespace Log {
    using SpdLogger = spdlog::logger;
    using namespace StdLib;

    class ILoggingRegistryManagement {
    public:
        virtual ~ILoggingRegistryManagement() = default;

        virtual void RegisterModule(const String &moduleName) = 0;

        virtual SharedPtr<SpdLogger> Logger(const String &moduleName) = 0;
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
    };

    class LoggerRegistry : public ILoggingRegistryManagement {
    public:
        virtual ~LoggerRegistry() = default;

        LoggerRegistry(spdlog::sinks_init_list sinksList) : mSinksList(std::move(sinksList)) {
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

    private:
        Map<String, SharedPtr<SpdLogger> > mLoggerByModuleName;
        spdlog::sinks_init_list mSinksList;
    };
} // namespace Log
