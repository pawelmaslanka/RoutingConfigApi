#pragma once

#include "Logging.hpp"
#include "StdLib.hpp"

class ModuleRegistry {
public:
    ModuleRegistry()
      : mLoggerRegistry { std::make_shared<Log::NullLoggerRegistryManagement>() } {
        // Nothing more to do
    }

    inline const StdLib::SharedPtr<Log::ILoggingRegistryManagement> LoggerRegistry() const { return mLoggerRegistry; }
    inline void SetLoggerRegistry(StdLib::SharedPtr<Log::ILoggingRegistryManagement> loggerRegistry) { mLoggerRegistry = loggerRegistry; }

private:
    StdLib::SharedPtr<Log::ILoggingRegistryManagement> mLoggerRegistry;
};
