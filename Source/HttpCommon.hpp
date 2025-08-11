/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
#pragma once

namespace HTTP {
enum class Method {
    GET,
    PATCH,
    PUT,
    POST,
    DEL
};

enum StatusCode {
    // For internal use
    INTERNAL_SUCCESS = 0,
    // Informational responses
    CONTINUE = 100,
    // Successful responses
    START_SUCCESS = 200,
    OK = START_SUCCESS,
    CREATED = 201,
    END_SUCCESS = 299,
    // Redirection messages
    SEE_OTHER = 303,
    // Client error responses
    CONFLICT = 409,
    INVALID_TOKEN = 498,
    TOKEN_REQUIRED = 499,
    // Server error responses
    INTERNAL_SERVER_ERROR = 500,
};

static constexpr inline bool IsSuccess(const StatusCode status_code) { return (status_code >= StatusCode::START_SUCCESS) && (status_code <= StatusCode::END_SUCCESS); }

namespace ContentType {
    static constexpr auto TEXT_PLAIN_RESP_CONTENT = "text/plain";
} // namespace ContentType
namespace Header {
namespace Tokens {
    static constexpr auto AUTHORIZATION = "Authorization";
    static constexpr auto BEARER = "Bearer";
} // namespace Tokens
} // namespace Header
} // namespace HTTP
