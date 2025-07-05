#pragma once

#include "FileStorage.hpp"

#include "JsonCommon.hpp"

namespace Storage {
class JsonFileStorage : public FileStorage {
public:
    JsonFileStorage(const String& fileName, const SharedPtr<ModuleRegistry>& moduleRegistry)
      : FileStorage(fileName, moduleRegistry) {}
    virtual ~JsonFileStorage() = default;
    Optional<ByteStream> LoadData() override final {
        IFStream jsonFile(mURI);
        if (!jsonFile.is_open()) {
            mLog->error("Failed to open file '{}'", mURI);
            return {};
        }

        try {
            // Let's iterate over other files in current dir and try to load other (sub)files
            Json::JSON jData = Json::JSON::parse(jsonFile);
            const std::filesystem::path filePath = mURI;
            for(const auto& otherFile: std::filesystem::recursive_directory_iterator(filePath.parent_path())) {
                if (std::filesystem::is_directory(otherFile)
                    || (otherFile.path() == mURI)) {
                    continue;
                }

                IFStream subFile(otherFile.path());
                auto jCombinedPatch = Json::JSON::array();
                size_t i = 0;
                for (auto& diffItem : Json::JSON::diff(jData, Json::JSON::parse(subFile))) {
                    if (diffItem[Json::Diff::Field::OPERATION] == Json::Diff::Operation::ADD) {
                        jCombinedPatch[i++] = diffItem;
                    }
                }

                jData = jData.patch(jCombinedPatch);
            }

            if (jData.empty()) {
                mLog->error("JSON file '{}' is empty", mURI);
                return {};
            }

            mLog->trace("Successfully loaded JSON data from file '{}':\n{}", mURI, jData.dump(Json::DEFAULT_OUTPUT_INDENT));
            String jStrData = jData.dump();
            return ByteStream(jStrData.begin(), jStrData.end());
        }
        catch (const Exception &ex) {
            mLog->error("Failed to load JSON data from file '{}'. Error: {}", mURI, ex.what());
            return {};
        }

        mLog->error("Failed to load JSON data from file '{}'", mURI);
        return {};
    }

    bool SaveData(const ByteStream& data) override final {
        if (data.size() == 0) {
            mLog->error("No JSON data to save into file '{}'", mURI);
            return false;
        }

        try {
            auto jData = Json::JSON::parse(data).dump(Json::DEFAULT_OUTPUT_INDENT);
            return FileStorage::SaveData(ByteStream(jData.begin(), jData.end()));
        }
        catch (const Exception &ex) {
            mLog->error("Failed to save JSON data to destination '{}'. Error: {}", mURI, ex.what());
            return false;
        }

        return false;
    }

}; // class JsonFileStorage
} // namespace Storage
