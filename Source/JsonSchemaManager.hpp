/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "ISchemaManagement.hpp"
#include "IDataStorage.hpp"
#include "JsonCommon.hpp"
#include "JsonFileStorage.hpp"
#include "Lib/ModuleRegistry.hpp"
#include "Modules.hpp"

#include <nlohmann/json-schema.hpp>

#include <fstream>
#include <sstream>

namespace Schema {
using namespace StdLib;
class JsonSchemaManager : public ISchemaManagement {
public:
    explicit JsonSchemaManager(SharedPtr<Storage::IDataStorage> dataStorage, const SharedPtr<ModuleRegistry>& moduleRegistry)
      : mValidator(nullptr, nlohmann::json_schema::default_string_format_check), mDataStorage(dataStorage), mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::SCHEMA_MNGMT)) {}
    bool LoadSchema() override {
        try {
            auto schemaData = mDataStorage->LoadData();
            if (!schemaData.has_value()) {
                mLog->error("Failed to load JSON schema data from '{}'", mDataStorage->URI());
                return false;
            }

            Json::JSON jSchema = Json::JSON::parse(schemaData.value());
            mValidator.set_root_schema(jSchema);
            mIsSchemaLoaded = true;
            mLog->trace("Successfully loaded JSON schema from file {}:\n{}", mDataStorage->URI(), jSchema.dump(Json::DEFAULT_OUTPUT_INDENT));
        }
        catch (const Exception &ex) {
            mLog->error("Failed to load JSON schema from file {}. Error: {}", mDataStorage->URI(), ex.what());
            return false;
        }

        return true;
    }

    bool ValidateData(const ByteStream& data) override {
        if (!mIsSchemaLoaded) {
            mLog->error("Failed to validate data against the schema. Error: The schema has not been loaded yet");
            return false;
        }

        try {
            auto jdata = Json::JSON::parse(data);
            ErrorHandler err(mLog);
            mValidator.validate(jdata, err);
            if (err) {
                mLog->error("Failed to validate data against schema. Error: {}", err.MsgError());
		        return false;
            }
            mLog->trace("Successfuly validated data against schema. Data:\n{}", jdata.dump(Json::DEFAULT_OUTPUT_INDENT));
        }
        catch (const nlohmann::json_schema::basic_error_handler &ex) {
            mLog->error("Caught non standard expecption.");
            return false;
        }
        catch (const Exception &ex) {
            mLog->error("Failed to validate data against schema. Error: {}", ex.what());
            return false;
        }

        return true;
    }

private:
    nlohmann::json_schema::json_validator mValidator;
    SharedPtr<Storage::IDataStorage> mDataStorage;
    SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;
    bool mIsSchemaLoaded = false;

    class ErrorHandler : public nlohmann::json_schema::basic_error_handler {
    public:
        ErrorHandler(SharedPtr<Log::SpdLogger> logger) : mLog(logger) {}
        String MsgError() { return osstream.str(); }
    
    private:
        void error(const Json::JSON::json_pointer &ptr, const nlohmann::json &instance, const std::string &message) override {
            nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
            osstream << "'" << ptr << "' >> '" << instance << "': " << message << "\n";
            // 1. Check if it is oneOf
            // 2. If there in not an error 'not found in object', it points out correct oneOf entry which missing attribute
            if (message.find("case#0") != String::npos) {
                mLog->trace("{}", osstream.str());
            }
        }

        SharedPtr<Log::SpdLogger> mLog;
        std::ostringstream osstream;
    };
}; // JsonSchemaManager
} // namespace Schema
