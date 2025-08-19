/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
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
#include <spdlog/sinks/ringbuffer_sink.h>

#include <cstdlib>
#include <iterator>

namespace Std = StdLib;

bool fSetupServerRequestHandlers(Std::SharedPtr<ConnectionManagement::Server>& cm, Std::UniquePtr<Config::IConfigManagement>& runningConfigMngr, Std::SharedPtr<Schema::ISchemaManagement> schemaMngr, Std::SharedPtr<Storage::IDataStorage>& runningConfigStorage, Std::SharedPtr<Storage::IDataStorage>& targetConfigStorage, Std::SharedPtr<Config::IConfigConverting> configConverter, Std::SharedPtr<Config::Executing::IConfigExecuting>& targetConfigExecutor, const Std::SharedPtr<ModuleRegistry>& moduleRegistry) {
    auto loggerRegistry = moduleRegistry->LoggerRegistry();
    loggerRegistry->RegisterModule(Module::Name::SRV_USR_REQ_HANDLE);

    static const size_t DEFAULT_RINGBUFFER_CAP = 64;
    // The userReqLogMsgBuf is used to log messages related to external user interaction/handle request
    auto srvUsrReqLogSink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(DEFAULT_RINGBUFFER_CAP);
    srvUsrReqLogSink->set_level(spdlog::level::err);
    srvUsrReqLogSink->set_pattern("%v"); // Pattern: message only
    loggerRegistry->AddLogSink(srvUsrReqLogSink);

    auto srvUsrReqLog = loggerRegistry->Logger(Module::Name::SRV_USR_REQ_HANDLE);
    srvUsrReqLog->set_level(spdlog::level::err);

    // Right now there can be active only single instance of candidate config
    static Std::UniquePtr<Config::IConfigManagement> gCandidateConfigMngr;

    cm->addOnPatchConnectionHandler("config_running_update", [&runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, schemaMngr, runningConfigStorage, targetConfigStorage, configConverter, targetConfigExecutor, moduleRegistry, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_UPDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with PATCH method: {}", path, dataRequest);

        if (candidateConfigMngr) {
            srvUsrReqLog->error("There is other active session with pending candidate config changes");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        // NOTE: Register new instance of class derived from Config::IConfigManagement,
        //       or consider refactoring this function and use template parameter do determine instance of class derived from Config::IConfigManagement
        if (!candidateConfigMngr) {
            if (auto jsonBasedConfigMngr = dynamic_cast<Config::JsonConfigManager*>(runningConfigMngr.get())) {
                candidateConfigMngr.reset(new Config::JsonConfigManager(*jsonBasedConfigMngr));
            }
            else {
                srvUsrReqLog->error("Unsupported type of derived class from Config::IConfigManagement");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        ByteStream patchData(dataRequest.begin(), dataRequest.end());
        if (!candidateConfigMngr->ApplyPatch(patchData)) {
            srvUsrReqLog->error("Failed to apply patch to running config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            srvUsrReqLog->error("Failed to serialize candidate config");
            candidateConfigMngr.reset(nullptr);
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!schemaMngr->ValidateData(configData.value())) {
            srvUsrReqLog->error("Failed to validate candidate config data against its schema");
            candidateConfigMngr.reset(nullptr);
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto targetConfigData = configConverter->Convert(configData.value());
        if (!targetConfigData.has_value()) {
            srvUsrReqLog->error("Failed to convert native config into target config");
            candidateConfigMngr.reset(nullptr);
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!targetConfigStorage->SaveData(targetConfigData.value())) {
            srvUsrReqLog->error("Failed to save target config into file {}", targetConfigStorage->URI());
            candidateConfigMngr.reset(nullptr);
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            if (!targetConfigExecutor->Validate()) {
                srvUsrReqLog->error("Failed to validate candidate config by external program");
                if (!targetConfigStorage->SaveData(configConverter->Convert(runningConfigMngr->SerializeConfig().value()).value())) {
                    srvUsrReqLog->error("Failed to restore running config into '{}'", targetConfigStorage->URI());
                }

                candidateConfigMngr.reset(nullptr);
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_running_get", [&runningConfigMngr, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request running on {} with GET method: {}", path, dataRequest);
        auto configData = runningConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            srvUsrReqLog->error("Failed to serialize config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream configDataStr;
        for (const auto c : configData.value()) {
            configDataStr << static_cast<char>(c);
        }
    
        returnData = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_running_diff", [&runningConfigMngr, &schemaMngr, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::RUNNING_DIFF);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request on {} with POST diff method: {}", path, dataRequest);
        ByteStream otherConfigData(dataRequest.begin(), dataRequest.end());
        if (!schemaMngr->ValidateData(otherConfigData)) {
            srvUsrReqLog->error("Failed to validate other config data against its schema");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto patchData = runningConfigMngr->MakeDiff(otherConfigData);
        if (!patchData.has_value()) {
            srvUsrReqLog->error("Failed to make a diff between running config and other config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        Std::OStrStream patchDataStr;
        std::transform(patchData.value().begin(), patchData.value().end(), std::ostream_iterator<char>(patchDataStr, ""), [](const Byte b) { return static_cast<char>(b); });
        returnData = patchDataStr.str();
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("config_candidate_get", [&candidateConfigMngr = gCandidateConfigMngr, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with GET method: {}", path, dataRequest);
        if (!candidateConfigMngr) {
            srvUsrReqLog->error("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto configData = candidateConfigMngr->SerializeConfig();
        if (!configData.has_value()) {
            srvUsrReqLog->error("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        Std::OStrStream configDataStr;
        std::transform(configData.value().begin(), configData.value().end(), std::ostream_iterator<char>(configDataStr, ""), [](const Byte b) { return static_cast<char>(b); });    
        returnData = configDataStr.str();
        return HTTP::StatusCode::OK;
    });

    static auto fApplyConfig = [&runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, &runningConfigStorage, configConverter, targetConfigStorage, targetConfigExecutor, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) -> HTTP::StatusCode {
        if (!candidateConfigMngr) {
            spdlog::trace("Not found active candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto candidateConfigData = candidateConfigMngr->SerializeConfig();
        if (!candidateConfigData.has_value()) {
            srvUsrReqLog->error("Failed to serialize candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        auto targetConfigData = configConverter->Convert(candidateConfigData.value());
        if (!targetConfigData.has_value()) {
            srvUsrReqLog->error("Failed to convert candidate config into target config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!targetConfigStorage->SaveData(targetConfigData.value())) {
            srvUsrReqLog->error("Failed to save target config into file {}", targetConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            if (!targetConfigExecutor->Load()) {
                srvUsrReqLog->error("Failed to load candidate config by external program");
                if (!targetConfigStorage->SaveData(configConverter->Convert(runningConfigMngr->SerializeConfig().value()).value())) {
                    srvUsrReqLog->error("Failed to restore running config into '{}'", targetConfigStorage->URI());
                }

                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        return HTTP::StatusCode::OK;
    };

    static Std::Optional<Std::String> waitCommitConfirmSessionId = {};

    cm->addOnPostConnectionHandler("config_candidate_commit", [&applyConfig = fApplyConfig, &runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, runningConfigStorage, srvUsrReqLog](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with POST method: {}", path, dataRequest);
        
        auto result = applyConfig(sessionId, path, dataRequest, returnData);
        if (result != HTTP::StatusCode::OK) {
            return result;
        }

        if (!runningConfigStorage->SaveData(candidateConfigMngr->SerializeConfig().value())) {
            srvUsrReqLog->error("Failed to save candidate config into running '{}'", runningConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!runningConfigMngr->LoadConfig()) {
            srvUsrReqLog->error("Failed to re-load running config after apply changes from candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        candidateConfigMngr.reset(nullptr);
        return HTTP::StatusCode::OK;
    });

    cm->addOnPostConnectionHandler("config_candidate_commit_timeout", [&applyConfig = fApplyConfig, &confirmBySessionId = waitCommitConfirmSessionId](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_TIMEOUT) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_TIMEOUT);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with POST method: {}", path, dataRequest);
        auto result = applyConfig(sessionId, path, dataRequest, returnData);
        if (result != HTTP::StatusCode::OK) {
            return result;
        }

        confirmBySessionId = sessionId;
        return HTTP::StatusCode::OK;
    });

    cm->addOnPostConnectionHandler("config_candidate_commit_confirm", [&applyConfig = fApplyConfig, &runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, runningConfigStorage, srvUsrReqLog, &confirmBySessionId = waitCommitConfirmSessionId](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_CONFIRM) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_CONFIRM);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        spdlog::debug("Get request candidate on {} with POST method: {}", path, dataRequest);
        if (!confirmBySessionId.has_value()) {
            spdlog::trace("There is not pending commit-confirm process");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (confirmBySessionId.value() != sessionId) {
            spdlog::trace("The session id '{}' is not owner of pending commit-confirm");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!runningConfigStorage->SaveData(candidateConfigMngr->SerializeConfig().value())) {
            srvUsrReqLog->error("Failed to save candidate config into running '{}'", runningConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!runningConfigMngr->LoadConfig()) {
            srvUsrReqLog->error("Failed to re-load running config after apply changes from candidate config");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        candidateConfigMngr.reset(nullptr);
        return HTTP::StatusCode::OK;
    });

    cm->addOnPostConnectionHandler("config_candidate_commit_cancel", [&runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, &configConverter, targetConfigStorage, targetConfigExecutor, srvUsrReqLog, &confirmBySessionId = waitCommitConfirmSessionId](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_CANCEL) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE_COMMIT_CANCEL);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        if (!confirmBySessionId.has_value()) {
            srvUsrReqLog->trace("There is not pending commit-confirm process");
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (confirmBySessionId.value() != sessionId) {
            srvUsrReqLog->trace("The session id '{}' is not owner of pending commit-confirm process", sessionId);
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (!candidateConfigMngr) {
            spdlog::debug("There is not active candidate config");
            return HTTP::StatusCode::OK;
        }

        if (!targetConfigStorage->SaveData(configConverter->Convert(runningConfigMngr->SerializeConfig().value()).value())) {
            srvUsrReqLog->error("Failed to restore running config into '{}'", targetConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            if (!targetConfigExecutor->Rollback(targetConfigStorage)) {
                srvUsrReqLog->error("Failed to load running config by external program");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }

        // Don't reset canidate config instance, just continue actions on current changes
        confirmBySessionId = std::nullopt;
        return HTTP::StatusCode::OK;
    });

    // NOTE: It is also automatically called in case of expired session token
    cm->addOnDeleteConnectionHandler("config_candidate_delete", [&runningConfigMngr, &candidateConfigMngr = gCandidateConfigMngr, &configConverter, targetConfigStorage, targetConfigExecutor, srvUsrReqLog, &confirmBySessionId = waitCommitConfirmSessionId](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Config::CANDIDATE) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Config::CANDIDATE);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        if (confirmBySessionId.has_value()) {
            // There is other session which waits for commit-confirm request. The request probably comes from other expired session (token)
            if (confirmBySessionId.value() != sessionId) {
                return HTTP::StatusCode::OK;
            }
        }

        if (!candidateConfigMngr) {
            spdlog::trace("There is not active candidate config");
            return HTTP::StatusCode::OK;
        }

        DEFER({
            candidateConfigMngr.reset(nullptr);
            confirmBySessionId = std::nullopt;
        });

        if (!targetConfigStorage->SaveData(configConverter->Convert(runningConfigMngr->SerializeConfig().value()).value())) {
            srvUsrReqLog->error("Failed to restore running config into '{}'", targetConfigStorage->URI());
            return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
        }

        if (targetConfigExecutor) {
            // FIXME: Use targetConfigExecutor->Rollback()?
            if (!targetConfigExecutor->Load()) {
                srvUsrReqLog->error("Failed to load running config by external program");
                return HTTP::StatusCode::INTERNAL_SERVER_ERROR;
            }
        }
        
        return HTTP::StatusCode::OK;
    });

    cm->addOnGetConnectionHandler("logs_latest_n_get", [srvUsrReqLog, srvUsrReqLogSink](const Std::String& sessionId, const Std::String& path, Std::String dataRequest, Std::String& returnData) {
        if (path != ConnectionManagement::URIRequestPath::Logs::LATEST_N) {
            spdlog::debug("Unexpected URI requested '{}' - expected '{}'", path, ConnectionManagement::URIRequestPath::Logs::LATEST_N);
            return HTTP::StatusCode::INTERNAL_SUCCESS;
        }

        Std::OStrStream returnDataBuf;
        for (const auto& msg : srvUsrReqLogSink->last_raw(std::stoi(dataRequest))) {
            returnDataBuf << fmt::format("{}\n", msg.payload);
        }

        returnData = returnDataBuf.str();
        return HTTP::StatusCode::OK;
    });

    return true;
}

int main(const int argc, const char* argv[]) {
    args::ArgumentParser argParser("Configuration Management System");
    args::HelpFlag help(argParser, "HELP", "Show this help menu", {'h', "help"});
    args::ValueFlag<Std::String> thisHostAddress(argParser, "ADDRESS", "The host binding address (hostname or IP address)", { 'a', "address" });
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

    auto consoleLogSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleLogSink->set_level(spdlog::level::err);
    consoleLogSink->set_pattern("%+");

    auto fileLogSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/bgp_config_api.log", true);
    fileLogSink->set_level(spdlog::level::err);
    fileLogSink->set_pattern("%+");

    auto loggerRegistry = std::make_shared<Log::LoggerRegistry>(spdlog::sinks_init_list{consoleLogSink, fileLogSink});
    loggerRegistry->RegisterModule(Module::Name::CONFIG_EXEC);
    loggerRegistry->Logger(Module::Name::CONFIG_EXEC)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_MNGMT);
    loggerRegistry->Logger(Module::Name::CONFIG_MNGMT)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_TRANSL);
    loggerRegistry->Logger(Module::Name::CONFIG_TRANSL)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::CONN_MNGMT);
    loggerRegistry->Logger(Module::Name::CONN_MNGMT)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::DATA_STORAGE);
    loggerRegistry->Logger(Module::Name::DATA_STORAGE)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::SCHEMA_MNGMT);
    loggerRegistry->Logger(Module::Name::SCHEMA_MNGMT)->set_level(spdlog::level::err);
    loggerRegistry->RegisterModule(Module::Name::SESSION_MNGMT);
    loggerRegistry->Logger(Module::Name::SESSION_MNGMT)->set_level(spdlog::level::err);

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
    Std::UniquePtr<Config::IConfigManagement> jsonConfigMngr = std::make_unique<Config::JsonConfigManager>(configFileStorage, moduleRegistry);
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

    Std::SharedPtr<Config::IConfigConverting> birdConfigConverter = std::make_shared<Config::BirdConfigConverter>(moduleRegistry);

    Std::SharedPtr<Storage::IDataStorage> birdConfigFileStorage;
    Std::SharedPtr<Config::Executing::IConfigExecuting> birdConfigExecutor;
    if (execPath && targetConfigFilename) {
        birdConfigFileStorage = std::make_shared<Storage::FileStorage>(args::get(targetConfigFilename), moduleRegistry);
        birdConfigExecutor = std::make_shared<Config::Executing::BirdConfigExecutor>(birdConfigFileStorage, args::get(execPath), moduleRegistry);
        
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

    auto cm = std::make_shared<ConnectionManagement::Server>(moduleRegistry);
    if (!fSetupServerRequestHandlers(cm, jsonConfigMngr, jsonSchemaMngr, configFileStorage, birdConfigFileStorage, birdConfigConverter, birdConfigExecutor, moduleRegistry)) {
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
