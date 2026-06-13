// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include <string>
#include <vector>

namespace System::Net::Http {

/// HTTP content backed by a plain text string.
class StringContent : public HttpContent {
    std::string content_;
    std::string mediaType_;
    std::string charset_;
public:
    explicit StringContent(const std::string& content,
                           const std::string& charset   = "utf-8",
                           const std::string& mediaType = "text/plain")
        : content_(content), mediaType_(mediaType), charset_(charset) {}

    [[nodiscard]] std::string ReadAsString() const override { return content_; }

    [[nodiscard]] std::vector<SharpRuntime::bytecs> ReadAsByteArray() const override {
        return std::vector<SharpRuntime::bytecs>(content_.begin(), content_.end());
    }

    [[nodiscard]] std::string getContentType() const override { return mediaType_; }
    [[nodiscard]] std::string getCharSet()     const override { return charset_; }
};

} // namespace System::Net::Http
