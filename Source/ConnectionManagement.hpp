/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "HttpCommon.hpp"
#include "Lib/ModuleRegistry.hpp"
#include "Lib/StdLib.hpp"
#include "Modules.hpp"
#include "SessionManagement.hpp"

namespace ConnectionManagement {
namespace URIRequestPath {
namespace Config {
    static constexpr auto CANDIDATE = "/config/candidate";
    static constexpr auto RUNNING = "/config/running";
    static constexpr auto RUNNING_UPDATE = "/config/running/update";
    static constexpr auto RUNNING_DIFF = "/config/running/diff";
} // namespace Config

namespace Log {
    static constexpr auto LAST_REQUEST = "/log/last";
}

namespace Session {
    static constexpr auto TOKEN = "/session/token";
} // namespace Session
}

namespace Std = StdLib;

class Client {
public:
    static bool post(const Std::String& host_addr, const Std::String& path, const Std::String& body);
};

using RequestCallback = std::function<HTTP::StatusCode(const Std::String& path, Std::String data_request, Std::String& return_data)>;

class Server {
public:
    Server(Std::SharedPtr<ModuleRegistry>& module_registry);

    bool addOnDeleteConnectionHandler(const Std::String& id, RequestCallback handler);
    bool removeOnDeleteConnectionHandler(const Std::String& id);
    bool addOnGetConnectionHandler(const Std::String& id, RequestCallback handler);
    bool removeOnGetConnectionHandler(const Std::String& id);
    bool addOnPostConnectionHandler(const Std::String& id, RequestCallback handler);
    bool removeOnPostConnectionHandler(const Std::String& id);
    bool addOnPutConnectionHandler(const Std::String& id, RequestCallback handler);
    bool removeOnPutConnectionHandler(const Std::String& id);
    bool Run(const Std::String& host, const uint16_t port);

private:
    HTTP::StatusCode processRequest(const HTTP::Method method, const Std::String& path, const Std::String& request_data, Std::String& return_data);
    bool addConnectionHandler(Std::Map<Std::String, RequestCallback>& callbacks, const Std::String& id, RequestCallback handler);
    bool removeConnectionHandler(Std::Map<Std::String, RequestCallback>& callbacks, const Std::String& id);
    Std::Map<Std::String, RequestCallback> _on_delete_callback_by_id;
    Std::Map<Std::String, RequestCallback> _on_get_callback_by_id;
    Std::Map<Std::String, RequestCallback> _on_post_callback_by_id;
    Std::Map<Std::String, RequestCallback> _on_put_callback_by_id;
    SessionManager _session_mngr;
    const Std::SharedPtr<ModuleRegistry> _module_registry;
    Std::SharedPtr<Log::SpdLogger> _log;
    Std::String _callback_register_id = "HttpServer";
};
} // ConnectionManagement
