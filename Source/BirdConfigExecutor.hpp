/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "IConfigExecuting.hpp"

#include "defer/defer.h"
#include "FileStorage.hpp"
#include "Lib/ModuleRegistry.hpp"
#include "Modules.hpp"

#include <subprocess.h/subprocess.h>

namespace Config {
namespace Executing {
class BirdConfigExecutor : public IConfigExecuting {
public:
    BirdConfigExecutor(const SharedPtr<Storage::IDataStorage> config, const String& birdcExecCmd, const SharedPtr<ModuleRegistry>& moduleRegistry)
      : IConfigExecuting(config), mBirdcExecCmd(birdcExecCmd), mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::CONFIG_EXEC)) {}
    virtual ~BirdConfigExecutor() = default;
    bool Validate() override {
        if (!IsSupportedConfigStorage()) {
            return false;
        }

        // Combine full execution command, e.g.: /opt/podman/bin/podman exec -it bird birdc configure check \"/etc/bird/bird.conf\"
        auto birdcExecCmd = mBirdcExecCmd + " configure check \"/etc/bird/" + mConfig->URI() + "\"";
        mLog->trace("Validation command to execute: '{}'", birdcExecCmd);
        return ExecuteCmdAndMatchForExpectedOutput(birdcExecCmd, { "Configuration OK" });
    }

    bool Load() override {
        if (!IsSupportedConfigStorage()) {
            return false;
        }

        // Combine full execution command, e.g.: /opt/podman/bin/podman exec -it bird birdc configure \"/etc/bird/bird.conf\"
        auto birdcExecCmd = mBirdcExecCmd + " configure \"/etc/bird/" + mConfig->URI() + "\"";
        mLog->trace("Loading config command to execute: '{}'", birdcExecCmd);
        return ExecuteCmdAndMatchForExpectedOutput(birdcExecCmd, { "Reconfiguration in progress", "Reconfigured" });
    }

    bool Rollback([[maybe_unused]] const SharedPtr<Storage::IDataStorage> backupConfig) override {
        if (!IsSupportedConfigStorage()) {
            return false;
        }

        // Combine full execution command, e.g.: /opt/podman/bin/podman exec -it bird birdc configure undo
        auto birdcExecCmd = mBirdcExecCmd + " configure undo";
        mLog->trace("Rollback command to execute: '{}'", birdcExecCmd);
        return ExecuteCmdAndMatchForExpectedOutput(birdcExecCmd, { "Reconfiguration in progress", "Reconfigured" });
    }

private:
    const String mBirdcExecCmd;
    const SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;

    bool IsSupportedConfigStorage() {
        if (dynamic_pointer_cast<Storage::FileStorage>(mConfig)) {
            return true;
        }

        mLog->error("Only config stored as a file is supported");
        return false;
    }

    bool ExecuteCmdAndMatchForExpectedOutput(const String& cmd, const ForwardList<String>& matchOutput) {
        auto cmdArguments = Utils::fSplitStringByWhitespace(cmd);
        Vector<const char*> nullTermCmdArgs(cmdArguments.size() + 1, nullptr);
        for (size_t i = 0; i < cmdArguments.size(); ++i) {
            nullTermCmdArgs[i] = cmdArguments[i].c_str();
            mLog->trace("Bird arg: '{}'", nullTermCmdArgs[i]);
        }

        subprocess_s process;
        auto result = subprocess_create(nullTermCmdArgs.data(), 0, &process);
        if (result != 0) {
            mLog->error("Failed to create subprocess '{}'", cmd);
            return false;
        }

        DEFER({
            auto result = subprocess_destroy(&process);
            if (result != 0) {
                mLog->error("Failed to destroy process '{}'", cmd);
            }

            mLog->trace("Successfully finished spawned process '{}'", cmd);
        });

        int processStatus;
        result = subprocess_join(&process, &processStatus);
        if (result != 0) {
            mLog->error("Failed to join spawned process '{}'", cmd);
            return false;
        }

        if (processStatus != EXIT_SUCCESS) {
            mLog->error("Failed to execute process '{}'. Returned process status: {}", cmd, processStatus);
            return false;
        }

        mLog->trace("Process return status code: {}", processStatus);
        ::FILE* processStdout = subprocess_stdout(&process);
        char output[1024];
        while (::fgets(output, sizeof(output), processStdout)) {
            mLog->trace("Output line from process: '{}'", output);
            if (std::find_if(matchOutput.begin(), matchOutput.end(),
                [&output](const String& match) {
                    return String(output).find(match) != String::npos;
                }) != matchOutput.end()) {
                return true;
            }
        }

        ::FILE* processStderr = subprocess_stderr(&process);
        while (::fgets(output, sizeof(output), processStderr)) {
            mLog->error("Output line from process: '{}'", output);
        }

        return false;
    }
}; // class BirdConfigExecutor 

} // namespace Executing
} // namespace Config
