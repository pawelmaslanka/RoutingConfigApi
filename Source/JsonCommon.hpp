/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "Lib/StdLib.hpp"

#include <nlohmann/json.hpp>

namespace Json {
using namespace StdLib;
using JSON = nlohmann::ordered_json;

constexpr int DEFAULT_OUTPUT_INDENT = 4;

namespace Diff {
namespace Field {
    static const String OPERATION = "op";
    static const String PARAMETERS = "params";
    static const String PATH = "path";
    static const String VALUE = "value";
} // namespace Field

namespace Operation {
    static const String ADD = "add";
    static const String REMOVE = "remove";
    static const String REPLACE = "replace";
} // namespace Operation
} // namespace Diff
} // namespace Json
