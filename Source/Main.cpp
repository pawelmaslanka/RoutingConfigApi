#include "BirdConfigConverter.hpp"
#include "BirdConfigExecutor.hpp"
#include "ConnectionManagement.hpp"
#include "FileStorage.hpp"
#include "HttpCommon.hpp"
#include "JsonConfigManager.hpp"
#include "JsonFileStorage.hpp"
#include "JsonSchemaManager.hpp"
#include "Modules.hpp"
#include "Lib/Utils.hpp"

#include "args/args.hxx"
#include "httplib/httplib.h"
#include "subprocess.h/subprocess.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <iterator>

namespace Std = StdLib;

/// NOTE: During passing argument candidateConfigMngr to fSetupServerRequestHandlers() it should contains the same content what runningConfigMngr
bool fSetupServerRequestHandlers(Std::SharedPtr<ConnectionManagement::Server>& cm, Std::SharedPtr<Config::IConfigManagement>& runningConfigMngr, Std::SharedPtr<Config::IConfigManagement>& candidateConfigMngr, Std::SharedPtr<Schema::ISchemaManagement> schemaMngr, Std::SharedPtr<Storage::IDataStorage>& runningConfigStorage, Std::SharedPtr<Storage::IDataStorage>& targetConfigStorage, Std::SharedPtr<Config::IConfigConverting> configConverter, Std::SharedPtr<Config::Executing::IConfigExecuting>& targetConfigExecutor, const Std::SharedPtr<ModuleRegistry>& moduleRegistry) {
    cm->addOnPostConnectionHandler("config_running_update", [&runningConfigMngr, &candidateConfigMngr, schemaMngr, runningConfigStorage, targetConfigStorage, configConverter, targetConfigExecutor, moduleRegistry](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST method: {}", path, data_request);
        // NOTE: Register new instance of class derived from Config::IConfigManagement,
        //       or consider refactoring this function and use template parameter do determine instance of class derived from Config::IConfigManagement
        if (!candidateConfigMngr) {
            if (auto jsonBasedConfigMngr = dynamic_pointer_cast<Config::JsonConfigManager>(runningConfigMngr)){
                candidateConfigMngr = std::make_shared<Config::JsonConfigManager>(*jsonBasedConfigMngr);
            }
            else {
                spdlog::error("Unsupported type of derived class from Config::IConfigManagement");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        ByteStream patchData(data_request.begin(), data_request.end());
        if (!candidateConfigMngr->ApplyPatch(patchData)) {
            spdlog::error("Failed to apply patch to running config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            spdlog::error("Failed to serialize candidate config");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!schemaMngr->ValidateData(configData.value())) {
            spdlog::error("Failed to validate candidate config data against its schema");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto targetConfigData = configConverter->Convert(configData.value());
        if (!targetConfigData.has_value()) {
            spdlog::error("Failed to convert native config into target config");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!targetConfigStorage->SaveData(targetConfigData.value())) {
            spdlog::error("Failed to save taget config into file {}", targetConfigStorage->URI());
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (configConverter) {
            if (!targetConfigExecutor->Validate()) {
                spdlog::error("Failed to validate candidate config by external program");
                candidateConfigMngr = nullptr;
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_running_get", [&runningConfigMngr](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request running on {} with GET method: {}", path, data_request);
        auto configData = runningConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            spdlog::error("Failed to serialize config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream configDataStr;
        for (const auto c : configData.value()) {
            configDataStr << static_cast<char>(c);
        }
    
        return_data = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnPostConnectionHandler("config_running_diff", [&runningConfigMngr, &schemaMngr](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST diff method: {}", path, data_request);
        ByteStream otherConfigData(data_request.begin(), data_request.end());
        if (!schemaMngr->ValidateData(otherConfigData)) {
            spdlog::error("Failed to validate other config data against its schema");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto patchData = runningConfigMngr->MakeDiff(otherConfigData);
        if (!patchData.has_value()) {
            spdlog::error("Failed to make a diff between running config and other config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream patchDataStr;
        std::transform(patchData.value().begin(), patchData.value().end(), std::ostream_iterator<char>(patchDataStr, ""), [](const Byte b) { return static_cast<char>(b); });
        return_data = patchDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_candidate_get", [&candidateConfigMngr](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with GET method: {}", path, data_request);
        if (!candidateConfigMngr) {
            spdlog::error("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            spdlog::error("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        Std::OStrStream configDataStr;
        std::transform(configData.value().begin(), configData.value().end(), std::ostream_iterator<char>(configDataStr, ""), [](const Byte b) { return static_cast<char>(b); });    
        return_data = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnPutConnectionHandler("config_candidate_apply", [&runningConfigMngr, &candidateConfigMngr, &runningConfigStorage, targetConfigExecutor](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with PUT method: {}", path, data_request);
        if (!candidateConfigMngr) {
            spdlog::trace("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto candidateConfigData = candidateConfigMngr->SerializeConfig();
        if (!candidateConfigData.has_value()) {
            spdlog::error("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            if (!targetConfigExecutor->Load()) {
                spdlog::error("Failed to load candidate config by external program");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        if (!runningConfigStorage->SaveData(candidateConfigData.value())) {
            spdlog::error("Failed to save candidate config into '{}'", runningConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!runningConfigMngr->LoadConfig()) {
            spdlog::error("Failed to re-load running config after apply changes to candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        candidateConfigMngr = nullptr;
        return HTTP::StatusCode::OK;
    });

    // NOTE: It is also automatically called in case of expired session token
    cm->addOnDeleteConnectionHandler("config_candidate_delete", [&candidateConfigMngr](const Std::String& path, Std::String data_request, Std::String& return_data) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with DELETE method: {}", path, data_request);
        candidateConfigMngr = nullptr;
        return HTTP::StatusCode::OK;
    });

    return true;
}

int main(const int argc, const char* argv[]) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    consoleSink->set_pattern("%+");

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/bgp_config_api.log", true);
    fileSink->set_level(spdlog::level::trace);
    fileSink->set_pattern("%+");

    args::ArgumentParser argParser("Configuration Management System");
    args::HelpFlag help(argParser, "HELP", "Show this help menu", {'h', "help"});
    args::ValueFlag<Std::String> thisHostAddress(argParser, "ADDRESS", "The host binding address (hostname or IP address)", { 'a', "address" });
    args::ValueFlag<Std::String> birdcExecPath(argParser, "BIRDC", "Path to 'birdc' executable program for validation and load config purpose", { 'b', "birdc" });
    args::ValueFlag<Std::String> configFilename(argParser, "CONFIG", "The configuration file", { 'c', "config" });
    args::ValueFlag<Std::String> execPath(argParser, "EXEC", "Path to execution program for validation and load config purpose", { 'e', "exec" });
    args::ValueFlag<Std::String> schemaRootFilename(argParser, "SCHEMA", "The schema file", { 's', "schema" });
    args::ValueFlag<uint16_t> thisHostPort(argParser, "PORT", "The host binding port", { 'p', "port" });
    args::ValueFlag<Std::String> targetConfigFilename(argParser, "TARGET", "The target config file", { 't', "target" });
    try {
        argParser.ParseCLI(argc, argv);
    }
    catch (args::Help) {
        spdlog::info("{}", argParser.Help());
        ::exit(EXIT_SUCCESS);
    }
    catch (args::ParseError& e) {
        spdlog::error("{}", e.what());
        spdlog::info("{}", argParser.Help());
        ::exit(EXIT_FAILURE);
    }
    catch (args::ValidationError& e) {
        spdlog::error("{}", e.what());
        spdlog::info("{}", argParser.Help());
        ::exit(EXIT_FAILURE);
    }

    if (!configFilename || !schemaRootFilename || !thisHostAddress || !thisHostPort) {
        std::cout << argParser;
        ::exit(EXIT_SUCCESS);
    }

    auto loggerRegistry = std::make_shared<Log::LoggerRegistry>(spdlog::sinks_init_list{consoleSink, fileSink});
    loggerRegistry->RegisterModule(Module::Name::CONFIG_EXEC);
    loggerRegistry->Logger(Module::Name::CONFIG_EXEC)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_MNGMT);
    loggerRegistry->Logger(Module::Name::CONFIG_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::DATA_STORAGE);
    loggerRegistry->Logger(Module::Name::DATA_STORAGE)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::SCHEMA_MNGMT);
    loggerRegistry->Logger(Module::Name::SCHEMA_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_TRANSL);
    loggerRegistry->Logger(Module::Name::CONFIG_TRANSL)->set_level(spdlog::level::trace);
    auto moduleRegistry = std::make_shared<ModuleRegistry>();
    moduleRegistry->SetLoggerRegistry(loggerRegistry);

    auto jConfigFilename = args::get(configFilename);
    auto jSchemaFilename = args::get(schemaRootFilename);

    Std::SharedPtr<Storage::IDataStorage> jsonSchemaFileStorage = std::make_shared<Storage::JsonFileStorage>(jSchemaFilename, moduleRegistry);
    auto jDataSchema = jsonSchemaFileStorage->LoadData();
    if (!jDataSchema.has_value()) {
        spdlog::error("Failed to load JSON schema from file '{}'", jsonSchemaFileStorage->URI());
        ::exit(EXIT_FAILURE);
    }

    spdlog::info("Loaded JSON schema from file '{}'", jsonSchemaFileStorage->URI());
    Std::SharedPtr<Schema::ISchemaManagement> jsonSchemaMngr = std::make_shared<Schema::JsonSchemaManager>(jsonSchemaFileStorage, moduleRegistry);
    if (!jsonSchemaMngr->LoadSchema()) {
        spdlog::error("Failed to load JSON schema");
        ::exit(EXIT_FAILURE);
    }

    Std::SharedPtr<Storage::IDataStorage> configFileStorage = std::make_shared<Storage::FileStorage>(jConfigFilename, moduleRegistry);
    Std::SharedPtr<Config::IConfigManagement> jsonConfigMngr = std::make_shared<Config::JsonConfigManager>(configFileStorage, moduleRegistry);
    if (!jsonConfigMngr->LoadConfig()) {
        spdlog::error("Failed to load startup JSON config from file '{}'", configFileStorage->URI());
        ::exit(EXIT_FAILURE);
    }

    auto startupConfigDataToValid = jsonConfigMngr->SerializeConfig();
    if (!startupConfigDataToValid.has_value()) {
        spdlog::error("Failed to serialize startup JSON config");
        ::exit(EXIT_FAILURE);
    }

    if (!jsonSchemaMngr->ValidateData(startupConfigDataToValid.value())) {
        spdlog::error("Failed to validate startup JSON config against the schema");
        ::exit(EXIT_FAILURE);
    }

    Std::SharedPtr<Config::IConfigManagement> jsonBackupConfigMngr = std::make_shared<Config::JsonConfigManager>(configFileStorage, moduleRegistry);
    if (!jsonBackupConfigMngr->LoadConfig()) {
        spdlog::error("Failed to load JSON backup config");
        ::exit(EXIT_FAILURE);
    }

    Std::SharedPtr<Config::IConfigConverting> birdConfigConverter = std::make_shared<Config::BirdConfigConverter>(moduleRegistry);

    Std::SharedPtr<Storage::IDataStorage> birdConfigFileStorage;
    Std::SharedPtr<Config::Executing::IConfigExecuting> birdConfigExecutor;
    if (birdcExecPath && targetConfigFilename) {
        birdConfigFileStorage = std::make_shared<Storage::FileStorage>(args::get(targetConfigFilename), moduleRegistry);
        birdConfigExecutor = std::make_shared<Config::Executing::BirdConfigExecutor>(birdConfigFileStorage, args::get(birdcExecPath), moduleRegistry);
        
        auto birdConfigData = birdConfigConverter->Convert(startupConfigDataToValid.value());
        if (!birdConfigData.has_value()) {
            spdlog::error("Failed to convert native config into BIRD config");
            ::exit(EXIT_FAILURE);
        }

        if (!birdConfigFileStorage->SaveData(birdConfigData.value())) {
            spdlog::error("Failed to save BIRD config into file {}", birdConfigFileStorage->URI());
            ::exit(EXIT_FAILURE);
        }

        if (!birdConfigExecutor->Validate()) {
            spdlog::error("Failed to validate coverted config by external program");
            ::exit(EXIT_FAILURE);
        }
    }

    auto cm = std::make_shared<ConnectionManagement::Server>();
    if (!fSetupServerRequestHandlers(cm, jsonConfigMngr, jsonBackupConfigMngr, jsonSchemaMngr, configFileStorage, birdConfigFileStorage, birdConfigConverter, birdConfigExecutor, moduleRegistry)) {
        spdlog::error("Failed to setup request handlers");
        ::exit(EXIT_FAILURE);
    }

    if (!cm->Run(args::get(thisHostAddress), args::get(thisHostPort))) {
        spdlog::error("Failed to run connection management server");
        ::exit(EXIT_FAILURE);
    }

    spdlog::info("The '{}' daemon is going to shutdown", argv[0]);

    ::exit(EXIT_SUCCESS);
}
