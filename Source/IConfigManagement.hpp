/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once
#include "Lib/StdLib.hpp"

namespace Config {
using namespace StdLib;
class IConfigManagement {
public:
    virtual ~IConfigManagement() = default;
    virtual bool LoadConfig() = 0;
    virtual Optional<ByteStream> SerializeConfig() = 0;
    virtual Optional<ByteStream> MakeDiff(const ByteStream& otherConfig) = 0;
    virtual bool ApplyPatch(const ByteStream& patch) = 0;
}; // class IConfigManagement
} // namespace Config
