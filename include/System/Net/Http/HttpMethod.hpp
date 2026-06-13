// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Net::Http {

/// Represents an HTTP method (verb), mirroring .NET System.Net.Http.HttpMethod.
class HttpMethod {
    std::string method_;
public:
    /// @brief Constructs an HttpMethod with the given verb string.
    /// @param method HTTP verb string (e.g. "GET", "POST").
    explicit HttpMethod(const std::string& method) : method_(method) {}

    /// Returns the HTTP method string (e.g. "GET").
    [[nodiscard]] const std::string& getMethodProperty() const { return method_; }
    /// Returns the HTTP method string.
    [[nodiscard]] std::string ToString() const { return method_; }

    /// Returns true if both HttpMethod instances represent the same verb.
    bool operator==(const HttpMethod& o) const { return method_ == o.method_; }
    /// Returns true if the two HttpMethod instances represent different verbs.
    bool operator!=(const HttpMethod& o) const { return method_ != o.method_; }

    static const HttpMethod& Get()     { static HttpMethod m("GET");     return m; } ///< The HTTP GET method.
    static const HttpMethod& Post()    { static HttpMethod m("POST");    return m; } ///< The HTTP POST method.
    static const HttpMethod& Put()     { static HttpMethod m("PUT");     return m; } ///< The HTTP PUT method.
    static const HttpMethod& Delete_() { static HttpMethod m("DELETE");  return m; } ///< The HTTP DELETE method.
    static const HttpMethod& Head()    { static HttpMethod m("HEAD");    return m; } ///< The HTTP HEAD method.
    static const HttpMethod& Options() { static HttpMethod m("OPTIONS"); return m; } ///< The HTTP OPTIONS method.
    static const HttpMethod& Patch()   { static HttpMethod m("PATCH");   return m; } ///< The HTTP PATCH method.
};

} // namespace System::Net::Http
