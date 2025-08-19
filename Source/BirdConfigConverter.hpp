/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "IConfigConverting.hpp"

#include "JsonCommon.hpp"
#include "JsonSchemaProperties.hpp"
#include "Lib/ModuleRegistry.hpp"
#include "Lib/Utils.hpp"
#include "Modules.hpp"

namespace BirdConfigTree {
using namespace StdLib;
class ConfigNodeRendering {
public:
    virtual ~ConfigNodeRendering() = default;
    virtual String Prolog() = 0;
    virtual String Epilog() = 0;
};

class ProtocolBgp : public ConfigNodeRendering {
public:
    ProtocolBgp(const String& sessionName) : mSessionName(sessionName) {}
    virtual ~ProtocolBgp() = default;

    String Prolog() override {
        return String("protocol bgp '") + mSessionName + "' {\n";
    }

    String Epilog() override {
        return String("}\n");
    }

private:
    const String mSessionName;
};
} // namespace BirdConfigTree

namespace Config {
using namespace BirdConfigTree;
using namespace Json::Schema;
using namespace StdLib;
class BirdConfigConverter : public IConfigConverting {
public:
    BirdConfigConverter(const SharedPtr<ModuleRegistry>& moduleRegistry)
      : mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::CONFIG_TRANSL)) {}
    virtual ~BirdConfigConverter() = default;
    Optional<ByteStream> Convert(const ByteStream& config) override {
        mAlreadyTakenListName.clear();
        Json::JSON jConfig;
        try {
            Stack<UniquePtr<ConfigNodeRendering>> configNodes;
            jConfig = Json::JSON::parse(config);

            auto birdConfig = std::make_shared<OStrStream>();
            Optional<String> birdConfigPart;

            birdConfigPart = RenderMiscOptions(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render misc config options");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderGlobalRouterInfo(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render global info about local router");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderDeviceProtocol(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render device protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderKernelProtocol(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render kernel protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderDirectProtocol(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render direct protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderBgpProtocol(jConfig, configNodes);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render bgp protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderStaticProtocol(jConfig, configNodes);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render static protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            auto birdConfigStr = birdConfig->str();
            mLog->trace("Converted JSON config into BIRD config:\n{}", birdConfigStr);
            return ByteStream(std::begin(birdConfigStr), std::end(birdConfigStr));
        }
        catch (const Exception &ex) {
            mLog->error("Failed to convert JSON data into BIRD config. Error: {}", ex.what());
            return {};
        }
        catch (...) {
            mLog->error("Unexpected exception during converting JSON data into BIRD config");
            return {};
        }

        return {};
    }

private:
    SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;
    Map<String, String> mAlreadyTakenListName;

    static constexpr size_t DEFAULT_INDENT = 4;
    static constexpr String NEW_LINE = "\n";

    static constexpr String NET_TYPE_IP4 = "ipv4";
    static constexpr String NET_TYPE_IP6 = "ipv6";

    static constexpr String SRC_PROTO_BGP = "BGP";
    static constexpr String SRC_PROTO_STATIC = "STATIC";

    enum class IfMatchType : uint8_t {
        ANY,
        ALL
    };

    // GetPrefixMaxLen() calculates max length mask based on prefix type - 32 for IPv4 and 128 for IPv6
    uint16_t GetPrefixMaxLen(const String& pfx) {
        // The 'colon' character should be only in IPv6 address
        if (pfx.find(":") != String::npos) {
            return 128; // It's IPv6 prefix
        }

        return 32;// It's IPv4 prefix
    }

    Optional<String> RenderMiscOptions(const Json::JSON& jConfig) {
        OStrStream headerStmt;
        headerStmt << "log syslog all;" << NEW_LINE;
        headerStmt << "watchdog warning 5 s;" << NEW_LINE;
        return headerStmt.str();
    }

    /** RenderBgpAsPathListSection expects JSON data inside of "as-path-list" property/node */
    Optional<String> RenderBgpAsPathListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream asPathListSection;
        auto asPathListIt = jConfigParent.find(Property::AS_PATH_LIST);
        if (asPathListIt == jConfigParent.end()) {
            return "";
        }

        asPathListSection << "############\n# ASN-SETS #\n############" << NEW_LINE;

        for (auto& [asPathListName, asPathListDetails] : asPathListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(asPathListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", asPathListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[asPathListName] = Property::AS_PATH_LIST;
            asPathListSection << "define " << asPathListName << " = [";
            auto asPath = asPathListDetails.template get<std::vector<uint16_t>>();
            for (size_t i = 0; i < asPath.size() - 1; ++i) {
                asPathListSection << std::to_string(asPath[i]) << ", ";
            }

            asPathListSection << asPath[asPath.size() - 1] << "];" << NEW_LINE;
        }

        return asPathListSection.str();
    }

    // The following helper methods render globally accessible lists like AS-PATH-LISTS, COMMUNITY-LISTS, FILTER-LISTS, PREFIX-LISTS
    /** RenderBgpCommunityListSection expects JSON data inside of "community-list" property/node */
    Optional<String> RenderBgpCommunityListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream communityListSection;
        auto communityListIt = jConfigParent.find(Property::COMMUNITY_LIST);
        if (communityListIt == jConfigParent.end()) {
            return "";
        }

        communityListSection << "###############\n# COMMUNITIES #\n###############" << NEW_LINE;
        for (auto& [communityListName, communityDetails] : communityListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(communityListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", communityListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[communityListName] = Property::COMMUNITY_LIST;

            communityListSection << "define " << communityListName << " = ";
            auto commList = communityDetails.template get<std::vector<String>>();
            if (commList.size() > 1) {
                communityListSection << "[";
            }

            communityListSection << "(" << Utils::fFindAndReplaceAll(commList[0], ":", ",") << ")";
            for (size_t i = 1; i < commList.size(); ++i) {
                communityListSection << ",(" << Utils::fFindAndReplaceAll(commList[i], ":", ",") << ")";
            }

            if (commList.size() > 1) {
                communityListSection << "]";
            }
            
            communityListSection << ";" << NEW_LINE;
        }

        return communityListSection.str();
    }

    /** RenderBgpExtCommunityListSection expects JSON data inside of "ext-community-list" property/node */
    Optional<String> RenderBgpExtCommunityListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream extCommunityListSection;
        auto extCommunityListIt = jConfigParent.find(Property::EXT_COMMUNITY_LIST);
        if (extCommunityListIt == jConfigParent.end()) {
            return "";
        }

        extCommunityListSection << "###################\n# EXT-COMMUNITIES #\n###################" << NEW_LINE;
        for (auto& [extCommunityListName, extCommunityDetails] : extCommunityListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(extCommunityListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", extCommunityListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[extCommunityListName] = Property::EXT_COMMUNITY_LIST;

            extCommunityListSection << "define " << extCommunityListName << " = ";
            auto extCommList = extCommunityDetails.template get<std::vector<String>>();
            if (extCommList.size() > 1) {
                extCommunityListSection << "[";
            }

            extCommunityListSection << "(" << Utils::fFindAndReplaceAll(extCommList[0], ":", ",") << ")";
            for (size_t i = 1; i < extCommList.size(); ++i) {
                extCommunityListSection << ",(" << Utils::fFindAndReplaceAll(extCommList[i], ":", ",") << ")";
            }

            if (extCommList.size() > 1) {
                extCommunityListSection << "]";
            }
            
            extCommunityListSection << ";" << NEW_LINE;
        }

        return extCommunityListSection.str();
    }

    /** RenderBgpLargeCommunityListSection expects JSON data inside of "large-community-list" property/node */
    Optional<String> RenderBgpLargeCommunityListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream largeCommunityListSection;
        auto largeCommunityListIt = jConfigParent.find(Property::LARGE_COMMUNITY_LIST);
        if (largeCommunityListIt == jConfigParent.end()) {
            return "";
        }

        largeCommunityListSection << "#####################\n# LARGE-COMMUNITIES #\n#####################" << NEW_LINE;
        for (auto& [largeCommunityListName, largeCommunityDetails] : largeCommunityListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(largeCommunityListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", largeCommunityListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[largeCommunityListName] = Property::LARGE_COMMUNITY_LIST;

            largeCommunityListSection << "define " << largeCommunityListName << " = ";
            auto largeCommList = largeCommunityDetails.template get<std::vector<String>>();
            if (largeCommList.size() > 1) {
                largeCommunityListSection << "[";
            }

            largeCommunityListSection << "(" << Utils::fFindAndReplaceAll(largeCommList[0], ":", ",") << ")";
            for (size_t i = 1; i < largeCommList.size(); ++i) {
                largeCommunityListSection << ",(" << Utils::fFindAndReplaceAll(largeCommList[i], ":", ",") << ")";
            }

            if (largeCommList.size() > 1) {
                largeCommunityListSection << "]";
            }
            
            largeCommunityListSection << ";" << NEW_LINE;
        }

        return largeCommunityListSection.str();
    }

    /** RenderBgpPolicyListSection expects JSON data inside of "policy-list" property/node */
    Optional<String> RenderBgpPolicyListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream filtersSection;
        auto policyListIt = jConfigParent.find(Property::POLICY_LIST);
        if (policyListIt == jConfigParent.end()) {
            return "";
        }

        filtersSection << "###########\n# FILTERS #\n###########)" << NEW_LINE;

        for (auto& [policyListName, policyDetails] : policyListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(policyListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", policyListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[policyListName] = Property::POLICY_LIST;
            filtersSection << String(indentSize, ' ') << "filter " << policyListName << " {" << NEW_LINE;
            for (auto& [termName, termDetails] : policyDetails.items()) {
                auto termOutput = RenderBgpPolicyIfStatement(jConfigBgpRoot, termDetails, indentSize + DEFAULT_INDENT);
                if (!termOutput.has_value()) {
                    mLog->error("Failed to render term '{}'", termName);
                    return {};
                }

                filtersSection << termOutput.value();
            }

            auto defaultActionIt = policyListIt->find(Property::DEFAULT_ACTION);
            String defaultAction;
            if (defaultActionIt == policyListIt->end()) {
                defaultAction = "reject";
            }
            else {
                defaultAction = defaultActionIt.value().template get<String>();
            }

            filtersSection << String(indentSize + DEFAULT_INDENT, ' ') << defaultAction << ";" << NEW_LINE;
            filtersSection << String(indentSize, ' ') << "}" << NEW_LINE;
        }

        return filtersSection.str();
    }

    Optional<String> RenderBgpPrefixIpCommonListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize,
            const String& sectionHeader, const String& propertyPrefixList, const uint16_t maxPfxLen) {
        OStrStream pfxIpListSection;
        auto pfxIpListIt = jConfigParent.find(propertyPrefixList);
        if (pfxIpListIt == jConfigParent.end()) {
            return "";
        }

        pfxIpListSection << sectionHeader << NEW_LINE;

        for (auto& [pfxListName, pfxList] : pfxIpListIt->items()) {
            auto listNameIt = mAlreadyTakenListName.find(pfxListName);
            if (listNameIt != mAlreadyTakenListName.end()) {
                mLog->error("There is already used list name '{}' in predefined list section '{}'", pfxListName, listNameIt->second);
                return {};
            }

            mAlreadyTakenListName[pfxListName] = propertyPrefixList;
            pfxIpListSection << "define " << pfxListName << " = [";
            for (auto& [pfx, attrs] : pfxList.items()) {
                pfxIpListSection << NEW_LINE << String(indentSize + DEFAULT_INDENT, ' ') << pfx;
                auto pfxLen = static_cast<uint16_t>(std::stoi(pfx.substr(pfx.find_last_of("/") + 1)));
                auto geIt = attrs.find(Property::PREFIX_GE_ATTR);
                auto leIt = attrs.find(Property::PREFIX_LE_ATTR);
                if ((geIt != attrs.end()) && (leIt != attrs.end())) {
                    auto minPfxRange = geIt.value().template get<uint16_t>();
                    auto maxPfxRange = leIt.value().template get<uint16_t>();
                    if ((pfxLen > minPfxRange) || (pfxLen > maxPfxRange) || (minPfxRange > maxPfxRange)) {
                        mLog->error("Invalid prefix range <{},{}>", minPfxRange, maxPfxRange);
                        return {};
                    }

                    pfxIpListSection << "{" << minPfxRange << "," << maxPfxRange << "}";
                }
                else if (geIt != attrs.end()) {
                    auto minPfxRange = geIt.value().template get<uint16_t>();
                    if (pfxLen > minPfxRange) {
                        mLog->error("Prefix len '{}' is higher than its minimum range '{}'", pfxLen, minPfxRange);
                        return {};
                    }

                    pfxIpListSection << "{" << minPfxRange << "," << maxPfxLen << "}";
                }
                else if (leIt != attrs.end()) {
                    auto maxPfxRange = leIt.value().template get<uint16_t>();
                    if (pfxLen > maxPfxRange) {
                        mLog->error("Prefix len '{}' is higher than its maximum range '{}'", pfxLen, maxPfxRange);
                        return {};
                    }

                    pfxIpListSection << "{" << pfxLen << "," << maxPfxRange << "}";
                }

                pfxIpListSection << ",";
            }

            // Let's get rid of comma ',' after last entry of the list
            const auto& pfxIpListSectionStr = pfxIpListSection.str();
            if (pfxIpListSectionStr[pfxIpListSectionStr.size() - 1] == ',') {
                pfxIpListSection.seekp(-1, std::ios_base::end);
                // pfxIpListSection << '\0'; // NULL terminating string
            }

            pfxIpListSection << NEW_LINE << "];" << NEW_LINE;
        }

        return pfxIpListSection.str();
    }

    /** RenderBgpPrefixIPv4ListSection expects JSON data inside of "prefix-v4-list" property/node */
    Optional<String> RenderBgpPrefixIPv4ListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpPrefixIpCommonListSection(jConfigBgpRoot, jConfigParent, indentSize,
                    "#####################\n# PREFIX-IPV4-LISTS #\n#####################",
                    Property::PREFIX_V4_LIST, 32);
    }

    /** RenderBgpPrefixIPv6ListSection expects JSON data inside of "prefix-v6-list" property/node */
    Optional<String> RenderBgpPrefixIPv6ListSection(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpPrefixIpCommonListSection(jConfigBgpRoot, jConfigParent, indentSize,
                    "#####################\n# PREFIX-IPV6-LISTS #\n#####################",
                    Property::PREFIX_V6_LIST, 128);
    }

    // This is section which represents conditional checks in if-statement
    Optional<String> RenderAsPathCommonCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize,
            const String& propertyAsPathCond, const String& condOp) {
        OStrStream asPathCheckStmt;
        auto asPathMatchIt = jConfigParent.find(propertyAsPathCond);
        if (asPathMatchIt == jConfigParent.end()) {
            return "";
        }

        auto asPathListIt = asPathMatchIt->find(Property::AS_PATH_LIST);
        if (asPathListIt != asPathMatchIt->end()) {
            if (!asPathListIt->is_string()) { // Reference to predefined AS-PATH list
                mLog->error("Unsupported type of as-path list property. Expected 'string' as predefined as-path list name");
                return {};
            }

            // Check if AS-PATH list exists
            auto asPathListSectionIt = jConfigBgpRoot.find(Property::AS_PATH_LIST);
            if (asPathListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("AS-PATH list section does not exist");
                return {};
            }

            auto asPathListName = asPathListIt.value().template get<String>();
            if (!asPathListSectionIt->contains(asPathListName)) {
                mLog->error("AS-PATH list '{}' does not exist", asPathListName);
                return {};
            }
                    
            asPathCheckStmt << "(bgp_path " << condOp << " " << asPathListName << ")";
        }
        else {
            if (!asPathMatchIt->is_array()) { // In-place AS-PATH list
                mLog->error("Unsupported type of as-path list property. Expected 'array' as list of as-paths");
                return {};
            }

            auto asPathList = asPathMatchIt.value().template get<Vector<uint32_t>>();
            asPathCheckStmt << "(bgp_path " << condOp << " [=";
            for (size_t i = 0; i < asPathList.size(); ++i) {
                asPathCheckStmt << " " << std::to_string(asPathList[i]);
            }

            asPathCheckStmt << " =])";
        }

        return asPathCheckStmt.str();
    }

    Optional<String> RenderAsPathEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderAsPathCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::AS_PATH_EQ, "=");
    }

    Optional<String> RenderAsPathInCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderAsPathCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::AS_PATH_IN, "~");
    }

    Optional<String> RenderBgpCommunityCommonCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize,
            const String& propertyCommunityCond, const String& condOp) {
        OStrStream commCheckStmt;
        auto commMatchIt = jConfigParent.find(propertyCommunityCond);
        if (commMatchIt == jConfigParent.end()) {
            return "";
        }

        auto commListIt = commMatchIt->find(Property::COMMUNITY_LIST);
        if (commListIt != commMatchIt->end()) {
            if (!commListIt->is_string()) { // Reference to predefined community list
                mLog->error("Unsupported type of community list property. Expected 'string' as predefined community list name");
                return {};
            }

            // Check if community list exists
            auto commListSectionIt = jConfigBgpRoot.find(Property::COMMUNITY_LIST);
            if (commListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Community list section does not exist");
                return {};
            }

            auto commListName = commListIt.value().template get<String>();
            if (!commListSectionIt->contains(commListName)) {
                mLog->error("Community list '{}' does not exist", commListName);
                return {};
            }
                    
            commCheckStmt << "(bgp_community " << condOp << " " << commListName << ")";
        }
        else { // In-place community list
            if (!commMatchIt->is_array()) {
                mLog->error("Unsupported type of community list property. Expected 'array' as list of communities");
                return {};
            }

            auto commList = commMatchIt.value().template get<Vector<String>>();
            commCheckStmt << "(bgp_community " << condOp << " [";
            for (size_t i = 0; i < commList.size() - 1; ++i) {
                commCheckStmt << "(" << Utils::fFindAndReplaceAll(commList[i], ":", ",") << "),";
            }

            commCheckStmt << "(" << Utils::fFindAndReplaceAll(commList[commList.size() - 1], ":", ",") << ")])";
        }

        return commCheckStmt.str();
    }

    Optional<String> RenderBgpCommunityEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpCommunityCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::COMMUNITY_EQ, "=");
    }

    Optional<String> RenderBgpCommunityInCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpCommunityCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::COMMUNITY_IN, "~");
    }

    Optional<String> RenderBgpExtCommunityCommonCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize,
            const String& propertyCommunityCond, const String& condOp) {
        OStrStream extCommCheckStmt;
        auto extCommMatchIt = jConfigParent.find(propertyCommunityCond);
        if (extCommMatchIt == jConfigParent.end()) {
            return "";
        }

        auto extCommListIt = extCommMatchIt->find(Property::EXT_COMMUNITY_LIST);
        if (extCommListIt != extCommMatchIt->end()) {
            if (!extCommListIt->is_string()) { // Reference to predefined ext-community list
                mLog->error("Unsupported type of extended community list property. Expected 'string' as predefined extended community list name");
                return {};
            }

            // Check if community list exists
            auto extCommListSectionIt = jConfigBgpRoot.find(Property::EXT_COMMUNITY_LIST);
            if (extCommListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Extended community list section does not exist");
                return {};
            }

            auto extCommListName = extCommListIt.value().template get<String>();
            if (!extCommListSectionIt->contains(extCommListName)) {
                mLog->error("Extended community list '{}' does not exist", extCommListName);
                return {};
            }
                    
            extCommCheckStmt << "(bgp_ext_community " << condOp << " " << extCommListName << ")";
        }
        else { // In-place ext-community list
            if (!extCommMatchIt->is_array()) {
                mLog->error("Unsupported type of extended community list property. Expected 'array' as list of extended communities");
                return {};
            }

            auto extCommList = extCommMatchIt.value().template get<Vector<String>>();
            extCommCheckStmt << "(bgp_ext_community " << condOp << " [";
            for (size_t i = 0; i < extCommList.size() - 1; ++i) {
                extCommCheckStmt << "(" << Utils::fFindAndReplaceAll(extCommList[i], ":", ",") << "),";
            }

            extCommCheckStmt << "(" << Utils::fFindAndReplaceAll(extCommList[extCommList.size() - 1], ":", ",") << ")])";
        }

        return extCommCheckStmt.str();
    }

    Optional<String> RenderBgpExtCommunityEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpExtCommunityCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::EXT_COMMUNITY_EQ, "=");
    }

    Optional<String> RenderBgpExtCommunityInCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        return RenderBgpExtCommunityCommonCheckStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::EXT_COMMUNITY_IN, "~");
    }

    Optional<String> RenderBgpNetEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream netCheckStmt;
        auto netEqIt = jConfigParent.find(Property::NET_EQ);
        if (netEqIt == jConfigParent.end()) {
            return "";
        }

        auto pfxIPv4It = netEqIt->find(Property::PREFIX_V4);
        if ((pfxIPv4It != netEqIt->end()) && (pfxIPv4It->begin() != pfxIPv4It->end())) {
            const auto& pfx = pfxIPv4It->begin().key();
            const auto& attrs = pfxIPv4It->begin().value();
            netCheckStmt << "(net = " << pfx;
            auto pfxLen = static_cast<uint16_t>(std::stoi(pfx.substr(pfx.find_last_of("/") + 1)));
            auto geIt = attrs.find(Property::PREFIX_GE_ATTR);
            auto leIt = attrs.find(Property::PREFIX_LE_ATTR);
            if ((geIt != attrs.end()) && (leIt != attrs.end())) {
                auto minPfxRange = geIt.value().template get<uint16_t>();
                auto maxPfxRange = leIt.value().template get<uint16_t>();
                if ((pfxLen > minPfxRange) || (pfxLen > maxPfxRange) || (minPfxRange > maxPfxRange)) {
                    mLog->error("Invalid prefix range <{},{}>", minPfxRange, maxPfxRange);
                    return {};
                }

                netCheckStmt << "{" << minPfxRange << "," << maxPfxRange << "}";
            }
            else if (geIt != attrs.end()) {
                auto minPfxRange = geIt.value().template get<uint16_t>();
                if (pfxLen > minPfxRange) {
                    mLog->error("Prefix len '{}' is higher than its minimum range '{}'", pfxLen, minPfxRange);
                    return {};
                }

                netCheckStmt << "{" << minPfxRange << ",32}";
            }
            else if (leIt != attrs.end()) {
                auto maxPfxRange = leIt.value().template get<uint16_t>();
                if (pfxLen > maxPfxRange) {
                    mLog->error("Prefix len '{}' is higher than its maximum range '{}'", pfxLen, maxPfxRange);
                    return {};
                }

                netCheckStmt << "{" << pfxLen << "," << maxPfxRange << "}";
            }

            netCheckStmt << ")";
        }

        auto pfxIPv6It = netEqIt->find(Property::PREFIX_V6);
        if ((pfxIPv6It != netEqIt->end()) && (pfxIPv6It->begin() != pfxIPv6It->end())) {
            const auto& pfx = pfxIPv6It->begin().key();
            const auto& attrs = pfxIPv6It->begin().value();
            netCheckStmt << "(net = " << pfx;
            auto pfxLen = static_cast<uint16_t>(std::stoi(pfx.substr(pfx.find_last_of("/") + 1)));
            auto geIt = attrs.find(Property::PREFIX_GE_ATTR);
            auto leIt = attrs.find(Property::PREFIX_LE_ATTR);
            if ((geIt != attrs.end()) && (leIt != attrs.end())) {
                auto minPfxRange = geIt.value().template get<uint16_t>();
                auto maxPfxRange = leIt.value().template get<uint16_t>();
                if ((pfxLen > minPfxRange) || (pfxLen > maxPfxRange) || (minPfxRange > maxPfxRange)) {
                    mLog->error("Invalid prefix range <{},{}>", minPfxRange, maxPfxRange);
                    return {};
                }

                netCheckStmt << "{" << minPfxRange << "," << maxPfxRange << "}";
            }
            else if (geIt != attrs.end()) {
                auto minPfxRange = geIt.value().template get<uint16_t>();
                if (pfxLen > minPfxRange) {
                    mLog->error("Prefix len '{}' is higher than its minimum range '{}'", pfxLen, minPfxRange);
                    return {};
                }

                netCheckStmt << "{" << minPfxRange << ",128}";
            }
            else if (leIt != attrs.end()) {
                auto maxPfxRange = leIt.value().template get<uint16_t>();
                if (pfxLen > maxPfxRange) {
                    mLog->error("Prefix len '{}' is higher than its maximum range '{}'", pfxLen, maxPfxRange);
                    return {};
                }

                netCheckStmt << "{" << pfxLen << "," << maxPfxRange << "}";
            }

            netCheckStmt << ")";
        }

        return netCheckStmt.str();
    }

    Optional<String> RenderBgpNetInCheckCommonStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize,
            const String& propertyPfxList, const String& propertyPfxIP, const String& pfxMaxLen) {
        OStrStream netCheckStmt;
        auto netMatchIt = jConfigParent.find(propertyPfxList);
        if (netMatchIt == jConfigParent.end()) {
            return "";
        }

        if (netMatchIt->find(propertyPfxList) != netMatchIt->end()) {
            auto netIpListIt = netMatchIt->find(propertyPfxList);
            if (!netIpListIt->is_string()) { // Reference to predefined prefix IP list
                mLog->error("Unsupported type of prefix IP list property. Expected 'string' as predefined prefix IP list name");
                return {};
            }

            // Check if prefix IP list exists
            auto prefixIPListSectionIt = jConfigBgpRoot.find(propertyPfxList);
            if (prefixIPListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Prefix IP list section does not exist");
                return {};
            }

            auto prefixIPListName = netIpListIt.value().template get<String>();
            if (!prefixIPListSectionIt->contains(prefixIPListName)) {
                mLog->error("Prefix IP list '{}' does not exist", prefixIPListName);
                return {};
            }
                    
            netCheckStmt << "(net ~ " << prefixIPListName << ")";
        }
        else if (netMatchIt->find(propertyPfxIP) != netMatchIt->end()) { // In-place prefixes IP
            netCheckStmt << "(net ~ [";
            for (const auto& [pfx, attrs] : netMatchIt->at(propertyPfxIP).items()) {
                netCheckStmt << pfx;
                auto pfxLen = static_cast<uint16_t>(std::stoi(pfx.substr(pfx.find_last_of("/") + 1)));
                auto geIt = attrs.find(Property::PREFIX_GE_ATTR);
                auto leIt = attrs.find(Property::PREFIX_LE_ATTR);
                if ((geIt != attrs.end()) && (leIt != attrs.end())) {
                    auto minPfxRange = geIt.value().template get<uint16_t>();
                    auto maxPfxRange = leIt.value().template get<uint16_t>();
                    if ((pfxLen > minPfxRange) || (pfxLen > maxPfxRange) || (minPfxRange > maxPfxRange)) {
                        mLog->error("Invalid prefix range <{},{}>", minPfxRange, maxPfxRange);
                        return {};
                    }

                    netCheckStmt << "{" << minPfxRange << "," << maxPfxRange << "}";
                }
                else if (geIt != attrs.end()) {
                    auto minPfxRange = geIt.value().template get<uint16_t>();
                    if (pfxLen > minPfxRange) {
                        mLog->error("Prefix len '{}' is higher than its minimum range '{}'", pfxLen, minPfxRange);
                        return {};
                    }

                    netCheckStmt << "{" << minPfxRange << "," << pfxMaxLen << "}";
                }
                else if (leIt != attrs.end()) {
                    auto maxPfxRange = leIt.value().template get<uint16_t>();
                    if (pfxLen > maxPfxRange) {
                        mLog->error("Prefix len '{}' is higher than its maximum range '{}'", pfxLen, maxPfxRange);
                        return {};
                    }

                    netCheckStmt << "{" << pfxLen << "," << maxPfxRange << "}";
                }

                netCheckStmt << ",";
            }

            // Let's get rid of comma ',' after last entry of the list
            const auto& netCheckStmtStr = netCheckStmt.str();
            if (netCheckStmtStr[netCheckStmtStr.size() - 1] == ',') {
                netCheckStmt.seekp(-1, std::ios_base::end);
                // netCheckStmt << '\0'; // NULL terminating string
            }

            netCheckStmt << "])";
        }

        return netCheckStmt.str();
    }

    Optional<String> RenderBgpNetInCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream netCheckStmt;
        auto netMatchIt = jConfigParent.find(Property::NET_IN);
        if (netMatchIt == jConfigParent.end()) {
            return "";
        }

        auto perIpNetCheckStmt = RenderBgpNetInCheckCommonStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::PREFIX_V4_LIST, Property::PREFIX_V4, "32");
        if (!perIpNetCheckStmt.has_value()) {
            mLog->error("Failed to render prefix IPv4 check in");
            return {};
        }

        if (!perIpNetCheckStmt.value().empty()) {
            return perIpNetCheckStmt.value(); 
        }

        perIpNetCheckStmt = RenderBgpNetInCheckCommonStatement(jConfigBgpRoot, jConfigParent, indentSize, Property::PREFIX_V6_LIST, Property::PREFIX_V6, "128");
        if (!perIpNetCheckStmt.has_value()) {
            mLog->error("Failed to render prefix IPv6 check in");
            return {};
        }

        return perIpNetCheckStmt.value();
    }

    Optional<String> RenderBgpNetTypeEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream netTypeCheckStmt;
        auto netTypeMatchIt = jConfigParent.find(Property::NET_TYPE_EQ);
        if (netTypeMatchIt == jConfigParent.end()) {
            return "";
        }

        auto netType = netTypeMatchIt.value().template get<String>();
        if (netType == NET_TYPE_IP4) {
            netTypeCheckStmt << "(net.type = NET_IP4)";
        }
        else if (netType == NET_TYPE_IP6) {
            netTypeCheckStmt << "(net.type = NET_IP6)";
        }
        else {
            mLog->error("Unsupported value of '{}'", Property::NET_TYPE_EQ);
            return {};
        }

        return netTypeCheckStmt.str();
    }

    Optional<String> RenderSourceProtocolEqCheckStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream srcProtoCheckStmt;
        auto srcProtoMatchIt = jConfigParent.find(Property::SOURCE_PROTOCOL_EQ);
        if (srcProtoMatchIt == jConfigParent.end()) {
            return "";
        }

        auto srcProto = srcProtoMatchIt.value().template get<String>();
        if (srcProto == SRC_PROTO_BGP) {
            srcProtoCheckStmt << "(source = RTS_BGP)";
        }
        else if (srcProto == SRC_PROTO_STATIC) {
            srcProtoCheckStmt << "(source = RTS_STATIC)";
        }
        else {
            mLog->error("Unsupported value of '{}'", Property::SOURCE_PROTOCOL_EQ);
            return {};
        }

        return srcProtoCheckStmt.str();
    }

    // This is section responsible for rendering action
    Optional<String> RenderBgpAsPathPrependStmt(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream asPathPrependStmt;
        auto asPathPrependStmtIt = jConfigParent.find(Property::AS_PATH_PREPEND);
        if (asPathPrependStmtIt == jConfigParent.end()) {
            return "";
        }

        auto asnIt = asPathPrependStmtIt->find(Property::ASN);
        if (asnIt == asPathPrependStmtIt->end()) {
            mLog->error("Missing mandatory property '{}'", Property::ASN);
            return {};
        }

        auto asn = asnIt.value().template get<uint16_t>();
        uint16_t count = 1;
        if (asPathPrependStmtIt->contains(Property::N_TIMES)) {
            count = asPathPrependStmtIt->at(Property::N_TIMES).template get<uint16_t>();
        }

        asPathPrependStmt << String(indentSize, ' ');
        for (uint16_t i = 0; i < count; ++i) {
            asPathPrependStmt << "bgp_path.prepend(" << std::to_string(asn) << "); ";
        }

        return asPathPrependStmt.str();
    }

    Optional<String> RenderBgpLocalPreferenceSetStmt(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream localPrefSetStmt;
        auto localPrefStmtIt = jConfigParent.find(Property::LOCAL_PREFERENCE_SET);
        if (localPrefStmtIt == jConfigParent.end()) {
            return "";
        }

        const uint32_t localPrefVal = localPrefStmtIt.value().template get<uint32_t>();
        localPrefSetStmt << String(indentSize, ' ') << "bgp_local_pref=" << std::to_string(localPrefVal) << ";";
        return localPrefSetStmt.str();
    }

    Optional<String> RenderBgpMedSetStmt(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream medSetStmt;
        auto medStmtIt = jConfigParent.find(Property::MED_SET);
        if (medStmtIt == jConfigParent.end()) {
            return "";
        }

        const uint32_t medVal = medStmtIt.value().template get<uint32_t>();
        medSetStmt << String(indentSize, ' ') << "bgp_med=" << std::to_string(medVal) << ";";
        return medSetStmt.str();
    }

    Optional<String> RenderBgpCommunityAddStmt(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream commAddStmt;
        auto commAddActionIt = jConfigParent.find(Property::COMMUNITY_ADD);
        if (commAddActionIt != jConfigParent.end()) {
            if (commAddActionIt->is_string()) {
                commAddStmt << String(indentSize, ' ') << "bgp_community.add(";
                commAddStmt << "(" << Utils::fFindAndReplaceAll(commAddActionIt.value().template get<String>(), ":", ",") << "));";
            }
            else { // It is an object
                // Check if community list exists
                auto commListSectionIt = jConfigBgpRoot.find(Property::COMMUNITY_LIST);
                if (commListSectionIt == jConfigBgpRoot.end()) {
                    mLog->error("Community list section does not exist");
                    return {};
                }

                auto commListIt = commAddActionIt->find(Property::COMMUNITY_LIST);
                if (commListIt == commAddActionIt->end()) {
                    mLog->error("Not found key '' in JSON data", Property::COMMUNITY_LIST);
                    return {};
                }

                // bgp_community.add() expects clist / quad / ip / int / pair. Let's check if it is not a set
                auto commListName = commListIt.value().template get<String>();
                auto commListEntryIt = commListSectionIt->find(commListName);
                if (commListEntryIt == commListSectionIt->end()) {
                    mLog->error("Community list '{}' does not exist", commListName);
                    return {};
                }

                auto commList = commListEntryIt.value().template get<Vector<String>>();
                if (commList.size() > 1) {
                    mLog->error("BGP community allows to add only single value/community. The community list '{}' consists of {} communities", commListName, commList.size());
                    return {};
                }
                    
                commAddStmt << String(indentSize, ' ') << "bgp_community.add(" << commListName << ");";
            }
        }

        return commAddStmt.str();
    }

    Optional<String> RenderBgpCommunityRemoveStmt(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream commDelStmt;
        auto commDelActionIt = jConfigParent.find(Property::COMMUNITY_REMOVE);
        if (commDelActionIt == jConfigParent.end()) {
            return "";
        }

        if (commDelActionIt->is_array()) {
            auto commList = commDelActionIt.value().template get<Vector<String>>();
            commDelStmt << String(indentSize, ' ') << "bgp_community.delete(";
            if (commList.size() > 1) {
                commDelStmt << "[";
            }

            for (size_t i = 0; i < (commList.size() - 1); ++i) {
                commDelStmt << "(" << Utils::fFindAndReplaceAll(commList[i], ":", ",") << "),";
            }

            commDelStmt << "(" << Utils::fFindAndReplaceAll(commList[commList.size() - 1], ":", ",") << ")";
            if (commList.size() > 1) {
                commDelStmt << "]);";
            }
        }
        else { // It is an object
            // Check if community list exists
            auto commListSectionIt = jConfigBgpRoot.find(Property::COMMUNITY_LIST);
            if (commListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Community list section does not exist");
                return {};
            }

            auto commListIt = commDelActionIt->find(Property::COMMUNITY_LIST);
            if (commListIt == commDelActionIt->end()) {
                mLog->error("Not found key '' in JSON data", Property::COMMUNITY_LIST);
                return {};
            }

            auto commListName = commListIt.value().template get<String>();
            auto commListEntryIt = commListSectionIt->find(commListName);
            if (commListEntryIt == commListSectionIt->end()) {
                mLog->error("Community list '{}' does not exist", commListName);
                return {};
            }
                    
            commDelStmt << String(indentSize, ' ') << "bgp_community.delete(" << commListName << ");";
        }

        return commDelStmt.str();
    }

    Optional<String> RenderNextHopSelfStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream nextHopSelfStmt;
        if (jConfigParent.find(Property::NEXT_HOP_SELF) != jConfigParent.end()) {
            nextHopSelfStmt << "next hop self ";
            if (jConfigParent[Property::NEXT_HOP_SELF].template get<bool>()) {
                nextHopSelfStmt << "on;";
            }
            else {
                nextHopSelfStmt << "off;";
            }
        }
        else if (jConfigBgpRoot.find(Property::IBGP) != jConfigBgpRoot.end()) {
            if (jConfigBgpRoot[Property::IBGP].find(Property::NEXT_HOP_SELF) != jConfigBgpRoot[Property::IBGP].end()) {
                nextHopSelfStmt << "next hop self ";
                if (jConfigBgpRoot[Property::IBGP][Property::NEXT_HOP_SELF].template get<bool>()) {
                    nextHopSelfStmt << "ibgp;";
                }
            }
        }
        else if (jConfigBgpRoot.find(Property::EBGP) != jConfigBgpRoot.end()) {
            if (jConfigBgpRoot[Property::EBGP].find(Property::NEXT_HOP_SELF) != jConfigBgpRoot[Property::EBGP].end()) {
                nextHopSelfStmt << "next hop self ";
                if (jConfigBgpRoot[Property::EBGP][Property::NEXT_HOP_SELF].template get<bool>()) {
                    nextHopSelfStmt << "ebgp;";
                }
            }
        }

        return nextHopSelfStmt.str();
    }

    Optional<String> RenderApplyBgpPolicy(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream filterStmtOutput;
        auto policyInIt = jConfigParent.find(Property::POLICY_IN);
        if (policyInIt != jConfigParent.end()) {
            auto policyName = policyInIt.value().template get<String>();
            auto policyListSectionIt = jConfigBgpRoot.find(Property::POLICY_LIST);
            if (policyListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Not found key '' in JSON data", Property::POLICY_LIST);
                return {};
            }

            if (!policyListSectionIt->contains(policyName)) {
                mLog->error("Policy list '{}' does not exist. It is required by '{}' property", policyName, Property::POLICY_IN);
                return {};
            }

            filterStmtOutput << String(indentSize, ' ') << "import filter " << policyName << ";" << NEW_LINE;
        }

        auto policyOutIt = jConfigParent.find(Property::POLICY_OUT);
        if (policyOutIt != jConfigParent.end()) {
            auto policyName = policyOutIt.value().template get<String>();
            auto policyListSectionIt = jConfigBgpRoot.find(Property::POLICY_LIST);
            if (policyListSectionIt == jConfigBgpRoot.end()) {
                mLog->error("Not found key '' in JSON data", Property::POLICY_LIST);
                return {};
            }

            if (!policyListSectionIt->contains(policyName)) {
                mLog->error("Policy list '{}' does not exist. It is required by '{}' property", policyName, Property::POLICY_OUT);
                return {};
            }

            filterStmtOutput << String(indentSize, ' ') << "export filter " << policyName << ";" << NEW_LINE;
        }

        return filterStmtOutput.str();
    }

    Optional<String> RenderRouterId(const Json::JSON& jConfig, const size_t indentSize) {
        auto routerIdIt = jConfig.find(Property::ROUTER_ID);
        if (routerIdIt == jConfig.end()) {
            return {};
        }

        return String(indentSize, ' ') + "router id " + routerIdIt.value().template get<String>() + ";" + NEW_LINE;
    }

    Optional<String> RenderGlobalRouterInfo(const Json::JSON& jConfig) {
        auto routerIdStmt = RenderRouterId(jConfig, 0);
        if (!routerIdStmt.has_value()) {
            mLog->error("Not found key '{}' in JSON data", Property::ROUTER_ID);
            return {};
        }
        
        return routerIdStmt.value();
    }

    Optional<String> RenderBgpProtocol(const Json::JSON& jConfig, Stack<UniquePtr<ConfigNodeRendering>>& configNodes) {
        const size_t indent = 0;
        auto bgpIt = jConfig.find(Property::BGP);
        if (bgpIt == jConfig.end()) {
            return "";
        }

        OStrStream birdBgpFullConfig;
        Optional<String> birdBgpConfigPart;
        birdBgpConfigPart = RenderBgpAsPathListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render AS Path list section");
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpCommunityListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render '{}' section", Property::COMMUNITY_LIST);
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpExtCommunityListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render '{}' section", Property::EXT_COMMUNITY_LIST);
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpLargeCommunityListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render '{}' section", Property::LARGE_COMMUNITY_LIST);
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpPrefixIPv4ListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render prefix IPv4 list section");
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpPrefixIPv6ListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render prefix IPv6 list section");
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        birdBgpConfigPart = RenderBgpPolicyListSection(*bgpIt, *bgpIt, 0);
        if (!birdBgpConfigPart.has_value()) {
            mLog->error("Failed to render policy list section");
            return {};
        }

        birdBgpFullConfig << birdBgpConfigPart.value() << NEW_LINE;

        auto sessionsIt = bgpIt->find(Property::SESSIONS);
        if (sessionsIt == bgpIt->end()) {
            return "";
        }

        for (auto& [sessionName, sessionDetails] : sessionsIt->items()) {
            configNodes.emplace(std::make_unique<ProtocolBgp>(sessionName));
            birdBgpFullConfig << configNodes.top()->Prolog();

            if (sessionDetails.find(Property::ROUTER_ID) != sessionDetails.end()) {
                auto routerIdStmt = RenderRouterId(sessionsIt->at(sessionName), indent + DEFAULT_INDENT);
                if (routerIdStmt.has_value()) {
                    birdBgpFullConfig << routerIdStmt.value();
                }
            }

            auto propertyIt = sessionDetails.find(Property::PEER);
            if (propertyIt == sessionDetails.end()) {
                mLog->error("Not found key '{}' in JSON data", Property::PEER);
                return {};
            }
            else {
                birdBgpFullConfig << RenderBgpPeerAddrAsnPort(*bgpIt, propertyIt.value(), indent + DEFAULT_INDENT);
            }

            propertyIt = sessionDetails.find(Property::LOCAL);
            if (propertyIt == sessionDetails.end()) {
                mLog->error("Not found key '{}' in JSON data", Property::LOCAL);
                return {};
            }
            else {
                birdBgpFullConfig << RenderBgpLocalAddrAsnPort(*bgpIt, propertyIt.value(), indent + DEFAULT_INDENT);
            }

            propertyIt = sessionDetails.find(Property::ADDRESS_FAMILY);
            if (propertyIt == sessionDetails.end()) {
                mLog->error("Not found key '{}' in JSON data", Property::ADDRESS_FAMILY);
                return {};
            }
            else {
                birdBgpConfigPart = RenderBgpSessionAddrFamily(*bgpIt, propertyIt.value(), indent + DEFAULT_INDENT);
                if (!birdBgpConfigPart.has_value()) {
                    mLog->error("Failed to parse '{}' section", Property::ADDRESS_FAMILY);
                    return {};
                }

                birdBgpFullConfig << birdBgpConfigPart.value();
            }

            // This is optional statement
            if (sessionDetails.find(Property::EBGP) != sessionDetails.end()) {
                birdBgpConfigPart = RenderBgpMultihopStatement(*bgpIt, sessionDetails[Property::EBGP], indent + DEFAULT_INDENT);
                if (!birdBgpConfigPart.has_value()) {
                    mLog->error("Failed to parse '{}' section", Property::EBGP);
                    return {};
                }

                birdBgpFullConfig << birdBgpConfigPart.value();
            }

            // This is optional statement
            if (sessionDetails.find(Property::IBGP) != sessionDetails.end()) {
                birdBgpConfigPart = RenderBgpNextHopSelfStatement(*bgpIt, sessionDetails[Property::IBGP], indent + DEFAULT_INDENT);
                if (!birdBgpConfigPart.has_value()) {
                    mLog->error("Failed to parse '{}' section", Property::IBGP);
                    return {};
                }

                birdBgpFullConfig << birdBgpConfigPart.value();
            }

            birdBgpFullConfig << configNodes.top()->Epilog();
        }

        return birdBgpFullConfig.str();
    }

    Optional<String> RenderDeviceProtocol(const Json::JSON& jConfig) {
        const size_t indent = 0;
        OStrStream deviceProtocolSection;
        deviceProtocolSection << "protocol device {" << NEW_LINE;
        deviceProtocolSection << String(DEFAULT_INDENT, ' ') << "scan time 10;" << NEW_LINE;
        deviceProtocolSection << String(DEFAULT_INDENT, ' ') << "interface \"*\";" << NEW_LINE;
        deviceProtocolSection << "}" << NEW_LINE;
        return deviceProtocolSection.str();
    }

    Optional<String> RenderDirectProtocol(const Json::JSON& jConfig) {
        const size_t indent = 0;
        OStrStream deviceProtocolSection;
        deviceProtocolSection << "protocol direct {" << NEW_LINE;
        deviceProtocolSection << String(DEFAULT_INDENT, ' ') << "ipv4;" << NEW_LINE;
        deviceProtocolSection << String(DEFAULT_INDENT, ' ') << "ipv6;" << NEW_LINE;
        deviceProtocolSection << String(DEFAULT_INDENT, ' ') << "interface \"*\";" << NEW_LINE;
        deviceProtocolSection << "}" << NEW_LINE;
        return deviceProtocolSection.str();
    }

    Optional<String> RenderKernelProtocol(const Json::JSON& jConfig) {
        const size_t indent = 0;
        OStrStream kernelProtocolSection;
        kernelProtocolSection << "protocol kernel 'PROTO_KERNEL_IPv4' {" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "scan time 5;" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "ipv4 {" << NEW_LINE;
        kernelProtocolSection << String(2 * DEFAULT_INDENT, ' ') << "export all;" << NEW_LINE;
        // kernelProtocolSection << String(2 * DEFAULT_INDENT, ' ') << "import all;" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "};" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "merge paths on limit 128;" << NEW_LINE;
        kernelProtocolSection << "}" << NEW_LINE;

        kernelProtocolSection << "protocol kernel 'PROTO_KERNEL_IPv6' {" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "ipv6 {" << NEW_LINE;
        kernelProtocolSection << String(2 * DEFAULT_INDENT, ' ') << "export all;" << NEW_LINE;
        // kernelProtocolSection << String(2 * DEFAULT_INDENT, ' ') << "import all;" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "};" << NEW_LINE;
        kernelProtocolSection << String(DEFAULT_INDENT, ' ') << "merge paths on limit 128;" << NEW_LINE;
        kernelProtocolSection << "}" << NEW_LINE;

        return kernelProtocolSection.str();
    }

    /** RenderBgpPolicyIfStatement expects JSON data inside of "if-match" property/node */
    Optional<String> RenderBgpPolicyIfStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream ifStmtBody;
        auto ifMatchStmtIt = jConfigParent.find(Property::IF_MATCH);
        if (ifMatchStmtIt == jConfigParent.end()) {
            mLog->error("Not found key '{}' in JSON data", Property::IF_MATCH);
            return {};
        }

        IfMatchType ifMatchType = IfMatchType::ALL;
        auto ifMatchTypeIt = ifMatchStmtIt->find(Property::MATCH_TYPE);
        if (ifMatchTypeIt != ifMatchStmtIt->end()) {
            if (ifMatchTypeIt.value().template get<String>() == "ANY") {
                ifMatchType = IfMatchType::ANY;
            }
        }

        ifStmtBody << String(indentSize, ' ') << "if (";
        // Let's remember if there is already any rendered condition check
        bool isFirstCondInStmt = false;
        Optional<String> checkStmt;
        checkStmt = RenderAsPathEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::AS_PATH_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            ifStmtBody << checkStmt.value();
            isFirstCondInStmt = true;
        }

        checkStmt = RenderAsPathInCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::AS_PATH_IN);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpCommunityEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::COMMUNITY_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpCommunityInCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::COMMUNITY_IN);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpExtCommunityEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::EXT_COMMUNITY_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpExtCommunityInCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::EXT_COMMUNITY_IN);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }
        
        checkStmt = RenderBgpNetEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::NET_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpNetInCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::NET_IN);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderBgpNetTypeEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::NET_TYPE_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        checkStmt = RenderSourceProtocolEqCheckStatement(jConfigBgpRoot, *ifMatchStmtIt, indentSize);
        if (!checkStmt.has_value()) {
            mLog->error("Failed to render '{}' check statement", Property::SOURCE_PROTOCOL_EQ);
            return {};
        }

        if (!checkStmt.value().empty()) {
            if (isFirstCondInStmt) {
                ifStmtBody << ((ifMatchType == IfMatchType::ALL) ? " && " : " || ");
            }
            else {
                isFirstCondInStmt = true;
            }

            ifStmtBody << checkStmt.value();
        }

        if (!isFirstCondInStmt) {
            mLog->error("Invalid '{}' statement body because it is empty", Property::IF_MATCH);
            // FIXME: When all condition statements will be implemented, please return empty object {}
            return "";
        }

        auto thenStmtIt = jConfigParent.find(Property::THEN);
        if (thenStmtIt == jConfigParent.end()) {
            mLog->error("Not found key '{}' in JSON data", Property::THEN);
            return {};
        }

        ifStmtBody << ") then {" << NEW_LINE;
        Optional<String> actionStmt;
        actionStmt = RenderBgpAsPathPrependStmt(jConfigBgpRoot, *thenStmtIt, indentSize + DEFAULT_INDENT);
        if (!actionStmt.has_value()) {
            mLog->error("Failed to render BGP '{}' statement", Property::AS_PATH_PREPEND);
            return {};
        }
        else if (!actionStmt.value().empty()) {
             ifStmtBody << actionStmt.value() << NEW_LINE;
        }

        actionStmt = RenderBgpCommunityAddStmt(jConfigBgpRoot, *thenStmtIt, indentSize + DEFAULT_INDENT);
        if (!actionStmt.has_value()) {
            mLog->error("Failed to render BGP '{}' action statement", Property::COMMUNITY_ADD);
            return {};
        }
        else if (!actionStmt.value().empty()) {
             ifStmtBody << actionStmt.value() << NEW_LINE;
        }

        actionStmt = RenderBgpCommunityRemoveStmt(jConfigBgpRoot, *thenStmtIt, indentSize + DEFAULT_INDENT);
        if (!actionStmt.has_value()) {
            mLog->error("Failed to render BGP '{}' action statement", Property::COMMUNITY_REMOVE);
            return {};
        }
        else if (!actionStmt.value().empty()) {
             ifStmtBody << actionStmt.value() << NEW_LINE;
        }

        actionStmt = RenderBgpLocalPreferenceSetStmt(jConfigBgpRoot, *thenStmtIt, indentSize + DEFAULT_INDENT);
        if (!actionStmt.has_value()) {
            mLog->error("Failed to render BGP '{}' statement", Property::LOCAL_PREFERENCE_SET);
            return {};
        }
        else if (!actionStmt.value().empty()) {
             ifStmtBody << actionStmt.value() << NEW_LINE;
        }

        actionStmt = RenderBgpMedSetStmt(jConfigBgpRoot, *thenStmtIt, indentSize + DEFAULT_INDENT);
        if (!actionStmt.has_value()) {
            mLog->error("Failed to render BGP '{}' action statement", Property::MED_SET);
            return {};
        }
        else if (!actionStmt.value().empty()) {
             ifStmtBody << actionStmt.value() << NEW_LINE;
        }

        auto actionStmtIt = thenStmtIt->find(Property::ACTION);
        if (actionStmtIt == thenStmtIt->end()) {
            mLog->error("Not found key '{}' in JSON data", Property::ACTION);
            return {};
        }

        auto action = actionStmtIt.value().template get<String>();
        if (action == "deny") {
            ifStmtBody << String(indentSize + DEFAULT_INDENT, ' ') << "reject;" << NEW_LINE;
        }
        else if (action == "permit") {
            ifStmtBody << String(indentSize + DEFAULT_INDENT, ' ') << "accept;" << NEW_LINE;
        }

        // TODO: Put other action statements
        ifStmtBody << String(indentSize, ' ') << "}" << NEW_LINE;

        return ifStmtBody.str();
    }

    /** RenderBgpPeerASN expects JSON data inside of "peer" property/node */
    String RenderBgpPeerAddrAsnPort(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream peerAttrs;
        bool isDirectlyConnected;
        if (jConfigParent.find(Property::ADDRESS) != jConfigParent.end()) {
            auto addrIt = jConfigParent.find(Property::ADDRESS);
            auto rangeIt = addrIt->find(Property::RANGE);
            if (rangeIt != addrIt->end()) {
                peerAttrs << " range " << rangeIt.value().template get<String>();
            }
            else {
                auto addr = addrIt.value().template get<String>();
                // If a peer is directly connected then it is defined out of 'neighbor' option
                if (addr == "direct") {
                    isDirectlyConnected = true;
                }
                else {
                    peerAttrs << " " << addr;
                }
            }
        }
        else if (jConfigParent.find(Property::LINK_LOCAL) != jConfigParent.end()) {
            auto linkLocalIt = jConfigParent.find(Property::LINK_LOCAL);
            auto addrIt = linkLocalIt->find(Property::ADDRESS);
            if (addrIt == linkLocalIt->end()) {
                mLog->error("Not found key '{}' at property '{}'", Property::ADDRESS, Property::LINK_LOCAL);
                return {};
            }

            peerAttrs << " " << addrIt.value().template get<String>();
            auto ifaceIt = linkLocalIt->find(Property::INTERFACE);
            if (ifaceIt != linkLocalIt->end()) {
                peerAttrs << "%" << ifaceIt.value().template get<String>();
            }
        }

        auto portIt = jConfigParent.find(Property::PORT);
        if (portIt != jConfigParent.end()) {
            peerAttrs << " port " << std::to_string(portIt.value().template get<std::uint16_t>());
        }

        auto asnIt = jConfigParent.find(Property::AS);
        if (asnIt != jConfigParent.end()) {
            if (asnIt.value().is_string()) {
                peerAttrs << " " << asnIt.value().template get<String>(); // renders "external" or "internal"
            }
            else {
                peerAttrs << " as " << std::to_string(asnIt.value().template get<std::uint32_t>());
            }
        }

        auto output = peerAttrs.str();
        if (output.size() == 0) {
            return "";
        }

        output = String(indentSize, ' ') + "neighbor" + output + ";\n";
        if (isDirectlyConnected) {
            output += String(indentSize, ' ') + "direct;\n";
        }

        return output;
    }

    /** RenderBgpPeerASN expects JSON data inside of "local" property/node */
    String RenderBgpLocalAddrAsnPort(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream peerAttrs;
        bool isDirectlyConnected;
        if (jConfigParent.find(Property::ADDRESS) != jConfigParent.end()) {
            peerAttrs << " " << jConfigParent[Property::ADDRESS].template get<String>();
        }
        else if (jConfigParent.find(Property::LINK_LOCAL) != jConfigParent.end()) {
            auto linkLocalIt = jConfigParent.find(Property::LINK_LOCAL);
            if (linkLocalIt->find(Property::ADDRESS) != linkLocalIt->end()) {
                peerAttrs << " " << (*linkLocalIt)[Property::ADDRESS].template get<String>();
            }

            if (linkLocalIt->find(Property::INTERFACE) != linkLocalIt->end()) {
                peerAttrs << "%" << (*linkLocalIt)[Property::INTERFACE].template get<String>();
            }
        }

        auto portIt = jConfigParent.find(Property::PORT);
        if (portIt != jConfigParent.end()) {
            peerAttrs << " port " << std::to_string(portIt.value().template get<std::uint16_t>());
        }

        auto asnIt = jConfigParent.find(Property::AS);
        if (asnIt != jConfigParent.end()) {
            peerAttrs << " as " << std::to_string(asnIt.value().template get<std::uint32_t>());
        }

        auto output = peerAttrs.str();
        if (output.size() != 0) {
            output = String(indentSize, ' ') + "local" + output + ";\n";
        }

        return output;
    }

    /** RenderBgpMultihopStatement expects JSON data inside of "ebgp" property/node */
    String RenderBgpMultihopStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream multihopStmt;
        auto multihopIt = jConfigParent.find(Property::MULTIHOP);
        if (multihopIt == jConfigParent.end()) {
            return "";
        }
        
        multihopStmt << String(indentSize, ' ') << "multihop";
        if (multihopIt->find(Property::TTL) != multihopIt->end()) {
            multihopStmt << " " << multihopIt->at(Property::TTL).template get<uint16_t>();
        }

        multihopStmt << ";" << NEW_LINE;
        return multihopStmt.str();
    }

    /** RenderBgpNextHopSelfStatement expects JSON data inside of "ibgp" property/node */
    String RenderBgpNextHopSelfStatement(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream nextHopSelfStmt;
        auto multihopIt = jConfigParent.find(Property::MULTIHOP);
        if (multihopIt == jConfigParent.end()) {
            return "";
        }
        
        nextHopSelfStmt << String(indentSize, ' ') << "multihop";
        if (multihopIt->find(Property::TTL) != multihopIt->end()) {
            nextHopSelfStmt << " " << multihopIt->at(Property::TTL).template get<uint16_t>();
        }

        nextHopSelfStmt << ";" << NEW_LINE;
        return nextHopSelfStmt.str();
    }

    Optional<String> RenderBgpSessionAddrFamily(const Json::JSON& jConfigBgpRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream addrFamilyStmtOutput;
        auto addrFamilyIPv4It = jConfigParent.find(Property::IPV4);
        if (addrFamilyIPv4It != jConfigParent.end()) {
            addrFamilyStmtOutput << String(indentSize, ' ') << "ipv4 {" << NEW_LINE;
            auto nextHopSelfStmt = RenderNextHopSelfStatement(jConfigBgpRoot, *addrFamilyIPv4It, indentSize + DEFAULT_INDENT);
            if (!nextHopSelfStmt.has_value()) {
                mLog->error("Failed to parse '{}' statement", Property::NEXT_HOP_SELF);
                return {};
            }

            if (!nextHopSelfStmt.value().empty()) {
                addrFamilyStmtOutput << String(indentSize + DEFAULT_INDENT, ' ') << nextHopSelfStmt.value() << NEW_LINE;
            }

            if (addrFamilyIPv4It->contains(Property::POLICY_IN)
                || addrFamilyIPv4It->contains(Property::POLICY_OUT)) {
                auto policyStmtOutput = RenderApplyBgpPolicy(jConfigBgpRoot, *addrFamilyIPv4It, indentSize + DEFAULT_INDENT);
                if (!policyStmtOutput.has_value()) {
                    mLog->error("Failed to parse policies for '{}' section", Property::ADDRESS_FAMILY);
                    return {};
                }

                
                addrFamilyStmtOutput << policyStmtOutput.value();
            }

            addrFamilyStmtOutput << String(indentSize, ' ') << "};" << NEW_LINE;
        }

        auto addrFamilyIPv6It = jConfigParent.find(Property::IPV6);
        if (addrFamilyIPv6It != jConfigParent.end()) {
            addrFamilyStmtOutput << String(indentSize, ' ') << "ipv6 {" << NEW_LINE;
            auto nextHopSelfStmt = RenderNextHopSelfStatement(jConfigBgpRoot, *addrFamilyIPv6It, indentSize + DEFAULT_INDENT);
            if (!nextHopSelfStmt.has_value()) {
                mLog->error("Failed to parse '{}' statement", Property::NEXT_HOP_SELF);
                return {};
            }

            if (!nextHopSelfStmt.value().empty()) {
                addrFamilyStmtOutput << String(indentSize + DEFAULT_INDENT, ' ') << nextHopSelfStmt.value() << NEW_LINE;
            }

            if (addrFamilyIPv6It->contains(Property::POLICY_IN)
                || addrFamilyIPv6It->contains(Property::POLICY_OUT)) {
                auto policyStmtOutput = RenderApplyBgpPolicy(jConfigBgpRoot, *addrFamilyIPv6It, indentSize + DEFAULT_INDENT);
                if (!policyStmtOutput.has_value()) {
                    mLog->error("Failed to parse policies for '' section", Property::ADDRESS_FAMILY);
                    return {};
                }

                addrFamilyStmtOutput << policyStmtOutput.value();
            }

            addrFamilyStmtOutput << String(indentSize, ' ') << "};" << NEW_LINE;
        }

        auto addrFamilySectionOutput = addrFamilyStmtOutput.str();
        if (addrFamilySectionOutput.empty()) {
            mLog->error("Failed to parse any attribute of mandatory property '{}'", Property::ADDRESS_FAMILY);
            return {};
        }

        return addrFamilySectionOutput;
    }

    Optional<String> RenderStaticProtocol(const Json::JSON& jConfig, Stack<UniquePtr<ConfigNodeRendering>>& configNodes) {
        const size_t indent = 0;
        auto staticIt = jConfig.find(Property::STATIC);
        if (staticIt == jConfig.end()) {
            return "";
        }

        auto routeIt = staticIt->find(Property::ROUTE);
        if (routeIt == staticIt->end()) {
            return "";
        }

        OStrStream birdStaticFullConfig;
        Optional<String> birdStaticConfigPart;

        if (routeIt->find(Property::IPV4) != routeIt->end()) {
            birdStaticFullConfig << "protocol static 'STATIC_IPv4' {" << NEW_LINE;
            birdStaticConfigPart = RenderStaticIpRouteSectionBody(Property::IPV4, routeIt->at(Property::IPV4), indent + DEFAULT_INDENT);
            if (!birdStaticConfigPart.has_value()) {
                mLog->error("Failed to render static IPv4 route section");
                return {};
            }

            birdStaticFullConfig << birdStaticConfigPart.value();
            birdStaticFullConfig << "}" << NEW_LINE;
        }

        if (routeIt->find(Property::IPV6) != routeIt->end()) {
            birdStaticFullConfig << "protocol static 'STATIC_IPv6' {" << NEW_LINE;
            birdStaticConfigPart = RenderStaticIpRouteSectionBody(Property::IPV6, routeIt->at(Property::IPV6), indent + DEFAULT_INDENT);
            if (!birdStaticConfigPart.has_value()) {
                mLog->error("Failed to render static IPv6 route section");
                return {};
            }

            birdStaticFullConfig << birdStaticConfigPart.value();
            birdStaticFullConfig << "}" << NEW_LINE;
        }

        return birdStaticFullConfig.str();
    }

    Optional<String> RenderStaticIpRouteSectionBody(const String& ipChannel, const Json::JSON& jConfigIpRouteList, const size_t indentSize) {
        OStrStream sectionBody;
        sectionBody << String(indentSize, ' ') << ipChannel << ";" << NEW_LINE;
        auto routeList = RenderStaticRouteStatement(jConfigIpRouteList, indentSize);
        if (!routeList.has_value()) {
            mLog->error("Failed to render list of {} routes", ipChannel);
            return {};
        }

        sectionBody << routeList.value();
        return sectionBody.str();
    }

    Optional<String> RenderStaticRouteStatement(const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream routeStmt;
        for (const auto& [prefix, attrs] : jConfigParent.items()) {
            routeStmt << String(indentSize, ' ') << "route " << prefix;
            if (attrs.find(Property::NEXT_HOP) != attrs.end()) {
                auto nexthopStmt = RenderStaticRouteNexthopStatement(attrs[Property::NEXT_HOP], indentSize);
                if (!nexthopStmt.has_value()) {
                    mLog->error("Failed to render nexthop of prefix '{}'", prefix);
                    return {};
                }

                routeStmt << nexthopStmt.value() << ";" << NEW_LINE;
            }
            else if (attrs.find(Property::IFNAME) != attrs.end()) {
                routeStmt << " via \"" << attrs[Property::IFNAME].template get<String>() << "\";" << NEW_LINE;
            }
            else {
                mLog->error("There is missing static route '{}' attributes", prefix);
                return {};
            }
        }

        return routeStmt.str();
    }

    Optional<String> RenderStaticRouteNexthopStatement(const Json::JSON& jConfigNexthop, const size_t indentSize) {
        OStrStream nexthopStmt;
        if (jConfigNexthop.is_string()) {
            // The route is "blackholed" or "unrechabled"
            nexthopStmt << " " << jConfigNexthop.template get<String>();
            return nexthopStmt.str();
        }

        for (const auto& [nexthop, attrs] : jConfigNexthop.items()) {
            if (!nexthopStmt.str().empty()) {
                nexthopStmt << NEW_LINE << String(indentSize + DEFAULT_INDENT, ' ');
            }

            nexthopStmt << " via " << nexthop;
            if (attrs.find(Property::IFNAME) != attrs.end()) {
                nexthopStmt << " dev \"" << attrs[Property::IFNAME].template get<String>() << "\"";
            }

            if (attrs.find(Property::ONLINK) != attrs.end()) {
                if (attrs[Property::ONLINK].template get<bool>()) {
                    nexthopStmt << " " << "onlink";
                }
            }
        }

        if (nexthopStmt.str().empty()) {
            mLog->error("There is missing nexthop");
            return {};
        }

        return nexthopStmt.str();
    }

}; // class BirdConfigConverter
} // namespace Config
