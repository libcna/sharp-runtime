// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Buffers/OperationStatus.hpp"
#include "System/Span.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers::Text {

/**
 * @brief Converts between binary data and UTF-8 base64 encoded text.
 *
 * C++ counterpart of .NET System.Buffers.Text.Base64.
 * All methods are static. The implementation uses the standard RFC 4648 base64
 * alphabet with '=' padding.
 */
class Base64 {
    static constexpr char kEncTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static constexpr int8_t kDecTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
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
    // -2 means '=' (padding); -1 means invalid

public:
    Base64() = delete;

    // -----------------------------------------------------------------------
    // Length helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the maximum number of bytes that decoding @p base64Length bytes can produce.
     * C++ counterpart of .NET Base64.GetMaxDecodedFromUtf8Length(int).
     */
    [[nodiscard]] static int GetMaxDecodedFromUtf8Length(int base64Length) {
        if (base64Length < 0) throw std::out_of_range("base64Length must be non-negative");
        return (base64Length / 4) * 3;
    }

    /**
     * @brief Returns the exact number of bytes that encoding @p bytesLength bytes produces.
     * C++ counterpart of .NET Base64.GetMaxEncodedToUtf8Length(int).
     */
    [[nodiscard]] static int GetMaxEncodedToUtf8Length(int bytesLength) {
        if (bytesLength < 0) throw std::out_of_range("bytesLength must be non-negative");
        return ((bytesLength + 2) / 3) * 4;
    }

    // -----------------------------------------------------------------------
    // Encode
    // -----------------------------------------------------------------------

    /**
     * @brief Encodes binary bytes as UTF-8 base64 text.
     *
     * C++ counterpart of .NET Base64.EncodeToUtf8(ReadOnlySpan, Span, out int, out int, bool).
     * @param bytes           Source bytes.
     * @param utf8            Destination for base64 ASCII bytes.
     * @param bytesConsumed   Set to number of source bytes consumed.
     * @param bytesWritten    Set to number of output bytes written.
     * @param isFinalBlock    If true, output is padded with '='.
     * @return OperationStatus::Done, DestinationTooSmall, or NeedMoreData.
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

        // Encode full 3-byte groups
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

        if (!isFinalBlock && remainder > 0) {
            return OperationStatus::NeedMoreData;
        }

        // Encode partial group with padding
        if (isFinalBlock && remainder > 0) {
            if (dstLen - bytesWritten < 4) return OperationStatus::DestinationTooSmall;
            if (remainder == 1) {
                uint32_t b = uint32_t(src[0]) << 16;
                dst[0] = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
                dst[1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
                dst[2] = '='; dst[3] = '=';
            } else {
                uint32_t b = (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8);
                dst[0] = static_cast<uint8_t>(kEncTable[(b >> 18) & 0x3F]);
                dst[1] = static_cast<uint8_t>(kEncTable[(b >> 12) & 0x3F]);
                dst[2] = static_cast<uint8_t>(kEncTable[(b >>  6) & 0x3F]);
                dst[3] = '=';
            }
            bytesConsumed += remainder;
            bytesWritten  += 4;
        }
        return OperationStatus::Done;
    }

    /**
     * @brief Encodes binary bytes as a std::string of base64 ASCII.
     *
     * Convenience wrapper not in .NET API but useful in C++.
     */
    [[nodiscard]] static std::string EncodeToString(const std::vector<uint8_t>& bytes) {
        int outLen = GetMaxEncodedToUtf8Length(static_cast<int>(bytes.size()));
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
     * @brief Decodes UTF-8 base64 text to binary bytes.
     *
     * C++ counterpart of .NET Base64.DecodeFromUtf8(ReadOnlySpan, Span, out int, out int, bool).
     * @param utf8            Source base64 ASCII bytes.
     * @param bytes           Destination for decoded bytes.
     * @param bytesConsumed   Set to number of input bytes consumed.
     * @param bytesWritten    Set to number of decoded bytes written.
     * @param isFinalBlock    If true, trailing padding '=' is expected/allowed.
     * @return OperationStatus.
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
        while (si + 3 < srcLen) {
            int8_t a = kDecTable[src[si]];
            int8_t b = kDecTable[src[si+1]];
            int8_t c = kDecTable[src[si+2]];
            int8_t d = kDecTable[src[si+3]];

            if (a < 0 || b < 0) return OperationStatus::InvalidData;

            bool cPad = (c == -2);
            bool dPad = (d == -2);

            if (cPad && dPad) {
                if (dstLen - di < 1) return OperationStatus::DestinationTooSmall;
                dst[di++] = static_cast<uint8_t>((uint32_t(a) << 2) | (uint32_t(b) >> 4));
                si += 4;
                bytesConsumed = si; bytesWritten = di;
                return OperationStatus::Done;
            }
            if (dPad) {
                if (c < 0) return OperationStatus::InvalidData;
                if (dstLen - di < 2) return OperationStatus::DestinationTooSmall;
                uint32_t val = (uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6);
                dst[di++] = static_cast<uint8_t>(val >> 16);
                dst[di++] = static_cast<uint8_t>(val >> 8);
                si += 4;
                bytesConsumed = si; bytesWritten = di;
                return OperationStatus::Done;
            }
            if (c < 0 || d < 0) return OperationStatus::InvalidData;
            if (dstLen - di < 3) return OperationStatus::DestinationTooSmall;
            uint32_t val = (uint32_t(a)<<18)|(uint32_t(b)<<12)|(uint32_t(c)<<6)|uint32_t(d);
            dst[di++] = static_cast<uint8_t>(val >> 16);
            dst[di++] = static_cast<uint8_t>(val >> 8);
            dst[di++] = static_cast<uint8_t>(val);
            si += 4;
        }
        bytesConsumed = si; bytesWritten = di;

        if (!isFinalBlock && (srcLen - si) > 0 && (srcLen - si) < 4) {
            return OperationStatus::NeedMoreData;
        }
        return OperationStatus::Done;
    }

    // -----------------------------------------------------------------------
    // Validate
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if @p base64Text is valid base64.
     *
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;byte&gt;).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<uint8_t> base64Text) {
        int len = base64Text.getLengthProperty();
        if (len % 4 != 0) return false;
        const uint8_t* p = base64Text.getPointer();
        int padCount = 0;
        for (int i = 0; i < len; ++i) {
            int8_t v = kDecTable[p[i]];
            if (v == -1) return false;
            if (v == -2) {
                ++padCount;
                if (padCount > 2) return false;
                if (i < len - 2) return false;
            } else if (padCount > 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns true if @p base64Text (as char string) is valid base64.
     *
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;char&gt;).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<char> base64Text) {
        int len = base64Text.getLengthProperty();
        if (len % 4 != 0) return false;
        const char* p = base64Text.getPointer();
        int padCount = 0;
        for (int i = 0; i < len; ++i) {
            int8_t v = kDecTable[static_cast<uint8_t>(p[i])];
            if (v == -1) return false;
            if (v == -2) {
                ++padCount;
                if (padCount > 2) return false;
                if (i < len - 2) return false;
            } else if (padCount > 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Validates base64 input and returns true along with the decoded byte count.
     *
     * C++ counterpart of .NET Base64.IsValid(ReadOnlySpan&lt;byte&gt;, out int).
     */
    [[nodiscard]] static bool IsValid(System::ReadOnlySpan<uint8_t> base64Text,
                                       int& decodedLength) {
        if (!IsValid(base64Text)) { decodedLength = 0; return false; }
        int len = base64Text.getLengthProperty();
        decodedLength = GetMaxDecodedFromUtf8Length(len);
        // subtract padding bytes
        if (len >= 1 && base64Text.getPointer()[len-1] == '=') --decodedLength;
        if (len >= 2 && base64Text.getPointer()[len-2] == '=') --decodedLength;
        return true;
    }
};

} // namespace System::Buffers::Text
