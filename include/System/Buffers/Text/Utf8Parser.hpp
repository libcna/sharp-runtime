// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "System/Buffers/StandardFormat.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers::Text {

/**
 * @brief Methods to parse common data types from UTF-8 encoded text.
 *
 * C++ counterpart of .NET System.Buffers.Text.Utf8Parser.
 * All methods are static TryParse overloads that read from a ReadOnlySpan&lt;byte&gt;
 * and return true on success or false if the input is not syntactically valid.
 */
class Utf8Parser {
public:
    Utf8Parser() = delete;

    // -----------------------------------------------------------------------
    // bool
    // -----------------------------------------------------------------------

    /**
     * @brief Parses a Boolean at the start of a UTF-8 string.
     *
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out bool, out int, char).
     * Accepts "True"/"False" (format 'G') and "true"/"false" (format 'l'); parsing is case-insensitive.
     * @param source        UTF-8 input.
     * @param value         Receives the parsed value.
     * @param bytesConsumed Receives the number of bytes consumed on success, or 0 on failure.
     * @param standardFormat Format character ('G', 'l', or '\0' for default).
     * @return true on success; false if input is not a valid boolean.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, bool& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        bytesConsumed = 0;
        int len = source.getLengthProperty();
        const uint8_t* p = source.getPointer();

        auto iequal = [](const uint8_t* a, const char* b, int n) {
            for (int i = 0; i < n; ++i)
                if ((a[i] | 0x20) != static_cast<uint8_t>(b[i] | 0x20)) return false;
            return true;
        };

        if (len >= 4 && iequal(p, "true", 4)) {
            value = true; bytesConsumed = 4; return true;
        }
        if (len >= 5 && iequal(p, "false", 5)) {
            value = false; bytesConsumed = 5; return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Integer types
    // -----------------------------------------------------------------------

    /**
     * @brief Parses a uint8_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out byte, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint8_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        uint64_t v = 0; int n = 0;
        if (!tryParseUInt(source, v, n) || v > 255) return false;
        value = static_cast<uint8_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int8_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out sbyte, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int8_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t v = 0; int n = 0;
        if (!tryParseInt(source, v, n) || v < -128 || v > 127) return false;
        value = static_cast<int8_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint16_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out ushort, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint16_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        uint64_t v = 0; int n = 0;
        if (!tryParseUInt(source, v, n) || v > 65535) return false;
        value = static_cast<uint16_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int16_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out short, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int16_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t v = 0; int n = 0;
        if (!tryParseInt(source, v, n) || v < -32768 || v > 32767) return false;
        value = static_cast<int16_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint32_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out uint, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint32_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        uint64_t v = 0; int n = 0;
        if (!tryParseUInt(source, v, n) || v > 0xFFFFFFFFu) return false;
        value = static_cast<uint32_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int32_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out int, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int32_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t v = 0; int n = 0;
        if (!tryParseInt(source, v, n) || v < INT32_MIN || v > INT32_MAX) return false;
        value = static_cast<int32_t>(v); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint64_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out ulong, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint64_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        uint64_t v = 0; int n = 0;
        if (!tryParseUInt(source, v, n)) return false;
        value = v; bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int64_t from decimal UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out long, out int, char).
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int64_t& value, int& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t v = 0; int n = 0;
        if (!tryParseInt(source, v, n)) return false;
        value = v; bytesConsumed = n; return true;
    }

private:
    static bool tryParseUInt(System::ReadOnlySpan<uint8_t> src, uint64_t& out, int& n) {
        int len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        if (len == 0 || p[0] < '0' || p[0] > '9') return false;
        uint64_t v = 0; n = 0;
        while (n < len && p[n] >= '0' && p[n] <= '9') {
            uint64_t next = v * 10 + (p[n] - '0');
            if (next < v) return false; // overflow
            v = next; ++n;
        }
        out = v; return n > 0;
    }

    static bool tryParseInt(System::ReadOnlySpan<uint8_t> src, int64_t& out, int& n) {
        int len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        bool neg = (len > 0 && p[0] == '-');
        System::ReadOnlySpan<uint8_t> rest(p + (neg ? 1 : 0), len - (neg ? 1 : 0));
        uint64_t v = 0; int digits = 0;
        if (!tryParseUInt(rest, v, digits)) return false;
        if (neg) {
            if (v > static_cast<uint64_t>(INT64_MAX) + 1) return false;
            out = -static_cast<int64_t>(v);
        } else {
            if (v > static_cast<uint64_t>(INT64_MAX)) return false;
            out = static_cast<int64_t>(v);
        }
        n = digits + (neg ? 1 : 0);
        return true;
    }
};

} // namespace System::Buffers::Text
