/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

#include "Lib/StdLib.hpp"
#include <httplib/httplib.h>

namespace Http = httplib;
namespace Std = StdLib;

using SessionTimeoutCB = std::function<void(const Std::String session_token)>;
using SessionTokenTimerCB = std::function<void(const Std::String session_token)>;

/*
    Manages client session and transaction.
    Client start a session and SessionManager start counter. If client won't confirm its changes in 
    5 minutes then his changes are withdrawing.
*/
class SessionManager {
public:
    SessionManager(const std::chrono::seconds session_timeout_sec);
    ~SessionManager();

    bool RegisterSessionToken(const Http::Request &req, Http::Response &res);
    bool CheckSessionToken(const Http::Request &req, Http::Response &res);
    bool SetActiveSessionToken(const Http::Request &req, Http::Response &res);
    bool CheckActiveSessionToken(const Http::Request &req, Http::Response &res);
    bool RemoveSessionToken(const Http::Request &req, Http::Response &res);
    bool RemoveSessionToken(const Std::String &session_token);
    bool RemoveActiveSessionToken(const Std::String &session_token);

    Std::Optional<Std::String> GetSessionToken(const Http::Request &req);
    Std::Optional<Std::String> GetActiveSessionToken();

    bool RegisterSessionTimeoutCallback(const Std::String& callback_receiver, SessionTimeoutCB session_timeout_cb);
    void RemoveSessionTimeoutCallback(const Std::String& callback_receiver);

    // 30 sec resolution 
    bool SetSessionTokenTimerOnce(const Http::Request &req, SessionTokenTimerCB timer_callback, const std::chrono::seconds timeout_sec);
    bool CancelSessionTokenTimerOnce(const Http::Request &req);

private:
    struct SessionDetails {
        std::chrono::time_point<std::chrono::system_clock> LastRequestAt;
        std::chrono::time_point<std::chrono::system_clock> StartAt;
    };

    Std::Map<Std::String, SessionDetails> _leased_session_tokens;
    Std::Optional<Std::String> _active_session_token;
    Std::Mutex _session_token_mutex;
    Std::Thread _checking_session_expiration_thread;
    Std::Map<Std::String, SessionTimeoutCB> _session_timeout_callbacks;
    Std::Mutex _session_timeout_callbacks_mutex;
    std::chrono::seconds _session_timeout_sec;
    std::atomic_bool _checking_session_expiration_quit_flag = false;
    struct TimerThreadDetails {
        SessionTokenTimerCB TimerCB;
        std::chrono::time_point<std::chrono::system_clock> StartAt;
        std::chrono::seconds Timeout;
        bool QuitFlag; // TODO: Since the structure is protected by mutex, is required atomic operation here?
    };
    Std::Map<Std::String, TimerThreadDetails> _session_token_timers;
    Std::Mutex _session_token_timers_mutex;
};
