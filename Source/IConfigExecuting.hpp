/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "IDataStorage.hpp"
#include "Lib/StdLib.hpp"


namespace Config {
namespace Executing {
using namespace StdLib;

class IConfigExecuting {
public:
    IConfigExecuting(const SharedPtr<Storage::IDataStorage> config)
      : mConfig(config) {}
    virtual ~IConfigExecuting() = default;
    virtual bool Validate() = 0;
    virtual bool Load() = 0;
    virtual bool Rollback([[maybe_unused]] const SharedPtr<Storage::IDataStorage> backupConfig) = 0;
    // virtual bool Confirm() = 0; // ?

protected:
    const SharedPtr<Storage::IDataStorage> mConfig;
};

} // namespace Executing
} // namespace Config
