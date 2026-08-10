// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "System/Net/Http/ByteArrayContent.hpp"

namespace System::Net::Http {

/**
 * @brief HTTP content encoded as application/x-www-form-urlencoded.
 *
 * C++ counterpart of .NET System.Net.Http.FormUrlEncodedContent.  Input
 * strings are UTF-8; every byte outside RFC 3986's unreserved set is
 * percent-encoded, and spaces are encoded as '+'.
 */
class FormUrlEncodedContent : public ByteArrayContent {
public:
    /**
     * @brief Initializes the content from name/value pairs.
     * @param nameValueCollection Ordered name/value pairs to encode.
     */
    explicit FormUrlEncodedContent(
        const std::vector<std::pair<std::string, std::string>>& nameValueCollection)
        : ByteArrayContent(EncodeCollection(nameValueCollection), kMediaType) {}

private:
    static constexpr const char* kMediaType = "application/x-www-form-urlencoded";

    static void AppendEncoded(std::string& target, const std::string& value) {
        static constexpr char Hex[] = "0123456789ABCDEF";
        target.reserve(target.size() + value.size());
        for (unsigned char byte : value) {
            const bool unreserved = (byte >= 'A' && byte <= 'Z') ||
                                    (byte >= 'a' && byte <= 'z') ||
                                    (byte >= '0' && byte <= '9') ||
                                    byte == '-' || byte == '.' || byte == '_' || byte == '~';
            if (unreserved) {
                target += static_cast<char>(byte);
            } else if (byte == ' ') {
                target += '+';
            } else {
                target += '%';
                target += Hex[byte >> 4U];
                target += Hex[byte & 0x0FU];
            }
        }
    }

    template<typename TPair>
    static std::vector<SharpRuntime::bytecs> EncodeCollection(const std::vector<TPair>& nameValueCollection) {
        std::string encoded;
        for (const auto& [key, value] : nameValueCollection) {
            if (!encoded.empty()) encoded += '&';
            AppendEncoded(encoded, key);
            encoded += '=';
            AppendEncoded(encoded, value);
        }
        return std::vector<SharpRuntime::bytecs>(encoded.begin(), encoded.end());
    }
};

} // namespace System::Net::Http
