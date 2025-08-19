/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "IConfigManagement.hpp"
#include "JsonCommon.hpp"
#include "Lib/ModuleRegistry.hpp"
#include "Modules.hpp"

namespace Config {
using namespace StdLib;
class JsonConfigManager : public IConfigManagement {
public:
    explicit JsonConfigManager(SharedPtr<Storage::IDataStorage> dataStorage, const SharedPtr<ModuleRegistry>& moduleRegistry)
      : mDataStorage(dataStorage), mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::CONFIG_MNGMT)) {}
    JsonConfigManager(const JsonConfigManager&) = default;
    virtual ~JsonConfigManager() = default;
    bool LoadConfig() override {
        try {
            auto configData = mDataStorage->LoadData();
            if (!configData.has_value()) {
                mLog->error("Failed to load JSON config data from '{}'", mDataStorage->URI());
                return false;
            }

            auto jConfig = Json::JSON::parse(configData.value());
            mJsonConfig = jConfig;
            mIsConfigLoaded = true;
            mLog->trace("Successfully loaded JSON config from file '{}':\n{}", mDataStorage->URI(), jConfig.dump(Json::DEFAULT_OUTPUT_INDENT));
            return true;
        }
        catch (const Exception &ex) {
            mLog->error("Failed to load JSON config from file '{}'. Error: {}", mDataStorage->URI(), ex.what());
        }

        return false;
    }

    Optional<ByteStream> SerializeConfig() override {
        if (!mIsConfigLoaded) {
            mLog->error("JSON config has not been loaded yet");
            return {};
        }

        String jdata = mJsonConfig.dump();
        return ByteStream(jdata.begin(), jdata.end());
    }

    Optional<ByteStream> MakeDiff(const ByteStream& otherConfig) override {
        if (!mIsConfigLoaded) {
            mLog->error("JSON config has not been loaded yet");
            return {};
        }

        if (otherConfig.size() == 0) {
            mLog->error("New JSON config to create diff is empty");
            return {};
        }

        try {
            auto jNewConfig = Json::JSON::parse(otherConfig);
            // Make diff between origin and new config
            auto jDiff = Json::JSON::diff(mJsonConfig, jNewConfig);
            String jData = jDiff.dump();
            mLog->trace("Successfully make diff for requested config:\n{}", jDiff.dump(Json::DEFAULT_OUTPUT_INDENT));
            return ByteStream(jData.begin(), jData.end());
        }
        catch (const Exception &ex) {
            mLog->error("Failed to make JSON diff for requested data. Error: '{}'", ex.what());
        }

        return {};
    }

    bool ApplyPatch(const ByteStream& patch) override {
        try {
            auto jPatch = Json::JSON().parse(patch);
            mJsonConfig.patch_inplace(jPatch);
            return true;
        }
        catch (Exception& ex) {
            mLog->error("Failed to apply patch due to caught exception. Error: {}", ex.what());
        }
        catch (...) {
            mLog->error("Caught unhandled exception during applying patch");
        }

        return false;
    }

private:
    Json::JSON mJsonConfig;
    SharedPtr<Storage::IDataStorage> mDataStorage;
    SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;
    bool mIsConfigLoaded = false;
}; // class JsonConfigManager
} // namespace Config
