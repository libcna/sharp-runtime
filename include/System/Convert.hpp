// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::bytecs;
    using SharpRuntime::Single;
    using SharpRuntime::sbytecs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::uintcs;
    using SharpRuntime::ulongcs;

    /**
     * @brief Converts a base data type to another base data type.
     *
     * C++ counterpart of .NET System.Convert.
     * All members are static; the class cannot be instantiated.
     */
    class Convert {
    public:
        /** @brief Deleted constructor — all members are static. */
        Convert() = delete;

        // ===================================================================
        // ToBoolean
        // ===================================================================

        /** @brief Returns the same Boolean value. */
        [[nodiscard]] static bool ToBoolean(bool value)          { return value; }
        /** @brief Converts a 32-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(intcs value)         { return value != 0; }
        /** @brief Converts a 16-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(shortcs value)       { return value != 0; }
        /** @brief Converts a byte to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(bytecs value)        { return value != 0; }
        /** @brief Converts a 64-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(longcs value)        { return value != 0; }
        /** @brief Converts a double to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(double value)        { return value != 0.0; }
        /** @brief Converts a float to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(float value)         { return value != 0.0f; }
        /** @brief Converts an unsigned 32-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(uintcs value)        { return value != 0u; }
        /** @brief Converts an unsigned 64-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(ulongcs value)       { return value != 0ull; }
        /** @brief Converts an unsigned 16-bit integer to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(ushortcs value)      { return value != 0; }
        /** @brief Converts a signed byte to Boolean (non-zero → true). */
        [[nodiscard]] static bool ToBoolean(sbytecs value)       { return value != 0; }
        /**
         * @brief Converts a string to Boolean.
         *
         * C++ counterpart of .NET Convert.ToBoolean(string).
         * Accepts "True"/"true"/"1" → true and "False"/"false"/"0" → false.
         * @throws FormatException for any other string.
         */
        [[nodiscard]] static bool ToBoolean(const std::string& value);
        /** @brief Converts a C-string to Boolean. Delegates to the std::string overload. */
        [[nodiscard]] static bool ToBoolean(const char* value) { return ToBoolean(std::string(value)); }

        // ===================================================================
        // ToChar
        // ===================================================================

        /** @brief Returns the same char. */
        [[nodiscard]] static char ToChar(char value)   { return value; }
        /**
         * @brief Converts a 32-bit integer to char.
         *
         * C++ counterpart of .NET Convert.ToChar(int).
         */
        [[nodiscard]] static char ToChar(intcs value)  { return static_cast<char>(value); }
        /**
         * @brief Converts a 64-bit integer to char.
         *
         * @throws OverflowException if the value is outside the char range.
         */
        [[nodiscard]] static char ToChar(longcs value)
        {
            if (value < 0 || value > 255)
                throw OverflowException("Value is out of char range.");
            return static_cast<char>(value);
        }
        /**
         * @brief Converts a single-character string to char.
         *
         * C++ counterpart of .NET Convert.ToChar(string).
         * @throws FormatException if the string is not exactly one character.
         */
        [[nodiscard]] static char ToChar(const std::string& value)
        {
            if (value.size() != 1)
                throw FormatException("String must be exactly one character long.");
            return value[0];
        }

        // ===================================================================
        // ToByte
        // ===================================================================

        /** @brief Returns the same byte value. */
        [[nodiscard]] static bytecs ToByte(bytecs value)   { return value; }
        /**
         * @brief Converts a 32-bit integer to byte.
         *
         * C++ counterpart of .NET Convert.ToByte(int).
         * @throws OverflowException if out of range [0, 255].
         */
        [[nodiscard]] static bytecs ToByte(intcs value);
        /**
         * @brief Converts a 16-bit integer to byte.
         *
         * @throws OverflowException if out of range [0, 255].
         */
        [[nodiscard]] static bytecs ToByte(shortcs value)
        {
            if (value < 0 || value > 255)
                throw OverflowException("Value is out of Byte range.");
            return static_cast<bytecs>(value);
        }
        /**
         * @brief Converts an unsigned 32-bit integer to byte.
         *
         * @throws OverflowException if greater than 255.
         */
        [[nodiscard]] static bytecs ToByte(uint32_t value)
        {
            if (value > 255u)
                throw OverflowException("Value is out of Byte range.");
            return static_cast<bytecs>(value);
        }
        /** @brief Converts a 64-bit integer to byte. */
        [[nodiscard]] static bytecs ToByte(longcs value)   { return static_cast<bytecs>(value); }
        /** @brief Converts a Boolean to byte (true → 1, false → 0). */
        [[nodiscard]] static bytecs ToByte(bool value)     { return value ? bytecs(1) : bytecs(0); }
        /** @brief Converts a double to byte (truncates). */
        [[nodiscard]] static bytecs ToByte(double value)   { return static_cast<bytecs>(value); }
        /** @brief Converts a float to byte (truncates). */
        [[nodiscard]] static bytecs ToByte(float value)    { return static_cast<bytecs>(value); }
        /**
         * @brief Converts a string to byte.
         *
         * C++ counterpart of .NET Convert.ToByte(string).
         * @throws OverflowException if out of range [0, 255].
         */
        [[nodiscard]] static bytecs ToByte(const std::string& value);

        // ===================================================================
        // ToInt16
        // ===================================================================

        /** @brief Returns the same 16-bit integer. */
        [[nodiscard]] static shortcs ToInt16(shortcs value)  { return value; }
        /**
         * @brief Converts a 32-bit integer to 16-bit integer.
         *
         * @throws OverflowException if outside [-32768, 32767].
         */
        [[nodiscard]] static shortcs ToInt16(intcs value);
        /**
         * @brief Converts a 64-bit integer to 16-bit integer.
         *
         * @throws OverflowException if outside [-32768, 32767].
         */
        [[nodiscard]] static shortcs ToInt16(longcs value);
        /** @brief Converts a double to 16-bit integer (truncates). */
        [[nodiscard]] static shortcs ToInt16(double value)   { return static_cast<shortcs>(value); }
        /** @brief Converts a float to 16-bit integer (truncates). */
        [[nodiscard]] static shortcs ToInt16(float value)    { return static_cast<shortcs>(value); }
        /** @brief Converts a Boolean to 16-bit integer (true → 1, false → 0). */
        [[nodiscard]] static shortcs ToInt16(bool value)     { return value ? shortcs(1) : shortcs(0); }
        /** @brief Converts a byte to 16-bit integer. */
        [[nodiscard]] static shortcs ToInt16(bytecs value)   { return static_cast<shortcs>(value); }
        /**
         * @brief Converts a string to 16-bit integer.
         *
         * @throws OverflowException if outside [-32768, 32767].
         */
        [[nodiscard]] static shortcs ToInt16(const std::string& value);

        // ===================================================================
        // ToInt32
        // ===================================================================

        /** @brief Returns the same 32-bit integer. */
        [[nodiscard]] static intcs ToInt32(intcs value)    { return value; }
        /** @brief Converts a 16-bit integer to 32-bit integer. */
        [[nodiscard]] static intcs ToInt32(shortcs value)  { return static_cast<intcs>(value); }
        /** @brief Converts a byte to 32-bit integer. */
        [[nodiscard]] static intcs ToInt32(bytecs value)   { return static_cast<intcs>(value); }
        /** @brief Converts a Boolean to 32-bit integer (true → 1, false → 0). */
        [[nodiscard]] static intcs ToInt32(bool value)     { return value ? 1 : 0; }
        /**
         * @brief Converts a 64-bit integer to 32-bit integer.
         *
         * @throws OverflowException if outside [-2147483648, 2147483647].
         */
        [[nodiscard]] static intcs ToInt32(longcs value);
        /**
         * @brief Converts a double to 32-bit integer (truncates).
         *
         * @throws OverflowException if outside Int32 range.
         */
        [[nodiscard]] static intcs ToInt32(double value);
        /** @brief Converts a float to 32-bit integer (truncates). */
        [[nodiscard]] static intcs ToInt32(float value);
        /**
         * @brief Converts a string to 32-bit integer.
         *
         * @throws FormatException if the string is not a valid integer.
         */
        [[nodiscard]] static intcs ToInt32(const std::string& value);
        /**
         * @brief Converts a string in the given base to a 32-bit integer.
         *
         * C++ counterpart of .NET Convert.ToInt32(string, int).
         * @param fromBase The base of the number (2, 8, 10, or 16).
         */
        [[nodiscard]] static intcs ToInt32(const std::string& value, int fromBase);

        // ===================================================================
        // ToInt64
        // ===================================================================

        /** @brief Returns the same 64-bit integer. */
        [[nodiscard]] static longcs ToInt64(longcs value)     { return value; }
        /** @brief Converts a 32-bit integer to 64-bit integer. */
        [[nodiscard]] static longcs ToInt64(intcs value)      { return static_cast<longcs>(value); }
        /** @brief Converts a 16-bit integer to 64-bit integer. */
        [[nodiscard]] static longcs ToInt64(shortcs value)    { return static_cast<longcs>(value); }
        /** @brief Converts a byte to 64-bit integer. */
        [[nodiscard]] static longcs ToInt64(bytecs value)     { return static_cast<longcs>(value); }
        /** @brief Converts an unsigned 32-bit integer to 64-bit integer. */
        [[nodiscard]] static longcs ToInt64(uint32_t value)   { return static_cast<longcs>(value); }
        /**
         * @brief Converts an unsigned 64-bit integer to 64-bit integer.
         *
         * @throws OverflowException if greater than Int64.MaxValue.
         */
        [[nodiscard]] static longcs ToInt64(uint64_t value)
        {
            if (value > static_cast<uint64_t>(std::numeric_limits<longcs>::max()))
                throw OverflowException("Value is out of Int64 range.");
            return static_cast<longcs>(value);
        }
        /** @brief Converts a double to 64-bit integer (truncates). */
        [[nodiscard]] static longcs ToInt64(double value);
        /** @brief Converts a float to 64-bit integer (truncates). */
        [[nodiscard]] static longcs ToInt64(float value)      { return static_cast<longcs>(value); }
        /** @brief Converts a Boolean to 64-bit integer (true → 1, false → 0). */
        [[nodiscard]] static longcs ToInt64(bool value)       { return value ? 1LL : 0LL; }
        /**
         * @brief Converts a string to 64-bit integer.
         *
         * @throws FormatException if the string is not a valid integer.
         */
        [[nodiscard]] static longcs ToInt64(const std::string& value);

        // ===================================================================
        // ToDouble
        // ===================================================================

        /** @brief Returns the same double. */
        [[nodiscard]] static double ToDouble(double value)    { return value; }
        /** @brief Converts a float to double. */
        [[nodiscard]] static double ToDouble(float value)     { return static_cast<double>(value); }
        /** @brief Converts a 32-bit integer to double. */
        [[nodiscard]] static double ToDouble(intcs value)     { return static_cast<double>(value); }
        /** @brief Converts a 16-bit integer to double. */
        [[nodiscard]] static double ToDouble(shortcs value)   { return static_cast<double>(value); }
        /** @brief Converts a byte to double. */
        [[nodiscard]] static double ToDouble(bytecs value)    { return static_cast<double>(value); }
        /** @brief Converts a 64-bit integer to double. */
        [[nodiscard]] static double ToDouble(longcs value)    { return static_cast<double>(value); }
        /** @brief Converts a Boolean to double (true → 1.0, false → 0.0). */
        [[nodiscard]] static double ToDouble(bool value)      { return value ? 1.0 : 0.0; }
        /**
         * @brief Converts a string to double.
         *
         * @throws FormatException if the string is not a valid number.
         */
        [[nodiscard]] static double ToDouble(const std::string& value);

        // ===================================================================
        // ToSingle
        // ===================================================================

        /** @brief Returns the same float. */
        [[nodiscard]] static Single ToSingle(float value)     { return value; }
        /** @brief Converts a double to float. */
        [[nodiscard]] static Single ToSingle(double value)    { return static_cast<Single>(value); }
        /** @brief Converts a 32-bit integer to float. */
        [[nodiscard]] static Single ToSingle(intcs value)     { return static_cast<Single>(value); }
        /** @brief Converts a 16-bit integer to float. */
        [[nodiscard]] static Single ToSingle(shortcs value)   { return static_cast<Single>(value); }
        /** @brief Converts a byte to float. */
        [[nodiscard]] static Single ToSingle(bytecs value)    { return static_cast<Single>(value); }
        /** @brief Converts a 64-bit integer to float. */
        [[nodiscard]] static Single ToSingle(longcs value)    { return static_cast<Single>(value); }
        /** @brief Converts a Boolean to float (true → 1.0f, false → 0.0f). */
        [[nodiscard]] static Single ToSingle(bool value)      { return value ? 1.0f : 0.0f; }
        /**
         * @brief Converts a string to float.
         *
         * @throws FormatException if the string is not a valid number.
         */
        [[nodiscard]] static Single ToSingle(const std::string& value);

        // ===================================================================
        // ToUInt32
        // ===================================================================

        /** @brief Converts a 32-bit integer to unsigned 32-bit integer. */
        [[nodiscard]] static uint32_t ToUInt32(intcs value)   { return static_cast<uint32_t>(value); }
        /** @brief Converts a 64-bit integer to unsigned 32-bit integer. */
        [[nodiscard]] static uint32_t ToUInt32(longcs value)  { return static_cast<uint32_t>(value); }
        /** @brief Converts a byte to unsigned 32-bit integer. */
        [[nodiscard]] static uint32_t ToUInt32(bytecs value)  { return static_cast<uint32_t>(value); }
        /** @brief Converts a Boolean to unsigned 32-bit integer (true → 1, false → 0). */
        [[nodiscard]] static uint32_t ToUInt32(bool value)    { return value ? 1u : 0u; }
        /**
         * @brief Converts a double to unsigned 32-bit integer (truncates).
         *
         * @throws OverflowException if out of range.
         */
        [[nodiscard]] static uint32_t ToUInt32(double value)
        {
            if (value < 0.0 || value > static_cast<double>(std::numeric_limits<uint32_t>::max()))
                throw OverflowException("Value is out of UInt32 range.");
            return static_cast<uint32_t>(value);
        }
        /**
         * @brief Converts a string to unsigned 32-bit integer.
         *
         * @throws FormatException if the string is not a valid unsigned integer.
         */
        [[nodiscard]] static uint32_t ToUInt32(const std::string& value);

        // ===================================================================
        // ToUInt64
        // ===================================================================

        /** @brief Converts a 32-bit integer to unsigned 64-bit integer. */
        [[nodiscard]] static uint64_t ToUInt64(intcs value)   { return static_cast<uint64_t>(value); }
        /** @brief Converts a 64-bit integer to unsigned 64-bit integer. */
        [[nodiscard]] static uint64_t ToUInt64(longcs value)  { return static_cast<uint64_t>(value); }
        /** @brief Converts a byte to unsigned 64-bit integer. */
        [[nodiscard]] static uint64_t ToUInt64(bytecs value)  { return static_cast<uint64_t>(value); }
        /** @brief Converts a Boolean to unsigned 64-bit integer (true → 1, false → 0). */
        [[nodiscard]] static uint64_t ToUInt64(bool value)    { return value ? 1ull : 0ull; }
        /**
         * @brief Converts a double to unsigned 64-bit integer (truncates).
         *
         * @throws OverflowException if out of range.
         */
        [[nodiscard]] static uint64_t ToUInt64(double value)
        {
            if (value < 0.0 || value > static_cast<double>(std::numeric_limits<uint64_t>::max()))
                throw OverflowException("Value is out of UInt64 range.");
            return static_cast<uint64_t>(value);
        }
        /**
         * @brief Converts a string to unsigned 64-bit integer.
         *
         * @throws FormatException if the string is not a valid unsigned integer.
         */
        [[nodiscard]] static uint64_t ToUInt64(const std::string& value);

        // ===================================================================
        // ToUInt16
        // ===================================================================

        /** @brief Returns the same unsigned 16-bit integer. */
        [[nodiscard]] static ushortcs ToUInt16(ushortcs value)  { return value; }
        /** @brief Converts a byte to unsigned 16-bit integer. */
        [[nodiscard]] static ushortcs ToUInt16(bytecs value)    { return static_cast<ushortcs>(value); }
        /** @brief Converts a Boolean to unsigned 16-bit integer (true → 1, false → 0). */
        [[nodiscard]] static ushortcs ToUInt16(bool value)      { return value ? ushortcs(1) : ushortcs(0); }
        /**
         * @brief Converts a 32-bit integer to unsigned 16-bit integer.
         *
         * @throws OverflowException if outside [0, 65535].
         */
        [[nodiscard]] static ushortcs ToUInt16(intcs value)
        {
            if (value < 0 || value > 65535)
                throw OverflowException("Value is out of UInt16 range.");
            return static_cast<ushortcs>(value);
        }
        /**
         * @brief Converts a 16-bit signed integer to unsigned 16-bit integer.
         *
         * @throws OverflowException if negative.
         */
        [[nodiscard]] static ushortcs ToUInt16(shortcs value)
        {
            if (value < 0)
                throw OverflowException("Value is out of UInt16 range.");
            return static_cast<ushortcs>(value);
        }
        /**
         * @brief Converts a 64-bit integer to unsigned 16-bit integer.
         *
         * @throws OverflowException if outside [0, 65535].
         */
        [[nodiscard]] static ushortcs ToUInt16(longcs value)
        {
            if (value < 0 || value > 65535)
                throw OverflowException("Value is out of UInt16 range.");
            return static_cast<ushortcs>(value);
        }
        /**
         * @brief Converts a double to unsigned 16-bit integer (truncates).
         *
         * @throws OverflowException if outside [0, 65535].
         */
        [[nodiscard]] static ushortcs ToUInt16(double value)
        {
            if (value < 0.0 || value > 65535.0)
                throw OverflowException("Value is out of UInt16 range.");
            return static_cast<ushortcs>(value);
        }
        /**
         * @brief Converts a string to unsigned 16-bit integer.
         *
         * @throws OverflowException if outside [0, 65535].
         */
        [[nodiscard]] static ushortcs ToUInt16(const std::string& value);

        // ===================================================================
        // ToSByte
        // ===================================================================

        /** @brief Returns the same signed byte. */
        [[nodiscard]] static sbytecs ToSByte(sbytecs value)  { return value; }
        /** @brief Converts a Boolean to signed byte (true → 1, false → 0). */
        [[nodiscard]] static sbytecs ToSByte(bool value)     { return value ? sbytecs(1) : sbytecs(0); }
        /**
         * @brief Converts a byte to signed byte.
         *
         * @throws OverflowException if greater than 127.
         */
        [[nodiscard]] static sbytecs ToSByte(bytecs value)
        {
            if (value > 127u)
                throw OverflowException("Value is out of SByte range.");
            return static_cast<sbytecs>(value);
        }
        /**
         * @brief Converts a 16-bit integer to signed byte.
         *
         * @throws OverflowException if outside [-128, 127].
         */
        [[nodiscard]] static sbytecs ToSByte(shortcs value)
        {
            if (value < -128 || value > 127)
                throw OverflowException("Value is out of SByte range.");
            return static_cast<sbytecs>(value);
        }
        /**
         * @brief Converts a 32-bit integer to signed byte.
         *
         * @throws OverflowException if outside [-128, 127].
         */
        [[nodiscard]] static sbytecs ToSByte(intcs value)
        {
            if (value < -128 || value > 127)
                throw OverflowException("Value is out of SByte range.");
            return static_cast<sbytecs>(value);
        }
        /**
         * @brief Converts a 64-bit integer to signed byte.
         *
         * @throws OverflowException if outside [-128, 127].
         */
        [[nodiscard]] static sbytecs ToSByte(longcs value)
        {
            if (value < -128 || value > 127)
                throw OverflowException("Value is out of SByte range.");
            return static_cast<sbytecs>(value);
        }
        /**
         * @brief Converts a double to signed byte (truncates).
         *
         * @throws OverflowException if outside [-128, 127].
         */
        [[nodiscard]] static sbytecs ToSByte(double value)
        {
            if (value < -128.0 || value > 127.0)
                throw OverflowException("Value is out of SByte range.");
            return static_cast<sbytecs>(value);
        }
        /**
         * @brief Converts a string to signed byte.
         *
         * @throws OverflowException if outside [-128, 127].
         */
        [[nodiscard]] static sbytecs ToSByte(const std::string& value);

        // ===================================================================
        // ToString
        // ===================================================================

        /** @brief Converts a 32-bit integer to its string representation. */
        [[nodiscard]] static std::string ToString(intcs value);
        /** @brief Converts a 16-bit integer to its string representation. */
        [[nodiscard]] static std::string ToString(shortcs value)  { return std::to_string(value); }
        /** @brief Converts a byte to its string representation. */
        [[nodiscard]] static std::string ToString(bytecs value);
        /** @brief Converts a 64-bit integer to its string representation. */
        [[nodiscard]] static std::string ToString(longcs value);
        /** @brief Converts an unsigned 32-bit integer to its string representation. */
        [[nodiscard]] static std::string ToString(uint32_t value) { return std::to_string(value); }
        /** @brief Converts an unsigned 64-bit integer to its string representation. */
        [[nodiscard]] static std::string ToString(uint64_t value) { return std::to_string(value); }
        /** @brief Converts a double to its string representation. */
        [[nodiscard]] static std::string ToString(double value);
        /** @brief Converts a float to its string representation. */
        [[nodiscard]] static std::string ToString(float value);
        /**
         * @brief Converts a Boolean to its string representation.
         *
         * C++ counterpart of .NET Convert.ToString(bool).
         * Returns "True" or "False".
         */
        [[nodiscard]] static std::string ToString(bool value) { return value ? "True" : "False"; }
        /** @brief Converts a char to its string representation. */
        [[nodiscard]] static std::string ToString(char value);

        /**
         * @brief Converts a 32-bit integer to a string in the specified base.
         *
         * C++ counterpart of .NET Convert.ToString(int, int).
         * @param toBase The base (2, 8, 10, or 16).
         * @throws FormatException for an unsupported base.
         */
        [[nodiscard]] static std::string ToString(intcs value, int toBase);

        // ===================================================================
        // ToChar (additional)
        // ===================================================================

        // ===================================================================
        // Hex conversions
        // ===================================================================

        /**
         * @brief Converts a byte array to a lowercase hexadecimal string (no separators).
         *
         * C++ counterpart of .NET Convert.ToHexStringLower (lowercase variant).
         * Produces lowercase hex digits, e.g., "ff" for byte 0xFF.
         */
        [[nodiscard]] static std::string ToHexString(const std::vector<bytecs>& inArray);

        /**
         * @brief Converts a byte array to an uppercase hexadecimal string (no separators).
         *
         * C++ counterpart of .NET Convert.ToHexString (the default uppercase form).
         * Produces uppercase hex digits, e.g., "FF" for byte 0xFF.
         */
        [[nodiscard]] static std::string ToHexStringUpper(const std::vector<bytecs>& inArray);

        /**
         * @brief Converts a hexadecimal string (even number of hex digits) to a byte array.
         *
         * C++ counterpart of .NET Convert.FromHexString(string).
         * @throws FormatException if the length is odd or a non-hex character is found.
         */
        [[nodiscard]] static std::vector<bytecs> FromHexString(const std::string& s);

        // ===================================================================
        // Base64 conversions
        // ===================================================================

        /**
         * @brief Converts a byte array to its Base64-encoded string representation.
         *
         * C++ counterpart of .NET Convert.ToBase64String(byte[]).
         */
        [[nodiscard]] static std::string ToBase64String(const std::vector<bytecs>& inArray);

        /**
         * @brief Converts a Base64-encoded string to a byte array.
         *
         * C++ counterpart of .NET Convert.FromBase64String(string).
         * @throws FormatException if the string is not valid Base64.
         */
        [[nodiscard]] static std::vector<bytecs> FromBase64String(const std::string& s);
    };

} // namespace System
