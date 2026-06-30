// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Convert.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

#include <stdexcept>
#include <cstdlib>
#include <cerrno>
#include <array>
#include <charconv>
#include <climits>
#include <limits>
#include <sstream>
#include <iomanip>
#include <bitset>

namespace System {

    using SharpRuntime::ushortcs;
    using SharpRuntime::sbytecs;

    namespace {
        intcs parseIntBase(const std::string& value, int base) {
            if (value.empty()) throw FormatException("Input string was not in a correct format.");
            errno = 0;
            char* end = nullptr;
            long long result = std::strtoll(value.c_str(), &end, base);
            if (end == value.c_str() || *end != '\0')
                throw FormatException("Input string was not in a correct format.");
            if (errno == ERANGE || result > INT32_MAX || result < INT32_MIN)
                throw OverflowException("Value was either too large or too small for an Int32.");
            return static_cast<intcs>(result);
        }
    }

    intcs Convert::ToInt32(const std::string& value) {
        return parseIntBase(value, 10);
    }
    intcs Convert::ToInt32(double value) {
        if (value > INT_MAX || value < INT_MIN) throw OverflowException();
        return static_cast<intcs>(value);
    }
    intcs Convert::ToInt32(float value) {
        return ToInt32(static_cast<double>(value));
    }
    intcs Convert::ToInt32(longcs value) {
        if (value > INT_MAX || value < INT_MIN) throw OverflowException();
        return static_cast<intcs>(value);
    }
    intcs Convert::ToInt32(const std::string& value, int fromBase) {
        return parseIntBase(value, fromBase);
    }

    longcs Convert::ToInt64(const std::string& value) {
        if (value.empty()) throw FormatException();
        errno = 0;
        char* end = nullptr;
        long long result = std::strtoll(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0') throw FormatException();
        if (errno == ERANGE) throw OverflowException();
        return static_cast<longcs>(result);
    }
    longcs Convert::ToInt64(double value) { return static_cast<longcs>(value); }

    shortcs Convert::ToInt16(const std::string& value) {
        intcs v = ToInt32(value);
        if (v > 32767 || v < -32768) throw OverflowException();
        return static_cast<shortcs>(v);
    }
    shortcs Convert::ToInt16(intcs value) {
        if (value > 32767 || value < -32768) throw OverflowException();
        return static_cast<shortcs>(value);
    }
    shortcs Convert::ToInt16(longcs value) {
        if (value > 32767 || value < -32768) throw OverflowException();
        return static_cast<shortcs>(value);
    }

    double Convert::ToDouble(const std::string& value) {
        if (value.empty()) throw FormatException();
        // Handle .NET special string representations
        if (value == "NaN")       return std::numeric_limits<double>::quiet_NaN();
        if (value == "Infinity")  return  std::numeric_limits<double>::infinity();
        if (value == "-Infinity") return -std::numeric_limits<double>::infinity();
        double result{};
        // from_chars is locale-independent (always uses '.' as decimal separator)
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (ec == std::errc::invalid_argument || ptr != value.data() + value.size())
            throw FormatException();
        if (ec == std::errc::result_out_of_range)
            throw OverflowException();
        return result;
    }

    Single Convert::ToSingle(const std::string& value) {
        return static_cast<Single>(ToDouble(value));
    }

    bytecs Convert::ToByte(intcs value) {
        if (value < 0 || value > 255) throw OverflowException();
        return static_cast<bytecs>(value);
    }
    bytecs Convert::ToByte(const std::string& value) {
        return ToByte(ToInt32(value));
    }

    bool Convert::ToBoolean(const std::string& value) {
        if (value == "True"  || value == "true"  || value == "1") return true;
        if (value == "False" || value == "false" || value == "0") return false;
        throw FormatException("String was not recognized as a valid Boolean.");
    }

    std::string Convert::ToString(intcs value)   { return std::to_string(value); }
    std::string Convert::ToString(longcs value)  { return std::to_string(value); }
    std::string Convert::ToString(double value) {
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        return ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value);
    }
    std::string Convert::ToString(float value) {
        std::array<char, 32> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        return ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value);
    }
    std::string Convert::ToString(char value)    { return std::string(1, value); }
    std::string Convert::ToString(bytecs value)  { return std::to_string(static_cast<int>(value)); }

    std::string Convert::ToString(intcs value, int toBase) {
        if (toBase == 10) return std::to_string(value);
        if (toBase == 16) {
            std::ostringstream oss;
            oss << std::hex << value;
            return oss.str();
        }
        if (toBase == 2) {
            if (value == 0) return "0";
            std::string result;
            auto uv = static_cast<uint32_t>(value);
            while (uv) { result = (char)('0' + (uv & 1)) + result; uv >>= 1; }
            return result;
        }
        if (toBase == 8) {
            std::ostringstream oss;
            oss << std::oct << value;
            return oss.str();
        }
        throw FormatException("Invalid base.");
    }

    std::string Convert::ToHexString(const std::vector<bytecs>& inArray)
    {
        static constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(inArray.size() * 2);
        for (bytecs b : inArray) {
            result += hex[(b >> 4) & 0xF];
            result += hex[b & 0xF];
        }
        return result;
    }

    std::string Convert::ToHexStringUpper(const std::vector<bytecs>& inArray)
    {
        static constexpr char hex[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(inArray.size() * 2);
        for (bytecs b : inArray) {
            result += hex[(b >> 4) & 0xF];
            result += hex[b & 0xF];
        }
        return result;
    }

    std::vector<SharpRuntime::bytecs> Convert::FromHexString(const std::string& s)
    {
        if (s.size() % 2 != 0)
            throw FormatException("Hex string length must be even.");
        std::vector<bytecs> result;
        result.reserve(s.size() / 2);
        for (size_t i = 0; i < s.size(); i += 2) {
            auto hexVal = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
                if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
                throw FormatException("Invalid hex character.");
                return 0;
            };
            result.push_back(static_cast<bytecs>((hexVal(s[i]) << 4) | hexVal(s[i + 1])));
        }
        return result;
    }

    ushortcs Convert::ToUInt16(const std::string& value)
    {
        intcs v = ToInt32(value);
        if (v < 0 || v > 65535) throw OverflowException("Value is out of UInt16 range.");
        return static_cast<ushortcs>(v);
    }

    sbytecs Convert::ToSByte(const std::string& value)
    {
        intcs v = ToInt32(value);
        if (v < -128 || v > 127) throw OverflowException("Value is out of SByte range.");
        return static_cast<sbytecs>(v);
    }

    uint32_t Convert::ToUInt32(const std::string& value)
    {
        unsigned long r = std::stoul(value);
        return static_cast<uint32_t>(r);
    }

    uint64_t Convert::ToUInt64(const std::string& value)
    {
        unsigned long long r = std::stoull(value);
        return static_cast<uint64_t>(r);
    }

    static constexpr char kB64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string Convert::ToBase64String(const std::vector<bytecs>& inArray)
    {
        std::string out;
        size_t n = inArray.size();
        out.reserve(((n + 2) / 3) * 4);
        for (size_t i = 0; i < n; i += 3) {
            uint32_t triple = static_cast<uint32_t>(inArray[i]) << 16;
            if (i + 1 < n) triple |= static_cast<uint32_t>(inArray[i + 1]) << 8;
            if (i + 2 < n) triple |= static_cast<uint32_t>(inArray[i + 2]);
            out += kB64Chars[(triple >> 18) & 0x3F];
            out += kB64Chars[(triple >> 12) & 0x3F];
            out += (i + 1 < n) ? kB64Chars[(triple >> 6) & 0x3F] : '=';
            out += (i + 2 < n) ? kB64Chars[triple & 0x3F]        : '=';
        }
        return out;
    }

    std::vector<SharpRuntime::bytecs> Convert::FromBase64String(const std::string& s)
    {
        auto b64val = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            if (c == '=') return -1;
            throw FormatException("Invalid Base64 character.");
            return 0;
        };
        if (s.size() % 4 != 0) throw FormatException("Base64 string length must be a multiple of 4.");
        std::vector<bytecs> out;
        out.reserve(s.size() / 4 * 3);
        for (size_t i = 0; i < s.size(); i += 4) {
            int v0 = b64val(s[i]), v1 = b64val(s[i+1]);
            int v2 = b64val(s[i+2]), v3 = b64val(s[i+3]);
            out.push_back(static_cast<bytecs>((v0 << 2) | (v1 >> 4)));
            if (v2 != -1) out.push_back(static_cast<bytecs>(((v1 & 0xF) << 4) | (v2 >> 2)));
            if (v3 != -1) out.push_back(static_cast<bytecs>(((v2 & 0x3) << 6) | v3));
        }
        return out;
    }

} // namespace System
