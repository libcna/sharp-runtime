// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include "System/Buffers/StandardFormat.hpp"
#include "System/Span.hpp"

namespace System::Buffers::Text {

/**
 * @brief Methods to format common data types as UTF-8 strings.
 *
 * C++ counterpart of .NET System.Buffers.Text.Utf8Formatter.
 * All methods are static TryFormat overloads that write into a Span&lt;byte&gt;
 * and return true on success or false if the destination is too small.
 */
class Utf8Formatter {
public:
    Utf8Formatter() = delete;

    // -----------------------------------------------------------------------
    // bool
    // -----------------------------------------------------------------------

    /**
     * @brief Formats a bool as a UTF-8 string ("True"/"False" or "true"/"false").
     *
     * C++ counterpart of .NET Utf8Formatter.TryFormat(bool, Span&lt;byte&gt;, out int, StandardFormat).
     * @param value        Value to format.
     * @param destination  Buffer to write into.
     * @param bytesWritten Receives number of bytes written.
     * @param format       Format specifier ('G' for True/False, 'l' for true/false).
     * @return true on success; false if destination is too small.
     */
    static bool TryFormat(bool value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        char sym = (format.getSymbolProperty() == '\0') ? 'G' : format.getSymbolProperty();
        const char* str = value
            ? (sym == 'l' ? "true"  : "True")
            : (sym == 'l' ? "false" : "False");
        int len = static_cast<int>(std::strlen(str));
        if (destination.getLengthProperty() < len) { bytesWritten = 0; return false; }
        std::memcpy(destination.getPointer(), str, static_cast<size_t>(len));
        bytesWritten = len;
        return true;
    }

    // -----------------------------------------------------------------------
    // Integer types (decimal format)
    // -----------------------------------------------------------------------

    /**
     * @brief Formats a uint8_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(byte, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(uint8_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatUInt(static_cast<uint64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a int8_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(sbyte, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(int8_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatInt(static_cast<int64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a uint16_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(ushort, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(uint16_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatUInt(static_cast<uint64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a int16_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(short, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(int16_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatInt(static_cast<int64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a uint32_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(uint, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(uint32_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatUInt(static_cast<uint64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a int32_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(int, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(int32_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatInt(static_cast<int64_t>(value), destination, bytesWritten);
    }

    /**
     * @brief Formats a uint64_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(ulong, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(uint64_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatUInt(value, destination, bytesWritten);
    }

    /**
     * @brief Formats a int64_t as UTF-8 decimal.
     * C++ counterpart of .NET Utf8Formatter.TryFormat(long, Span&lt;byte&gt;, out int, StandardFormat).
     */
    static bool TryFormat(int64_t value, System::Span<uint8_t> destination, int& bytesWritten,
                          System::Buffers::StandardFormat format = {}) {
        return tryFormatInt(value, destination, bytesWritten);
    }

private:
    static bool tryFormatUInt(uint64_t v, System::Span<uint8_t> dest, int& written) {
        char buf[20];
        int len = 0;
        if (v == 0) { buf[len++] = '0'; }
        else { while (v > 0) { buf[len++] = static_cast<char>('0' + v % 10); v /= 10; } std::reverse(buf, buf + len); }
        if (dest.getLengthProperty() < len) { written = 0; return false; }
        std::memcpy(dest.getPointer(), buf, static_cast<size_t>(len));
        written = len; return true;
    }
    static bool tryFormatInt(int64_t v, System::Span<uint8_t> dest, int& written) {
        char buf[21];
        int len = 0;
        bool neg = v < 0;
        uint64_t u = neg ? static_cast<uint64_t>(-(v + 1)) + 1 : static_cast<uint64_t>(v);
        if (u == 0) { buf[len++] = '0'; }
        else { while (u > 0) { buf[len++] = static_cast<char>('0' + u % 10); u /= 10; } std::reverse(buf, buf + len); }
        if (neg) { std::memmove(buf + 1, buf, static_cast<size_t>(len)); buf[0] = '-'; len++; }
        if (dest.getLengthProperty() < len) { written = 0; return false; }
        std::memcpy(dest.getPointer(), buf, static_cast<size_t>(len));
        written = len; return true;
    }
};

} // namespace System::Buffers::Text
