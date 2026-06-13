// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Net::Http {

/// Represents an HTTP method (verb).
class HttpMethod {
    std::string method_;
public:
    explicit HttpMethod(const std::string& method) : method_(method) {}

    [[nodiscard]] const std::string& getMethodProperty() const { return method_; }
    [[nodiscard]] std::string ToString() const { return method_; }

    bool operator==(const HttpMethod& o) const { return method_ == o.method_; }
    bool operator!=(const HttpMethod& o) const { return method_ != o.method_; }

    static const HttpMethod& Get()     { static HttpMethod m("GET");     return m; }
    static const HttpMethod& Post()    { static HttpMethod m("POST");    return m; }
    static const HttpMethod& Put()     { static HttpMethod m("PUT");     return m; }
    static const HttpMethod& Delete_() { static HttpMethod m("DELETE");  return m; }
    static const HttpMethod& Head()    { static HttpMethod m("HEAD");    return m; }
    static const HttpMethod& Options() { static HttpMethod m("OPTIONS"); return m; }
    static const HttpMethod& Patch()   { static HttpMethod m("PATCH");   return m; }
};

} // namespace System::Net::Http
