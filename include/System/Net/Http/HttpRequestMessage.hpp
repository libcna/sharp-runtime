// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace System::Net::Http {

/// Represents an HTTP request message.
class HttpRequestMessage {
    HttpMethod                                     method_;
    std::string                                    uri_;
    std::shared_ptr<HttpContent>                   content_;
    std::unordered_map<std::string, std::string>   headers_;
public:
    HttpRequestMessage() : method_(HttpMethod::Get()) {}

    HttpRequestMessage(const HttpMethod& method, const std::string& uri)
        : method_(method), uri_(uri) {}

    [[nodiscard]] const HttpMethod& getMethodProperty() const { return method_; }
    void setMethodProperty(const HttpMethod& v)               { method_ = v; }

    [[nodiscard]] const std::string& getRequestUriProperty() const { return uri_; }
    void setRequestUriProperty(const std::string& v)               { uri_ = v; }

    [[nodiscard]] std::shared_ptr<HttpContent> getContentProperty() const { return content_; }
    void setContentProperty(std::shared_ptr<HttpContent> v)               { content_ = std::move(v); }

    void setHeader(const std::string& name, const std::string& value) { headers_[name] = value; }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeaders() const {
        return headers_;
    }
};

} // namespace System::Net::Http
