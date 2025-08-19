/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "StdLib.hpp"

#include <regex>

namespace Utils {
using namespace StdLib;

static inline String fLeftTrim(const String &s) {
    return std::regex_replace(s, std::regex("^\\s+"), String(""));
}
 
static inline String fRightTrim(const String &s) {
    return std::regex_replace(s, std::regex("\\s+$"), String(""));
}
 
static inline String fTrim(const String &s) {
    return fLeftTrim(fRightTrim(s));
}

static void fFindAndReplaceAllInPlace(String& data, String toSearch, String replaceStr) {
    // Get the first occurrence
    size_t pos = data.find(toSearch);
    // Repeat till end is reached
    while (pos != String::npos) {
        // Replace this occurrence of Sub String
        data.replace(pos, toSearch.size(), replaceStr);
        // Get the next occurrence from the current position
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

static String fFindAndReplaceAll(const String& data, String toSearch, String replaceStr) {
    String copySource = data;
    // Get the first occurrence
    size_t pos = copySource.find(toSearch);
    // Repeat till end is reached
    while (pos != String::npos) {
        // Replace this occurrence of Sub String
        copySource.replace(pos, toSearch.size(), replaceStr);
        // Get the next occurrence from the current position
        pos = copySource.find(toSearch, pos + replaceStr.size());
    }

    return copySource;
}

static Vector<String> fSplitStringByWhitespace(const String& input) {
    std::regex pattern("\\s+");
    std::sregex_token_iterator tokensIt(input.begin(), input.end(), pattern, -1);
    std::sregex_token_iterator end;

    Vector<String> tokens;
    while (tokensIt != end) {
        tokens.push_back(*tokensIt++);
    }

    return tokens;
}

} // namespace Utils
