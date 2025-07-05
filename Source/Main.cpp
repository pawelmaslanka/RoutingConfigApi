#include "BirdConfigConverter.hpp"
#include "FileStorage.hpp"
#include "JsonConfigManager.hpp"
#include "JsonFileStorage.hpp"
#include "JsonSchemaManager.hpp"
#include "Modules.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <iterator>

int main() {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    consoleSink->set_pattern("%+");

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/bird_config_api.log", true);
    fileSink->set_level(spdlog::level::trace);
    fileSink->set_pattern("%+");

    auto loggerRegistry = std::make_shared<Log::LoggerRegistry>(spdlog::sinks_init_list{consoleSink, fileSink});
    loggerRegistry->RegisterModule(Module::Name::SCHEMA_MNGMT);
    loggerRegistry->Logger(Module::Name::SCHEMA_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::DATA_STORAGE);
    loggerRegistry->Logger(Module::Name::DATA_STORAGE)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_MNGMT);
    loggerRegistry->Logger(Module::Name::CONFIG_MNGMT)->set_level(spdlog::level::trace);
    loggerRegistry->RegisterModule(Module::Name::CONFIG_TRANSL);
    loggerRegistry->Logger(Module::Name::CONFIG_TRANSL)->set_level(spdlog::level::trace);
    auto moduleRegistry = std::make_shared<ModuleRegistry>();
    moduleRegistry->SetLoggerRegistry(loggerRegistry);
    auto jsonSchemaFileStorage = std::make_shared<Storage::JsonFileStorage>("../Config/Schemas/bgp-main-config.json", moduleRegistry);
    auto jDataSchema = jsonSchemaFileStorage->LoadData();
    if (!jDataSchema.has_value()) {
        spdlog::error("Failed to load JSON schema from file '{}'", jsonSchemaFileStorage->URI());
        return EXIT_FAILURE;
    }

    spdlog::info("Loaded JSON schema from file '{}'", jsonSchemaFileStorage->URI());
    Schema::JsonSchemaManager jsonSchemaMngr(jsonSchemaFileStorage, moduleRegistry);
    if (!jsonSchemaMngr.LoadSchema()) {
        spdlog::error("Failed to load JSON schema");
        return EXIT_FAILURE;
    }

    auto configFileStorage = std::make_shared<Storage::FileStorage>("../Config/Test/bgp-config-test.json", moduleRegistry);
    Config::JsonConfigManager jsonConfigMngr(configFileStorage, moduleRegistry);
    if (!jsonConfigMngr.LoadConfig()) {
        spdlog::error("Failed to load JSON config");
        return EXIT_FAILURE;
    }

    auto configData = jsonConfigMngr.SerializeConfig();
    if (!configData.has_value()) {
        spdlog::error("Failed to serialize config");
        return EXIT_FAILURE;
    }

    if (!jsonSchemaMngr.ValidateData(configData.value())) {
        spdlog::error("Failed to validate config data against its schema");
        return EXIT_FAILURE;
    }

    auto newConfigFileStorage = std::make_shared<Storage::FileStorage>("../Config/Test/bgp-config-diff2-test.json", moduleRegistry);
    Config::JsonConfigManager jsonNewConfigMngr(newConfigFileStorage, moduleRegistry);
    if (!jsonNewConfigMngr.LoadConfig()) {
        spdlog::error("Failed to load new JSON config");
        return EXIT_FAILURE;
    }

    auto newConfigData = jsonNewConfigMngr.SerializeConfig();
    if (!configData.has_value()) {
        spdlog::error("Failed to serialize new config");
        return EXIT_FAILURE;
    }

    if (!jsonSchemaMngr.ValidateData(newConfigData.value())) {
        spdlog::error("Failed to validate new config data against its schema");
        return EXIT_FAILURE;
    }

    auto jsonDiffConfig = jsonConfigMngr.MakeDiff(newConfigData.value());
    if (!jsonDiffConfig.has_value()) {
        spdlog::error("Failed to make diff between two configs");
        return EXIT_FAILURE;
    }

    auto birdCfgData = Config::BirdConfigConverter(moduleRegistry).Convert(configData.value());
    if (!birdCfgData.has_value()) {
        spdlog::error("Failed to convert native config into BIRD config");
        return EXIT_FAILURE;
    }

    auto birdConfigFileStorage = std::make_shared<Storage::FileStorage>("./bird.conf", moduleRegistry);
    if (!birdConfigFileStorage->SaveData(birdCfgData.value())) {
        spdlog::error("Failed to save BIRD config into file {}", birdConfigFileStorage->URI());
        return EXIT_FAILURE;
    }

    spdlog::info("Successfully converted JSON config into target config");
    
    return EXIT_SUCCESS;
}
