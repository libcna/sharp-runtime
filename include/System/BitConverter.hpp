// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <bit>
#include <cstring>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::Single;

    /**
     * @brief Converts base data types to an array of bytes, and an array of
     * bytes to base data types (little-endian).
     *
     * Partial C++ counterpart of .NET System.BitConverter.
     *
     * @note Status: Implemented
     */
    class BitConverter {
    public:
        /// Deleted constructor; all members are static.
        BitConverter() = delete;

        /// Indicates the byte order in which the system stores data; true for little-endian.
        static constexpr bool IsLittleEndian = (std::endian::native == std::endian::little);

        // --- To bytes ---
        /// Returns the specified 16-bit integer as an array of bytes.
        [[nodiscard]] static std::array<bytecs,2> GetBytes(shortcs value) {
            std::array<bytecs,2> b;
            std::memcpy(b.data(), &value, 2);
            return b;
        }
        /// Returns the specified 32-bit integer as an array of bytes.
        [[nodiscard]] static std::array<bytecs,4> GetBytes(intcs value) {
            std::array<bytecs,4> b;
            std::memcpy(b.data(), &value, 4);
            return b;
        }
        /// Returns the specified 64-bit integer as an array of bytes.
        [[nodiscard]] static std::array<bytecs,8> GetBytes(longcs value) {
            std::array<bytecs,8> b;
            std::memcpy(b.data(), &value, 8);
            return b;
        }
        /// Returns the specified single-precision float as an array of bytes.
        [[nodiscard]] static std::array<bytecs,4> GetBytes(Single value) {
            std::array<bytecs,4> b;
            std::memcpy(b.data(), &value, 4);
            return b;
        }
        /// Returns the specified double-precision float as an array of bytes.
        [[nodiscard]] static std::array<bytecs,8> GetBytes(double value) {
            std::array<bytecs,8> b;
            std::memcpy(b.data(), &value, 8);
            return b;
        }
        /// Returns the specified boolean value as a one-element byte array.
        [[nodiscard]] static std::array<bytecs,1> GetBytes(bool value) {
            return { static_cast<bytecs>(value ? 1 : 0) };
        }

        // --- From bytes ---
        /// Converts two bytes at the specified position to a 16-bit signed integer.
        [[nodiscard]] static shortcs ToInt16 (const bytecs* value, intcs startIndex) { shortcs r; std::memcpy(&r, value+startIndex, 2); return r; }
        /// Converts four bytes at the specified position to a 32-bit signed integer.
        [[nodiscard]] static intcs   ToInt32 (const bytecs* value, intcs startIndex) { intcs   r; std::memcpy(&r, value+startIndex, 4); return r; }
        /// Converts eight bytes at the specified position to a 64-bit signed integer.
        [[nodiscard]] static longcs  ToInt64 (const bytecs* value, intcs startIndex) { longcs  r; std::memcpy(&r, value+startIndex, 8); return r; }
        /// Converts four bytes at the specified position to a single-precision float.
        [[nodiscard]] static Single  ToSingle(const bytecs* value, intcs startIndex) { Single  r; std::memcpy(&r, value+startIndex, 4); return r; }
        /// Converts eight bytes at the specified position to a double-precision float.
        [[nodiscard]] static double  ToDouble(const bytecs* value, intcs startIndex) { double  r; std::memcpy(&r, value+startIndex, 8); return r; }
        /// Returns a boolean converted from the byte at the specified position.
        [[nodiscard]] static bool    ToBoolean(const bytecs* value, intcs startIndex) { return value[startIndex] != 0; }

        // Vector overloads
        /// Converts two bytes from a vector to a 16-bit signed integer.
        [[nodiscard]] static shortcs ToInt16 (const std::vector<bytecs>& v, intcs i) { return ToInt16 (v.data(), i); }
        /// Converts four bytes from a vector to a 32-bit signed integer.
        [[nodiscard]] static intcs   ToInt32 (const std::vector<bytecs>& v, intcs i) { return ToInt32 (v.data(), i); }
        /// Converts eight bytes from a vector to a 64-bit signed integer.
        [[nodiscard]] static longcs  ToInt64 (const std::vector<bytecs>& v, intcs i) { return ToInt64 (v.data(), i); }
        /// Converts four bytes from a vector to a single-precision float.
        [[nodiscard]] static Single  ToSingle(const std::vector<bytecs>& v, intcs i) { return ToSingle(v.data(), i); }
        /// Converts eight bytes from a vector to a double-precision float.
        [[nodiscard]] static double  ToDouble(const std::vector<bytecs>& v, intcs i) { return ToDouble(v.data(), i); }

        // --- Bit reinterpretation ---
        /// Reinterprets a double as its IEEE 754 bit pattern (a 64-bit integer).
        [[nodiscard]] static longcs DoubleToInt64Bits(double value) {
            longcs r; std::memcpy(&r, &value, 8); return r;
        }
        /// Reinterprets a 64-bit integer as a double using its IEEE 754 bit pattern.
        [[nodiscard]] static double Int64BitsToDouble(longcs value) {
            double r; std::memcpy(&r, &value, 8); return r;
        }
        /// Reinterprets a single-precision float as its IEEE 754 bit pattern (a 32-bit integer).
        [[nodiscard]] static intcs SingleToInt32Bits(Single value) {
            intcs r; std::memcpy(&r, &value, 4); return r;
        }
        /// Reinterprets a 32-bit integer as a single-precision float using its IEEE 754 bit pattern.
        [[nodiscard]] static Single Int32BitsToSingle(intcs value) {
            Single r; std::memcpy(&r, &value, 4); return r;
        }

        /// Converts a byte range to a hex-string representation.
        [[nodiscard]] static std::string ToString(const bytecs* value, intcs startIndex, intcs length);
        /// Converts a byte vector to a hex-string representation.
        [[nodiscard]] static std::string ToString(const std::vector<bytecs>& value);
    };

} // namespace System
