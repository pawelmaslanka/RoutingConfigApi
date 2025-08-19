cmake_minimum_required(VERSION 3.30)
project(RoutingConfigApi)

find_package(nlohmann_json_schema_validator REQUIRED)

find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 20)

set(LIB_DIR Source/Lib)

include_directories(${LIB_DIR})
include_directories(${LIB_DIR}/ThirdParty)

#add_compile_options(-g -fsanitize=address -fsanitize-recover=address)
#add_link_options(-g -fsanitize=address -fsanitize-recover=address)

add_executable(${PROJECT_NAME} Source/Main.cpp
        ${LIB_DIR}/Composite/Node.hpp
        ${LIB_DIR}/StdLib.hpp
        ${LIB_DIR}/MultiinheritShared.hpp
        ${LIB_DIR}/Composite/Visitor.hpp
        ${LIB_DIR}/Composite/Composite.hpp
        ${LIB_DIR}/Composite/Test.hpp
        Source/BirdConfigConverter.hpp
        Source/ConnectionManagement.cpp
        Source/Common.hpp
        Source/FileStorage.hpp
        Source/JsonConfigManager.hpp
        Source/JsonCommon.hpp
        Source/JsonSchemaManager.hpp
        Source/ISchemaManagement.hpp
        ${LIB_DIR}/ModuleRegistry.hpp
        ${LIB_DIR}/Logging.hpp
        Source/Modules.hpp
        Source/SessionManagement.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json nlohmann_json_schema_validator::validator)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
