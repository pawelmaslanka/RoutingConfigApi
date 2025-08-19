/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "IDataStorage.hpp"
#include "Common.hpp"

#include <filesystem>
#include <iterator>

namespace Storage {
class FileStorage : public IDataStorage {
public:
    FileStorage(const String& fileName, const SharedPtr<ModuleRegistry>& moduleRegistry)
      : IDataStorage(fileName), mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::DATA_STORAGE)) {}
    virtual ~FileStorage() = default;
    virtual Optional<ByteStream> LoadData() override {
        IFStream file(mURI, std::ios_base::binary);
        if (!file.is_open()) {
            mLog->error("Failed to open file '{}'", mURI);
            return {};
        }

        return ByteStream(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    virtual bool SaveData(const ByteStream& data) override {
        if (data.size() == 0) {
            mLog->warn("No data to save in file {}", mURI);
            return true;
        }

        auto tmpFileName = mURI + ".tmp";
        OFStream tmpFile(tmpFileName);
        if (!tmpFile.is_open()) {
            mLog->error("Failed to open file {} to save data", tmpFileName);
            return false;
        }

        tmpFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        tmpFile.flush();
        tmpFile.close();
        if (!tmpFile.good()) {
            mLog->error("Failed to save data to file {}", tmpFileName);
            std::filesystem::remove(std::filesystem::path(tmpFileName));
            return false;
        }
        
        std::error_code errCode = {};
        std::filesystem::rename(std::filesystem::path(tmpFileName), mURI, errCode);
        if (errCode) {
            mLog->error("Failed to save temporary filename {} into target filename {}. Error: {}",
                tmpFileName, mURI, errCode.message());
            return false;
        }

        errCode.clear();
        std::filesystem::remove(std::filesystem::path(tmpFileName), errCode);
        if (errCode) {
            mLog->warn("Failed to remove temporary filename {}. Error: {}",
                tmpFileName, errCode.message());
        }

        return true;
    }

protected:
    SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;
}; // class FileStorage
} // namespace Storage
