// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net::Http {

/// Abstract base class for HTTP request/response body content.
class HttpContent {
public:
    virtual ~HttpContent() = default;

    /// Returns the body as a UTF-8 string.
    [[nodiscard]] virtual std::string ReadAsString() const = 0;

    /// Returns the body as raw bytes.
    [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> ReadAsByteArray() const = 0;

    /// Returns the MIME media type (e.g. "text/plain", "application/json").
    [[nodiscard]] virtual std::string getContentType() const = 0;

    /// Returns the charset (e.g. "utf-8"), or empty if not applicable.
    [[nodiscard]] virtual std::string getCharSet() const { return ""; }
};

} // namespace System::Net::Http
