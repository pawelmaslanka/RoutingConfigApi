/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "Common.hpp"

namespace Storage {
using namespace StdLib;
class IDataStorage {
public:
    IDataStorage(const String& uri) : mURI(uri) {}
    virtual ~IDataStorage() = default;
    virtual Optional<ByteStream> LoadData() = 0;
    virtual bool SaveData(const ByteStream& data) = 0;
    String URI() const { return mURI; }

protected:
    const String mURI;
}; // class IDataStorage
} // namespace Storage
