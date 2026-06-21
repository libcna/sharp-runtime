// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Buffers/OperationStatus.hpp"
#include "System/Span.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers::Text {

/**
 * @brief Converts between binary data and URL-safe base64url encoded text.
 *
 * C++ counterpart of .NET System.Buffers.Text.Base64Url (introduced .NET 9).
 * Uses the RFC 4648 §5 alphabet ('+' → '-', '/' → '_') without padding.
 */
class Base64Url {
    static constexpr char kEncTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    static constexpr int8_t kDecTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

public:
    Base64Url() = delete;

    // -----------------------------------------------------------------------
    // Length helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the number of UTF-8 bytes needed to encode @p bytesLength raw bytes.
     * C++ counterpart of .NET Base64Url.GetEncodedLength(int).
     * Base64Url output is unpadded: length = ceil(n * 4/3).
     */
    [[nodiscard]] static int GetEncodedLength(int bytesLength) {
        if (bytesLength < 0) throw std::out_of_range("bytesLength must be non-negative");
        return (bytesLength * 4 + 2) / 3;
    }

    /**
     * @brief Returns the maximum number of bytes that decoding @p base64UrlLength chars can produce.
     * C++ counterpart of .NET Base64Url.GetMaxDecodedLength(int).
     */
    [[nodiscard]] static int GetMaxDecodedLength(int base64UrlLength) {
        if (base64UrlLength < 0) throw std::out_of_range("base64UrlLength must be non-negative");
        return (base64UrlLength * 3 + 3) / 4;
    }

    // -----------------------------------------------------------------------
    // Encode
    // -----------------------------------------------------------------------

    /**
     * @brief Encodes binary bytes as UTF-8 base64url (no padding).
     *
     * C++ counterpart of .NET Base64Url.EncodeToUtf8(ReadOnlySpan, Span, out int, out int, bool).
     */
    static OperationStatus EncodeToUtf8(
        System::ReadOnlySpan<uint8_t> bytes,
        System::Span<uint8_t>         utf8,
        int&                          bytesConsumed,
        int&                          bytesWritten,
        bool                          isFinalBlock = true)
    {
        bytesConsumed = 0;
        bytesWritten  = 0;
        const uint8_t* src = bytes.getPointer();
        uint8_t*       dst = utf8.getPointer();
        int srcLen = bytes.getLengthProperty();
        int dstLen = utf8.getLengthProperty();

        int fullGroups = srcLen / 3;
        int remainder  = srcLen % 3;

        for (int i = 0; i < fullGroups; ++i) {
            if (dstLen - bytesWritten < 4) return OperationStatus::DestinationTooSmall;
            uint32_t b = (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8) | src[2];
            dst[0] = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            dst[1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            dst[2] = static_cast<uint8_t>(kEncTable[(b >>  6) & 0x3F]);
            dst[3] = static_cast<uint8_t>(kEncTable[(b      ) & 0x3F]);
            src += 3; dst += 4;
            bytesConsumed += 3;
            bytesWritten  += 4;
        }

        if (!isFinalBlock && remainder > 0) return OperationStatus::NeedMoreData;

        if (isFinalBlock && remainder == 1) {
            if (dstLen - bytesWritten < 2) return OperationStatus::DestinationTooSmall;
            uint32_t b = uint32_t(src[0]) << 16;
            dst[0] = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            dst[1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            bytesConsumed += 1; bytesWritten += 2;
        } else if (isFinalBlock && remainder == 2) {
            if (dstLen - bytesWritten < 3) return OperationStatus::DestinationTooSmall;
            uint32_t b = (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8);
            dst[0] = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
            dst[1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
            dst[2] = static_cast<uint8_t>(kEncTable[(b >>  6) & 0x3F]);
            bytesConsumed += 2; bytesWritten += 3;
        }
        return OperationStatus::Done;
    }

    /**
     * @brief Encodes binary bytes as a std::string of base64url text.
     */
    [[nodiscard]] static std::string EncodeToString(const std::vector<uint8_t>& bytes) {
        int outLen = GetEncodedLength(static_cast<int>(bytes.size()));
        std::vector<uint8_t> out(static_cast<size_t>(outLen));
        int consumed = 0, written = 0;
        System::ReadOnlySpan<uint8_t> src(bytes.data(), static_cast<int>(bytes.size()));
        System::Span<uint8_t>         dst(out.data(),   outLen);
        EncodeToUtf8(src, dst, consumed, written, true);
        return std::string(out.begin(), out.begin() + written);
    }

    // -----------------------------------------------------------------------
    // Decode
    // -----------------------------------------------------------------------

    /**
     * @brief Decodes UTF-8 base64url text to binary bytes.
     *
     * C++ counterpart of .NET Base64Url.DecodeFromUtf8(ReadOnlySpan, Span, out int, out int, bool).
     */
    static OperationStatus DecodeFromUtf8(
        System::ReadOnlySpan<uint8_t> utf8,
        System::Span<uint8_t>         bytes,
        int&                          bytesConsumed,
        int&                          bytesWritten,
        bool                          isFinalBlock = true)
    {
        bytesConsumed = 0;
        bytesWritten  = 0;
        const uint8_t* src = utf8.getPointer();
        uint8_t*       dst = bytes.getPointer();
        int srcLen = utf8.getLengthProperty();
        int dstLen = bytes.getLengthProperty();

        int si = 0, di = 0;
        // Decode full 4-char groups
        while (si + 3 < srcLen) {
            int8_t a = kDecTable[src[si]];
            int8_t b = kDecTable[src[si+1]];
            int8_t c = kDecTable[src[si+2]];
            int8_t d = kDecTable[src[si+3]];
            if (a < 0 || b < 0 || c < 0 || d < 0) break;
            if (dstLen - di < 3) return OperationStatus::DestinationTooSmall;
            uint32_t val = (uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6)|uint32_t(d);
            dst[di++] = static_cast<uint8_t>(val >> 16);
            dst[di++] = static_cast<uint8_t>(val >> 8);
            dst[di++] = static_cast<uint8_t>(val);
            si += 4;
        }
        // Handle tail (base64url: 2 or 3 chars without padding)
        int rem = srcLen - si;
        if (rem == 3) {
            int8_t a = kDecTable[src[si]];
            int8_t b = kDecTable[src[si+1]];
            int8_t c = kDecTable[src[si+2]];
            if (a < 0 || b < 0 || c < 0) { bytesConsumed = si; bytesWritten = di; return OperationStatus::InvalidData; }
            if (dstLen - di < 2) return OperationStatus::DestinationTooSmall;
            uint32_t val = (uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6);
            dst[di++] = static_cast<uint8_t>(val >> 16);
            dst[di++] = static_cast<uint8_t>(val >> 8);
            si += 3;
        } else if (rem == 2) {
            int8_t a = kDecTable[src[si]];
            int8_t b = kDecTable[src[si+1]];
            if (a < 0 || b < 0) { bytesConsumed = si; bytesWritten = di; return OperationStatus::InvalidData; }
            if (dstLen - di < 1) return OperationStatus::DestinationTooSmall;
            uint32_t val = (uint32_t(a)<<18)|(uint32_t(b)<<12);
            dst[di++] = static_cast<uint8_t>(val >> 16);
            si += 2;
        } else if (rem == 1) {
            if (!isFinalBlock) { bytesConsumed = si; bytesWritten = di; return OperationStatus::NeedMoreData; }
            bytesConsumed = si; bytesWritten = di;
            return OperationStatus::InvalidData;
        }
        bytesConsumed = si; bytesWritten = di;
        return OperationStatus::Done;
    }

    // -----------------------------------------------------------------------
    // Validate
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if @p base64UrlText is valid base64url (no padding expected).
     * C++ counterpart of .NET Base64Url.IsValid(ReadOnlySpan&lt;byte&gt;).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<uint8_t> base64UrlText) {
        int len = base64UrlText.getLengthProperty();
        if (len % 4 == 1) return false;  // impossible length
        const uint8_t* p = base64UrlText.getPointer();
        for (int i = 0; i < len; ++i) {
            if (kDecTable[p[i]] < 0) return false;
        }
        return true;
    }

    /**
     * @brief Returns true if @p base64UrlText (char) is valid base64url.
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<char> base64UrlText) {
        int len = base64UrlText.getLengthProperty();
        if (len % 4 == 1) return false;
        const char* p = base64UrlText.getPointer();
        for (int i = 0; i < len; ++i) {
            if (kDecTable[static_cast<uint8_t>(p[i])] < 0) return false;
        }
        return true;
    }
};

} // namespace System::Buffers::Text
