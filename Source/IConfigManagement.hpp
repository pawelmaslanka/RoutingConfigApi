#pragma once
#include "Lib/StdLib.hpp"

namespace Config {
using namespace StdLib;
class IConfigManagement {
public:
    virtual ~IConfigManagement() = default;
    virtual bool LoadConfig() = 0;
    virtual Optional<ByteStream> SerializeConfig() = 0;
}; // class IConfigManagement
} // namespace Config
