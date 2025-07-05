#pragma once

#include "IConfigConverting.hpp"

#include "JsonCommon.hpp"
#include "Lib/ModuleRegistry.hpp"
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
using namespace StdLib;
using namespace BirdConfigTree;
class BirdConfigConverter : public IConfigConverting {
public:
    BirdConfigConverter(const SharedPtr<ModuleRegistry>& moduleRegistry)
      : mModuleRegistry(moduleRegistry), mLog(moduleRegistry->LoggerRegistry()->Logger(Module::Name::CONFIG_TRANSL)) {}
    virtual ~BirdConfigConverter() = default;
    Optional<ByteStream> Convert(const ByteStream& config) override {
        Json::JSON jConfig;
        try {
            Stack<UniquePtr<ConfigNodeRendering>> configNodes;
            jConfig = Json::JSON::parse(config);

            auto birdConfig = std::make_shared<OStrStream>();
            Optional<String> birdConfigPart;
            birdConfigPart = RenderGlobalRouterInfo(jConfig);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render global info about local router");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            birdConfigPart = RenderBgpProtocol(jConfig, configNodes);
            if (!birdConfigPart.has_value()) {
                mLog->error("Failed to render bgp protocol");
                return {};
            }

            *birdConfig << birdConfigPart.value();

            while (!configNodes.empty()) {
                *birdConfig << configNodes.top()->Epilog();
                configNodes.pop();
            }

            auto birdConfigStr = birdConfig->str();
            mLog->trace("Converted JSON config into BIRD config:\n{}", birdConfigStr);
            return ByteStream(std::begin(birdConfigStr), std::end(birdConfigStr));
        }
        catch (const Exception &ex) {
            mLog->error("Failed to convert JSON data into BIRD config. Error: {}", ex.what());
            return {};
        }

        return {};
    }

private:
    SharedPtr<ModuleRegistry> mModuleRegistry;
    SharedPtr<Log::SpdLogger> mLog;

    static constexpr size_t DEFAULT_INDENT = 4;
    Optional<String> RenderGlobalRouterInfo(const Json::JSON& jConfig) {
        auto routerIdIt = jConfig.find("router-id");
        if (routerIdIt == jConfig.end()) {
            mLog->trace("Not found key 'router-id' in JSON data");
            return "";
        }

        return String("router id ") + routerIdIt.value().template get<std::string>() + ";\n";
    }

    // FIXME: Pass output sink
    Optional<String> RenderBgpProtocol(const Json::JSON& jConfig, Stack<UniquePtr<ConfigNodeRendering>>& configNodes) {
        const size_t indent = 0;
        auto bgpIt = jConfig.find("bgp");
        if (bgpIt == jConfig.end()) {
            mLog->trace("Not found key 'bgp' in JSON data");
            return "";
        }

        auto sessionsIt = bgpIt->find("sessions");
        // auto sessions = jConfig.at(Json::JSON::json_pointer("/bgp/sessions").to_string());
        if (sessionsIt == bgpIt->end()) {
            mLog->trace("Not found key 'sessions' in JSON data");
            return "";
        }

        String output;
        // return sessionsIt->dump();
        for (auto& [sessionName, sessionDetails] : sessionsIt->items()) {
            configNodes.emplace(std::make_unique<ProtocolBgp>(sessionName));
            output += configNodes.top()->Prolog();
            auto peerIt = sessionDetails.find("peer");
            if (peerIt == sessionDetails.end()) {
                mLog->trace("Not found key 'peer' in JSON data");
            }
            else {
                // if (peerIt->is_object()) {
                // output += RenderBgpPeerAsn(peerIt.value(), 4);
                output += RenderBgpPeerAddrAsnPort(jConfig, peerIt.value(), indent + DEFAULT_INDENT);
            }
            
            output += configNodes.top()->Epilog();
        }

        return output;
    }

    /** RenderBgpPeerASN expects JSON data inside of "peer" property/node */
    String RenderBgpPeerAddrAsnPort(const Json::JSON& jConfigRoot, const Json::JSON& jConfigParent, const size_t indentSize) {
        OStrStream peerAttrs;
        bool isDirectlyConnected;
        auto addrIt = jConfigParent.find("address");
        if (addrIt != jConfigParent.end()) {
            auto rangeIt = addrIt->find("range");
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
        else {
            mLog->trace("Not found key 'address' in JSON data");
        }

        auto portIt = jConfigParent.find("port");
        if (portIt != jConfigParent.end()) {
            peerAttrs << " port " << std::to_string(portIt.value().template get<std::uint16_t>());
        }
        else {
            mLog->trace("Not found key 'port' in JSON data");
        }

        auto asnIt = jConfigParent.find("as");
        if (asnIt != jConfigParent.end()) {
            if (asnIt.value().is_string()) {
                peerAttrs << " " << asnIt.value().template get<String>();
            }
            else {
                peerAttrs << " as " << std::to_string(asnIt.value().template get<std::uint32_t>());
            }
        }
        else {
            mLog->trace("Not found key 'asn' in JSON data");
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

}; // class BirdConfigConverter
} // namespace Config
