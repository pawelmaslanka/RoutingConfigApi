#pragma once
#include "Lib/StdLib.hpp"

namespace Schema {
using namespace StdLib;
class ISchemaManagement {
public:
    virtual ~ISchemaManagement() = default;
    virtual bool LoadSchema() = 0;
    virtual bool ValidateData(const ByteStream& data) = 0;
}; // class ISchemaManagement
} // namespace Schema
