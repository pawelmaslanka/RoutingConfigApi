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

#include <fmt/core.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/ostream_sink.h>

#include <cstdlib>
#include <iterator>

namespace Std = StdLib;

/// NOTE: During passing argument candidateConfigMngr to fSetupServerRequestHandlers() it should contains the same content what runningConfigMngr
bool fSetupServerRequestHandlers(Std::SharedPtr<ConnectionManagement::Server>& cm, Std::SharedPtr<Config::IConfigManagement>& runningConfigMngr, Std::SharedPtr<Config::IConfigManagement>& candidateConfigMngr, Std::SharedPtr<Schema::ISchemaManagement> schemaMngr, Std::SharedPtr<Storage::IDataStorage>& runningConfigStorage, Std::SharedPtr<Storage::IDataStorage>& targetConfigStorage, Std::SharedPtr<Config::IConfigConverting> configConverter, Std::SharedPtr<Config::Executing::IConfigExecuting>& targetConfigExecutor, const Std::SharedPtr<ModuleRegistry>& moduleRegistry, Std::SharedPtr<Std::OStrStream>& userReqLogMsgBuf) {
    cm->addOnPostConnectionHandler("config_running_update", [&runningConfigMngr, &candidateConfigMngr, schemaMngr, runningConfigStorage, targetConfigStorage, configConverter, targetConfigExecutor, moduleRegistry, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request on {} with POST method: {}", path, dataRequest);
        // NOTE: Register new instance of class derived from Config::IConfigManagement,
        //       or consider refactoring this function and use template parameter do determine instance of class derived from Config::IConfigManagement
        if (!candidateConfigMngr) {
            if (auto jsonBasedConfigMngr = dynamic_pointer_cast<Config::JsonConfigManager>(runningConfigMngr)){
                candidateConfigMngr = std::make_shared<Config::JsonConfigManager>(*jsonBasedConfigMngr);
            }
            else {
                *userReqLogMsgBuf << fmt::format("Unsupported type of derived class from Config::IConfigManagement");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        ByteStream patchData(dataRequest.begin(), dataRequest.end());
        if (!candidateConfigMngr->ApplyPatch(patchData)) {
            *userReqLogMsgBuf << fmt::format("Failed to apply patch to running config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to serialize candidate config");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!schemaMngr->ValidateData(configData.value())) {
            *userReqLogMsgBuf << fmt::format("Failed to validate candidate config data against its schema");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto targetConfigData = configConverter->Convert(configData.value());
        if (!targetConfigData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to convert native config into target config");
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!targetConfigStorage->SaveData(targetConfigData.value())) {
            *userReqLogMsgBuf << fmt::format("Failed to save taget config into file {}", targetConfigStorage->URI());
            candidateConfigMngr = nullptr;
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (configConverter) {
            if (!targetConfigExecutor->Validate()) {
                *userReqLogMsgBuf << fmt::format("Failed to validate candidate config by external program");
                candidateConfigMngr = nullptr;
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_running_get", [&runningConfigMngr, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request running on {} with GET method: {}", path, dataRequest);
        auto configData = runningConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to serialize config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream configDataStr;
        for (const auto c : configData.value()) {
            configDataStr << static_cast<char>(c);
        }
    
        returnData = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_running_diff", [&runningConfigMngr, &schemaMngr, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request on {} with POST diff method: {}", path, dataRequest);
        ByteStream otherConfigData(dataRequest.begin(), dataRequest.end());
        if (!schemaMngr->ValidateData(otherConfigData)) {
            *userReqLogMsgBuf << fmt::format("Failed to validate other config data against its schema");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto patchData = runningConfigMngr->MakeDiff(otherConfigData);
        if (!patchData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to make a diff between running config and other config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream patchDataStr;
        std::transform(patchData.value().begin(), patchData.value().end(), std::ostream_iterator<char>(patchDataStr, ""), [](const Byte b) { return static_cast<char>(b); });
        returnData = patchDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_candidate_get", [&candidateConfigMngr, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request candidate on {} with GET method: {}", path, dataRequest);
        if (!candidateConfigMngr) {
            *userReqLogMsgBuf << fmt::format("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        Std::OStrStream configDataStr;
        std::transform(configData.value().begin(), configData.value().end(), std::ostream_iterator<char>(configDataStr, ""), [](const Byte b) { return static_cast<char>(b); });    
        returnData = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnPutConnectionHandler("config_candidate_apply", [&runningConfigMngr, &candidateConfigMngr, &runningConfigStorage, targetConfigExecutor, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request candidate on {} with PUT method: {}", path, dataRequest);
        if (!candidateConfigMngr) {
            spdlog::trace("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto candidateConfigData = candidateConfigMngr->SerializeConfig();
        if (!candidateConfigData.has_value()) {
            *userReqLogMsgBuf << fmt::format("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            if (!targetConfigExecutor->Load()) {
                *userReqLogMsgBuf << fmt::format("Failed to load candidate config by external program");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        if (!runningConfigStorage->SaveData(candidateConfigData.value())) {
            *userReqLogMsgBuf << fmt::format("Failed to save candidate config into '{}'", runningConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!runningConfigMngr->LoadConfig()) {
            *userReqLogMsgBuf << fmt::format("Failed to re-load running config after apply changes to candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        candidateConfigMngr = nullptr;
        return HTTP::StatusCode::OK;
    });

    // NOTE: It is also automatically called in case of expired session token
    cm->addOnDeleteConnectionHandler("config_candidate_delete", [&candidateConfigMngr, userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        userReqLogMsgBuf->str({});
        spdlog::debug("Get request candidate on {} with DELETE method: {}", path, dataRequest);
        candidateConfigMngr = nullptr;
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("last_request_log_msg_get", [userReqLogMsgBuf](const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Log::LAST_REQUEST) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Log::LAST_REQUEST);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        returnData = userReqLogMsgBuf->str();
        return HTTP::StatusCode::OK;
    });

    return true;
}

int main(const int argc, const char* argv[]) {
    auto consoleLogSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleLogSink->set_level(spdlog::level::trace);
    consoleLogSink->set_pattern("%+");

    auto fileLogSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/bgp_config_api.log", true);
    fileLogSink->set_level(spdlog::level::trace);
    fileLogSink->set_pattern("%+");

    // The userReqLogMsgBuf is used to log messages related to external user interaction/handle request
    auto userReqLogMsgBuf = std::make_shared<Std::OStrStream>();
    auto onlyErrorLogSink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*userReqLogMsgBuf);
    onlyErrorLogSink->set_level(spdlog::level::err);
    onlyErrorLogSink->set_pattern("%+");

    args::ArgumentParser argParser("Configuration Management System");
    args::HelpFlag help(argParser, "HELP", "Show this help menu", {'h', "help"});
    args::ValueFlag<Std::String> thisHostAddress(argParser, "ADDRESS", "The host binding address (hostname or IP address)", { 'a', "address" });
    args::ValueFlag<Std::String> birdcExecPath(argParser, "BIRDC", "Path to 'birdc' executable program for validation and load config purpose", { 'b', "birdc" });
    args::ValueFlag<Std::String> configFilename(argParser, "CONFIG", "The configuration file", { 'c', "config" });
    args::ValueFlag<Std::String> execPath(argParser, "EXEC", "Path to the executable program to verify and load the config", { 'e', "exec" });
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
        *userReqLogMsgBuf << fmt::format("{}", e.what());
        spdlog::info("{}", argParser.Help());
        ::exit(EXIT_FAILURE);
    }
    catch (args::ValidationError& e) {
        *userReqLogMsgBuf << fmt::format("{}", e.what());
        spdlog::info("{}", argParser.Help());
        ::exit(EXIT_FAILURE);
    }

    if (!configFilename || !schemaRootFilename || !thisHostAddress || !thisHostPort) {
        std::cout << argParser;
        ::exit(EXIT_SUCCESS);
    }

    auto loggerRegistry = std::make_shared<Log::LoggerRegistry>(spdlog::sinks_init_list{consoleLogSink, fileLogSink, onlyErrorLogSink});
    loggerRegistry->RegisterModule(Module::Name::CONFIG_EXEC);
    loggerRegistry->Logger(Module::Name::CONFIG_EXEC)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_MNGMT);
    loggerRegistry->Logger(Module::Name::CONFIG_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_TRANSL);
    loggerRegistry->Logger(Module::Name::CONFIG_TRANSL)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONN_MNGMT);
    loggerRegistry->Logger(Module::Name::CONN_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::DATA_STORAGE);
    loggerRegistry->Logger(Module::Name::DATA_STORAGE)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::SCHEMA_MNGMT);
    loggerRegistry->Logger(Module::Name::SCHEMA_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::SESSION_MNGMT);
    loggerRegistry->Logger(Module::Name::SESSION_MNGMT)->set_level(spdlog::level::trace);
    auto moduleRegistry = std::make_shared<ModuleRegistry>();
    moduleRegistry->SetLoggerRegistry(loggerRegistry);

    auto jConfigFilename = args::get(configFilename);
    auto jSchemaFilename = args::get(schemaRootFilename);

    Std::SharedPtr<Storage::IDataStorage> jsonSchemaFileStorage = std::make_shared<Storage::JsonFileStorage>(jSchemaFilename, moduleRegistry);
    auto jDataSchema = jsonSchemaFileStorage->LoadData();
    if (!jDataSchema.has_value()) {
        *userReqLogMsgBuf << fmt::format("Failed to load JSON schema from file '{}'", jsonSchemaFileStorage->URI());
        ::exit(EXIT_FAILURE);
    }

    spdlog::info("Loaded JSON schema from file '{}'", jsonSchemaFileStorage->URI());
    Std::SharedPtr<Schema::ISchemaManagement> jsonSchemaMngr = std::make_shared<Schema::JsonSchemaManager>(jsonSchemaFileStorage, moduleRegistry);
    if (!jsonSchemaMngr->LoadSchema()) {
        *userReqLogMsgBuf << fmt::format("Failed to load JSON schema");
        ::exit(EXIT_FAILURE);
    }

    Std::SharedPtr<Storage::IDataStorage> configFileStorage = std::make_shared<Storage::FileStorage>(jConfigFilename, moduleRegistry);
    Std::SharedPtr<Config::IConfigManagement> jsonConfigMngr = std::make_shared<Config::JsonConfigManager>(configFileStorage, moduleRegistry);
    if (!jsonConfigMngr->LoadConfig()) {
        *userReqLogMsgBuf << fmt::format("Failed to load startup JSON config from file '{}'", configFileStorage->URI());
        ::exit(EXIT_FAILURE);
    }

    auto startupConfigDataToValid = jsonConfigMngr->SerializeConfig();
    if (!startupConfigDataToValid.has_value()) {
        *userReqLogMsgBuf << fmt::format("Failed to serialize startup JSON config");
        ::exit(EXIT_FAILURE);
    }

    if (!jsonSchemaMngr->ValidateData(startupConfigDataToValid.value())) {
        *userReqLogMsgBuf << fmt::format("Failed to validate startup JSON config against the schema");
        ::exit(EXIT_FAILURE);
    }

    Std::SharedPtr<Config::IConfigManagement> jsonBackupConfigMngr = std::make_shared<Config::JsonConfigManager>(configFileStorage, moduleRegistry);
    if (!jsonBackupConfigMngr->LoadConfig()) {
        *userReqLogMsgBuf << fmt::format("Failed to load JSON backup config");
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
            *userReqLogMsgBuf << fmt::format("Failed to convert native config into BIRD config");
            ::exit(EXIT_FAILURE);
        }

        if (!birdConfigFileStorage->SaveData(birdConfigData.value())) {
            *userReqLogMsgBuf << fmt::format("Failed to save BIRD config into file {}", birdConfigFileStorage->URI());
            ::exit(EXIT_FAILURE);
        }

        if (!birdConfigExecutor->Validate()) {
            *userReqLogMsgBuf << fmt::format("Failed to validate coverted config by external program");
            ::exit(EXIT_FAILURE);
        }
    }

    auto cm = std::make_shared<ConnectionManagement::Server>(moduleRegistry);
    if (!fSetupServerRequestHandlers(cm, jsonConfigMngr, jsonBackupConfigMngr, jsonSchemaMngr, configFileStorage, birdConfigFileStorage, birdConfigConverter, birdConfigExecutor, moduleRegistry, userReqLogMsgBuf)) {
        *userReqLogMsgBuf << fmt::format("Failed to setup request handlers");
        ::exit(EXIT_FAILURE);
    }

    if (!cm->Run(args::get(thisHostAddress), args::get(thisHostPort))) {
        *userReqLogMsgBuf << fmt::format("Failed to run connection management server");
        ::exit(EXIT_FAILURE);
    }

    spdlog::info("The '{}' daemon is going to shutdown", argv[0]);

    ::exit(EXIT_SUCCESS);
}
