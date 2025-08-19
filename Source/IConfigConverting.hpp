/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "Common.hpp"
#include "Lib/StdLib.hpp"

namespace Config {
using namespace StdLib;
class IConfigConverting {
public:
    virtual ~IConfigConverting() = default;
    virtual Optional<ByteStream> Convert(const ByteStream& config) = 0;
}; // class IConfigConverting
} // namespace Config
