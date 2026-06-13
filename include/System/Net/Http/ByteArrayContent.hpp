// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include <string>
#include <vector>

namespace System::Net::Http {

/// HTTP content backed by a raw byte array.
class ByteArrayContent : public HttpContent {
    std::vector<SharpRuntime::bytecs> data_;
    std::string mediaType_;
public:
    explicit ByteArrayContent(const std::vector<SharpRuntime::bytecs>& data,
                              const std::string& mediaType = "application/octet-stream")
        : data_(data), mediaType_(mediaType) {}

    [[nodiscard]] std::string ReadAsString() const override {
        return std::string(data_.begin(), data_.end());
    }

    [[nodiscard]] std::vector<SharpRuntime::bytecs> ReadAsByteArray() const override {
        return data_;
    }

    [[nodiscard]] std::string getContentType() const override { return mediaType_; }
};

} // namespace System::Net::Http
