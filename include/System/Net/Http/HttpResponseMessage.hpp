// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace System::Net::Http {

/** Represents an HTTP response, including status code, headers, and body content. */
class HttpResponseMessage {
    System::Net::HttpStatusCode                    statusCode_;
    std::string                                    reasonPhrase_;
    std::shared_ptr<HttpContent>                   content_;
    std::unordered_map<std::string, std::string>   headers_;
public:
    explicit HttpResponseMessage(
        System::Net::HttpStatusCode statusCode = System::Net::HttpStatusCode::OK)
        : statusCode_(statusCode) {}

    [[nodiscard]] System::Net::HttpStatusCode getStatusCodeProperty() const { return statusCode_; }
    void setStatusCodeProperty(System::Net::HttpStatusCode v)               { statusCode_ = v; }

    [[nodiscard]] const std::string& getReasonPhraseProperty() const { return reasonPhrase_; }
    void setReasonPhraseProperty(const std::string& v)               { reasonPhrase_ = v; }

    [[nodiscard]] std::shared_ptr<HttpContent> getContentProperty() const { return content_; }
    void setContentProperty(std::shared_ptr<HttpContent> v)               { content_ = std::move(v); }

    /** True when StatusCode is in the 200–299 range. */
    [[nodiscard]] bool getIsSuccessStatusCodeProperty() const {
        int c = static_cast<int>(statusCode_);
        return c >= 200 && c < 300;
    }

    /** Throws std::runtime_error if the response indicates failure. */
    void EnsureSuccessStatusCode() const {
        if (!getIsSuccessStatusCodeProperty())
            throw std::runtime_error(
                "Response status code does not indicate success: " +
                std::to_string(static_cast<int>(statusCode_)) + " (" + reasonPhrase_ + ")");
    }

    void setHeader(const std::string& name, const std::string& value) { headers_[name] = value; }

    [[nodiscard]] std::string getHeader(const std::string& name) const {
        auto it = headers_.find(name);
        return it != headers_.end() ? it->second : "";
    }

    [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeaders() const {
        return headers_;
    }
};

} // namespace System::Net::Http
