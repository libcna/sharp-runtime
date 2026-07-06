// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <string>

namespace System::Net {

    /**
     * @brief Identifies well-known HTTP response headers.
     *
     * C++ counterpart of .NET System.Net.HttpResponseHeader.
     */
    enum class HttpResponseHeader {
        CacheControl = 0,
        Connection = 1,
        Date = 2,
        KeepAlive = 3,
        Pragma = 4,
        Trailer = 5,
        TransferEncoding = 6,
        Upgrade = 7,
        Via = 8,
        Warning = 9,
        Allow = 10,
        ContentLength = 11,
        ContentType = 12,
        ContentEncoding = 13,
        ContentLanguage = 14,
        ContentLocation = 15,
        ContentMd5 = 16,
        ContentRange = 17,
        Expires = 18,
        LastModified = 19,

        AcceptRanges = 20,
        Age = 21,
        ETag = 22,
        Location = 23,
        ProxyAuthenticate = 24,
        RetryAfter = 25,
        Server = 26,
        SetCookie = 27,
        Vary = 28,
        WwwAuthenticate = 29,
    };

    /** @return The wire-format header name for @p header (e.g. "Set-Cookie"). */
    inline const std::string& HttpResponseHeaderGetName(HttpResponseHeader header) {
        static const std::array<std::string, 30> names = {
            "Cache-Control", "Connection", "Date", "Keep-Alive", "Pragma", "Trailer",
            "Transfer-Encoding", "Upgrade", "Via", "Warning", "Allow", "Content-Length",
            "Content-Type", "Content-Encoding", "Content-Language", "Content-Location",
            "Content-MD5", "Content-Range", "Expires", "Last-Modified", "Accept-Ranges",
            "Age", "ETag", "Location", "Proxy-Authenticate", "Retry-After", "Server",
            "Set-Cookie", "Vary", "WWW-Authenticate",
        };
        return names.at(static_cast<size_t>(header));
    }

} // namespace System::Net
