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
#include "System/Half.hpp"

namespace System {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::Single;
    using SharpRuntime::ushortcs;
    using SharpRuntime::uintcs;
    using SharpRuntime::ulongcs;
    using SharpRuntime::charcs;

    /**
     * @brief Converts base data types to an array of bytes, and an array of
     * bytes to base data types (little-endian).
     *
     * C++ counterpart of .NET System.BitConverter.
     */
    class BitConverter {
    public:
        /** @brief Deleted constructor; all members are static. */
        BitConverter() = delete;

        /**
         * @brief Indicates the byte order in which the system stores data.
         *
         * C++ counterpart of .NET BitConverter.IsLittleEndian.
         * true on little-endian systems; false on big-endian systems.
         */
        static constexpr bool IsLittleEndian = (std::endian::native == std::endian::little);

        // -----------------------------------------------------------------------
        // GetBytes — value → byte array
        // -----------------------------------------------------------------------

        /** @brief Returns the specified Boolean value as a one-element byte array. */
        [[nodiscard]] static std::array<bytecs,1> GetBytes(bool value) {
            return { static_cast<bytecs>(value ? 1 : 0) };
        }
        /** @brief Returns the specified Unicode character value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,2> GetBytes(charcs value) {
            std::array<bytecs,2> b; std::memcpy(b.data(), &value, 2); return b;
        }
        /** @brief Returns the specified 16-bit signed integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,2> GetBytes(shortcs value) {
            std::array<bytecs,2> b; std::memcpy(b.data(), &value, 2); return b;
        }
        /** @brief Returns the specified 16-bit unsigned integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,2> GetBytes(ushortcs value) {
            std::array<bytecs,2> b; std::memcpy(b.data(), &value, 2); return b;
        }
        /** @brief Returns the specified 32-bit signed integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,4> GetBytes(intcs value) {
            std::array<bytecs,4> b; std::memcpy(b.data(), &value, 4); return b;
        }
        /** @brief Returns the specified 32-bit unsigned integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,4> GetBytes(uintcs value) {
            std::array<bytecs,4> b; std::memcpy(b.data(), &value, 4); return b;
        }
        /** @brief Returns the specified 64-bit signed integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,8> GetBytes(longcs value) {
            std::array<bytecs,8> b; std::memcpy(b.data(), &value, 8); return b;
        }
        /** @brief Returns the specified 64-bit unsigned integer value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,8> GetBytes(ulongcs value) {
            std::array<bytecs,8> b; std::memcpy(b.data(), &value, 8); return b;
        }
        /** @brief Returns the specified single-precision floating-point value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,4> GetBytes(Single value) {
            std::array<bytecs,4> b; std::memcpy(b.data(), &value, 4); return b;
        }
        /** @brief Returns the specified double-precision floating-point value as an array of bytes. */
        [[nodiscard]] static std::array<bytecs,8> GetBytes(double value) {
            std::array<bytecs,8> b; std::memcpy(b.data(), &value, 8); return b;
        }

        // -----------------------------------------------------------------------
        // To* — byte array → value (raw pointer overloads)
        // -----------------------------------------------------------------------

        /** @brief Returns a Boolean converted from the byte at a specified position in a byte array. */
        [[nodiscard]] static bool     ToBoolean(const bytecs* v, intcs i) { return v[i] != 0; }
        /** @brief Returns a Unicode character converted from two bytes at a specified position. */
        [[nodiscard]] static charcs   ToChar   (const bytecs* v, intcs i) { charcs   r; std::memcpy(&r, v+i, 2); return r; }
        /** @brief Returns a 16-bit signed integer converted from two bytes at a specified position. */
        [[nodiscard]] static shortcs  ToInt16  (const bytecs* v, intcs i) { shortcs  r; std::memcpy(&r, v+i, 2); return r; }
        /** @brief Returns a 16-bit unsigned integer converted from two bytes at a specified position. */
        [[nodiscard]] static ushortcs ToUInt16 (const bytecs* v, intcs i) { ushortcs r; std::memcpy(&r, v+i, 2); return r; }
        /** @brief Returns a 32-bit signed integer converted from four bytes at a specified position. */
        [[nodiscard]] static intcs    ToInt32  (const bytecs* v, intcs i) { intcs    r; std::memcpy(&r, v+i, 4); return r; }
        /** @brief Returns a 32-bit unsigned integer converted from four bytes at a specified position. */
        [[nodiscard]] static uintcs   ToUInt32 (const bytecs* v, intcs i) { uintcs   r; std::memcpy(&r, v+i, 4); return r; }
        /** @brief Returns a 64-bit signed integer converted from eight bytes at a specified position. */
        [[nodiscard]] static longcs   ToInt64  (const bytecs* v, intcs i) { longcs   r; std::memcpy(&r, v+i, 8); return r; }
        /** @brief Returns a 64-bit unsigned integer converted from eight bytes at a specified position. */
        [[nodiscard]] static ulongcs  ToUInt64 (const bytecs* v, intcs i) { ulongcs  r; std::memcpy(&r, v+i, 8); return r; }
        /** @brief Returns a single-precision floating-point number converted from four bytes at a specified position. */
        [[nodiscard]] static Single   ToSingle (const bytecs* v, intcs i) { Single   r; std::memcpy(&r, v+i, 4); return r; }
        /** @brief Returns a double-precision floating-point number converted from eight bytes at a specified position. */
        [[nodiscard]] static double   ToDouble (const bytecs* v, intcs i) { double   r; std::memcpy(&r, v+i, 8); return r; }

        // -----------------------------------------------------------------------
        // To* — vector overloads
        // -----------------------------------------------------------------------

        /** @brief Returns a Boolean converted from the byte at a specified position in a byte vector. */
        [[nodiscard]] static bool     ToBoolean(const std::vector<bytecs>& v, intcs i) { return ToBoolean(v.data(), i); }
        /** @brief Returns a Unicode character converted from two bytes in a byte vector. */
        [[nodiscard]] static charcs   ToChar   (const std::vector<bytecs>& v, intcs i) { return ToChar   (v.data(), i); }
        /** @brief Returns a 16-bit signed integer converted from two bytes in a byte vector. */
        [[nodiscard]] static shortcs  ToInt16  (const std::vector<bytecs>& v, intcs i) { return ToInt16  (v.data(), i); }
        /** @brief Returns a 16-bit unsigned integer converted from two bytes in a byte vector. */
        [[nodiscard]] static ushortcs ToUInt16 (const std::vector<bytecs>& v, intcs i) { return ToUInt16 (v.data(), i); }
        /** @brief Returns a 32-bit signed integer converted from four bytes in a byte vector. */
        [[nodiscard]] static intcs    ToInt32  (const std::vector<bytecs>& v, intcs i) { return ToInt32  (v.data(), i); }
        /** @brief Returns a 32-bit unsigned integer converted from four bytes in a byte vector. */
        [[nodiscard]] static uintcs   ToUInt32 (const std::vector<bytecs>& v, intcs i) { return ToUInt32 (v.data(), i); }
        /** @brief Returns a 64-bit signed integer converted from eight bytes in a byte vector. */
        [[nodiscard]] static longcs   ToInt64  (const std::vector<bytecs>& v, intcs i) { return ToInt64  (v.data(), i); }
        /** @brief Returns a 64-bit unsigned integer converted from eight bytes in a byte vector. */
        [[nodiscard]] static ulongcs  ToUInt64 (const std::vector<bytecs>& v, intcs i) { return ToUInt64 (v.data(), i); }
        /** @brief Returns a single-precision float converted from four bytes in a byte vector. */
        [[nodiscard]] static Single   ToSingle (const std::vector<bytecs>& v, intcs i) { return ToSingle (v.data(), i); }
        /** @brief Returns a double-precision float converted from eight bytes in a byte vector. */
        [[nodiscard]] static double   ToDouble (const std::vector<bytecs>& v, intcs i) { return ToDouble (v.data(), i); }

        // -----------------------------------------------------------------------
        // Bit-reinterpretation methods
        // -----------------------------------------------------------------------

        /** @brief Reinterprets a double as its IEEE 754 bit pattern (64-bit signed integer). */
        [[nodiscard]] static longcs  DoubleToInt64Bits (double value) { longcs  r; std::memcpy(&r, &value, 8); return r; }
        /** @brief Reinterprets a 64-bit signed integer as a double. */
        [[nodiscard]] static double  Int64BitsToDouble (longcs  value) { double  r; std::memcpy(&r, &value, 8); return r; }
        /** @brief Reinterprets a double as its IEEE 754 bit pattern (64-bit unsigned integer). */
        [[nodiscard]] static ulongcs DoubleToUInt64Bits(double value) { ulongcs r; std::memcpy(&r, &value, 8); return r; }
        /** @brief Reinterprets a 64-bit unsigned integer as a double. */
        [[nodiscard]] static double  UInt64BitsToDouble(ulongcs value) { double  r; std::memcpy(&r, &value, 8); return r; }
        /** @brief Reinterprets a single-precision float as its IEEE 754 bit pattern (32-bit signed integer). */
        [[nodiscard]] static intcs   SingleToInt32Bits (Single value) { intcs   r; std::memcpy(&r, &value, 4); return r; }
        /** @brief Reinterprets a 32-bit signed integer as a single-precision float. */
        [[nodiscard]] static Single  Int32BitsToSingle (intcs   value) { Single  r; std::memcpy(&r, &value, 4); return r; }
        /** @brief Reinterprets a single-precision float as its IEEE 754 bit pattern (32-bit unsigned integer). */
        [[nodiscard]] static uintcs  SingleToUInt32Bits(Single value) { uintcs  r; std::memcpy(&r, &value, 4); return r; }
        /** @brief Reinterprets a 32-bit unsigned integer as a single-precision float. */
        [[nodiscard]] static Single  UInt32BitsToSingle(uintcs  value) { Single  r; std::memcpy(&r, &value, 4); return r; }
        /** @brief Returns the specified half-precision float value as a 16-bit signed integer bit pattern. */
        [[nodiscard]] static shortcs HalfToInt16Bits  (Half value)   { return static_cast<shortcs>(value.bits); }
        /** @brief Returns a half-precision float converted from the specified 16-bit signed integer bit pattern. */
        [[nodiscard]] static Half    Int16BitsToHalf  (shortcs value) { return Half{static_cast<uint16_t>(value)}; }
        /** @brief Returns the specified half-precision float value as a 16-bit unsigned integer bit pattern. */
        [[nodiscard]] static ushortcs HalfToUInt16Bits(Half value)   { return value.bits; }
        /** @brief Returns a half-precision float converted from the specified 16-bit unsigned integer bit pattern. */
        [[nodiscard]] static Half    UInt16BitsToHalf (ushortcs value){ return Half{value}; }

        // -----------------------------------------------------------------------
        // ToString — bytes → hex string
        // -----------------------------------------------------------------------

        /**
         * @brief Converts the value of an array of bytes to its equivalent string
         * representation in hexadecimal, separated by hyphens.
         *
         * C++ counterpart of .NET BitConverter.ToString(byte[], int, int).
         */
        [[nodiscard]] static std::string ToString(const bytecs* value, intcs startIndex, intcs length);

        /**
         * @brief Converts bytes from startIndex to end of vector to a hex string.
         *
         * C++ counterpart of .NET BitConverter.ToString(byte[], int).
         */
        [[nodiscard]] static std::string ToString(const std::vector<bytecs>& value, intcs startIndex);

        /**
         * @brief Converts an entire byte vector to a hyphen-separated hex string.
         *
         * C++ counterpart of .NET BitConverter.ToString(byte[]).
         */
        [[nodiscard]] static std::string ToString(const std::vector<bytecs>& value);
    };

} // namespace System
