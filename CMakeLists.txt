cmake_minimum_required(VERSION 3.30)
project(BgpConfigApi)

find_package(nlohmann_json_schema_validator REQUIRED)

find_path(LIBYANG_INCLUDE_DIR
        NAMES
        libyang/libyang.h
        PATHS
        /Users/pmaslank/Development/libyang/lib/include
        REQUIRED
)

find_library(LIBYANG_LIBRARY
        NAMES
        yang
        libyang
        PATHS
        /Users/pmaslank/Development/libyang/lib
        /Users/pmaslank/Development/libyang/lib/lib
        REQUIRED
)

find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 20)

#include_directories(/Users/pmaslank/Development/libyang/lib/include/libyang)
include_directories(Lib ${LIBYANG_INCLUDE_DIR})
include_directories(Source/Lib)
include_directories(Source/Lib/ThirdParty)
link_directories(/Users/pmaslank/Development/libyang/lib/lib)

#add_compile_options(-g -fsanitize=address -fsanitize-recover=address)
#add_link_options(-g -fsanitize=address -fsanitize-recover=address)

add_executable(${PROJECT_NAME} Source/Main.cpp
        Source/Lib/Composite/Node.hpp
        Source/Lib/StdLib.hpp
        Source/Lib/MultiinheritShared.hpp
        Source/Lib/Composite/Visitor.hpp
        Source/Lib/Composite/Composite.hpp
        Source/Lib/Composite/Test.hpp
        Source/BirdConfigConverter.hpp
        Source/ConnectionManagement.cpp
        Source/Common.hpp
        Source/FileStorage.hpp
        Source/JsonConfigManager.hpp
        Source/JsonCommon.hpp
        Source/JsonSchemaManager.hpp
        Source/ISchemaManagement.hpp
        Source/Lib/ModuleRegistry.hpp
        Source/Lib/Logging.hpp
        Source/Modules.hpp
        Source/SessionManagement.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json nlohmann_json_schema_validator::validator)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC yang)
