// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::bytecs;
    using SharpRuntime::Single;

    /**
     * @brief Converts a base data type to another base data type.
     *
     * Partial C++ counterpart of .NET System.Convert.
     *
     * @note Status: Partial
     */
    class Convert {
    public:
        /// Deleted constructor; all members are static.
        Convert() = delete;

        // --- To numeric types ---
        /// Converts the specified string to a 32-bit integer.
        [[nodiscard]] static intcs    ToInt32(const std::string& value);
        /// Converts a Boolean to a 32-bit integer (true=1, false=0).
        [[nodiscard]] static intcs    ToInt32(bool value)   { return value ? 1 : 0; }
        /// Converts a double to a 32-bit integer.
        [[nodiscard]] static intcs    ToInt32(double value);
        /// Converts a float to a 32-bit integer.
        [[nodiscard]] static intcs    ToInt32(float value);
        /// Returns the given 32-bit integer unchanged.
        [[nodiscard]] static intcs    ToInt32(intcs value)  { return value; }
        /// Converts a 64-bit integer to a 32-bit integer.
        [[nodiscard]] static intcs    ToInt32(longcs value);
        /// Converts a byte to a 32-bit integer.
        [[nodiscard]] static intcs    ToInt32(bytecs value) { return static_cast<intcs>(value); }

        /// Converts the specified string to a 64-bit integer.
        [[nodiscard]] static longcs   ToInt64(const std::string& value);
        /// Converts a 32-bit integer to a 64-bit integer.
        [[nodiscard]] static longcs   ToInt64(intcs value)  { return static_cast<longcs>(value); }
        /// Converts a double to a 64-bit integer.
        [[nodiscard]] static longcs   ToInt64(double value);
        /// Converts a float to a 64-bit integer.
        [[nodiscard]] static longcs   ToInt64(float value)  { return static_cast<longcs>(value); }
        /// Converts a Boolean to a 64-bit integer (true=1, false=0).
        [[nodiscard]] static longcs   ToInt64(bool value)   { return value ? 1LL : 0LL; }

        /// Converts the specified string to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(const std::string& value);
        /// Converts a 32-bit integer to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(intcs value);
        /// Converts a 64-bit integer to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(longcs value);
        /// Converts a double to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(double value)  { return static_cast<shortcs>(value); }
        /// Converts a float to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(float value)   { return static_cast<shortcs>(value); }
        /// Converts a Boolean to a 16-bit integer (true=1, false=0).
        [[nodiscard]] static shortcs  ToInt16(bool value)    { return value ? shortcs(1) : shortcs(0); }
        /// Converts a byte to a 16-bit integer.
        [[nodiscard]] static shortcs  ToInt16(bytecs value)  { return static_cast<shortcs>(value); }

        /// Converts the specified string to a double.
        [[nodiscard]] static double ToDouble(const std::string& value);
        /// Converts a 32-bit integer to a double.
        [[nodiscard]] static double ToDouble(intcs value)  { return static_cast<double>(value); }
        /// Converts a 64-bit integer to a double.
        [[nodiscard]] static double ToDouble(longcs value) { return static_cast<double>(value); }
        /// Converts a float to a double.
        [[nodiscard]] static double ToDouble(float value)  { return static_cast<double>(value); }
        /// Converts a Boolean to a double (true=1.0, false=0.0).
        [[nodiscard]] static double ToDouble(bool value)   { return value ? 1.0 : 0.0; }

        /// Converts the specified string to a single-precision float.
        [[nodiscard]] static Single   ToSingle(const std::string& value);
        /// Converts a double to a single-precision float.
        [[nodiscard]] static Single   ToSingle(double value) { return static_cast<Single>(value); }
        /// Converts a 32-bit integer to a single-precision float.
        [[nodiscard]] static Single   ToSingle(intcs value)  { return static_cast<Single>(value); }
        /// Converts a 64-bit integer to a single-precision float.
        [[nodiscard]] static Single   ToSingle(longcs value) { return static_cast<Single>(value); }
        /// Converts a Boolean to a single-precision float (true=1.0f, false=0.0f).
        [[nodiscard]] static Single   ToSingle(bool value)   { return value ? 1.0f : 0.0f; }

        /// Converts a 32-bit integer to a byte.
        [[nodiscard]] static bytecs   ToByte(intcs value);
        /// Converts the specified string to a byte.
        [[nodiscard]] static bytecs   ToByte(const std::string& value);
        /// Converts a 64-bit integer to a byte.
        [[nodiscard]] static bytecs   ToByte(longcs value) { return static_cast<bytecs>(value); }
        /// Converts a Boolean to a byte (true=1, false=0).
        [[nodiscard]] static bytecs   ToByte(bool value)   { return value ? bytecs(1) : bytecs(0); }
        /// Converts a double to a byte.
        [[nodiscard]] static bytecs   ToByte(double value) { return static_cast<bytecs>(value); }
        /// Converts a float to a byte.
        [[nodiscard]] static bytecs   ToByte(float value)  { return static_cast<bytecs>(value); }

        /// Converts a 32-bit integer to a Boolean (non-zero is true).
        [[nodiscard]] static bool     ToBoolean(intcs value)            { return value != 0; }
        /// Converts the specified string to a Boolean.
        [[nodiscard]] static bool     ToBoolean(const std::string& value);
        /// Converts a 64-bit integer to a Boolean (non-zero is true).
        [[nodiscard]] static bool     ToBoolean(longcs value)           { return value != 0; }
        /// Converts a double to a Boolean (non-zero is true).
        [[nodiscard]] static bool     ToBoolean(double value)           { return value != 0.0; }
        /// Converts a float to a Boolean (non-zero is true).
        [[nodiscard]] static bool     ToBoolean(float value)            { return value != 0.0f; }

        /// Converts a 32-bit integer to an unsigned 32-bit integer.
        [[nodiscard]] static uint32_t ToUInt32(intcs value)  { return static_cast<uint32_t>(value); }
        /// Converts a 64-bit integer to an unsigned 32-bit integer.
        [[nodiscard]] static uint32_t ToUInt32(longcs value) { return static_cast<uint32_t>(value); }
        /// Converts the specified string to an unsigned 32-bit integer.
        [[nodiscard]] static uint32_t ToUInt32(const std::string& value);

        /// Converts a 32-bit integer to an unsigned 64-bit integer.
        [[nodiscard]] static uint64_t ToUInt64(intcs value)  { return static_cast<uint64_t>(value); }
        /// Converts a 64-bit integer to an unsigned 64-bit integer.
        [[nodiscard]] static uint64_t ToUInt64(longcs value) { return static_cast<uint64_t>(value); }
        /// Converts the specified string to an unsigned 64-bit integer.
        [[nodiscard]] static uint64_t ToUInt64(const std::string& value);

        /// Converts a 32-bit integer to a character (Unicode code point → char).
        [[nodiscard]] static char     ToChar(intcs value)    { return static_cast<char>(value); }

        // --- To string ---
        /// Converts a 32-bit integer to its string representation.
        [[nodiscard]] static std::string ToString(intcs value);
        /// Converts a 64-bit integer to its string representation.
        [[nodiscard]] static std::string ToString(longcs value);
        /// Converts a double to its string representation.
        [[nodiscard]] static std::string ToString(double value);
        /// Converts a float to its string representation.
        [[nodiscard]] static std::string ToString(float value);
        /// Converts a Boolean to its string representation ("True" or "False").
        [[nodiscard]] static std::string ToString(bool value) { return value ? "True" : "False"; }
        /// Converts a character to its string representation.
        [[nodiscard]] static std::string ToString(char value);
        /// Converts a byte to its string representation.
        [[nodiscard]] static std::string ToString(bytecs value);

        // --- Base-N conversions ---
        /// Converts a 32-bit integer to a string in the specified base.
        [[nodiscard]] static std::string ToString(intcs value, int toBase);
        /// Parses a string representation of a number in the given base to a 32-bit integer.
        [[nodiscard]] static intcs        ToInt32(const std::string& value, int fromBase);

        // --- Hex conversions ---
        /// Converts a byte array to a lowercase hexadecimal string (no separators).
        [[nodiscard]] static std::string ToHexString(const std::vector<bytecs>& inArray);
        /// Converts a hexadecimal string (even number of hex digits) to a byte array.
        [[nodiscard]] static std::vector<SharpRuntime::bytecs> FromHexString(const std::string& s);

        // --- Base64 conversions ---
        /// Converts a byte array to its Base64-encoded string representation.
        [[nodiscard]] static std::string ToBase64String(const std::vector<bytecs>& inArray);
        /// Converts a Base64-encoded string to a byte array.
        [[nodiscard]] static std::vector<bytecs> FromBase64String(const std::string& s);
    };

} // namespace System
