// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Convert.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

#include <stdexcept>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <sstream>
#include <iomanip>
#include <bitset>

namespace System {

    namespace {
        intcs parseIntBase(const std::string& value, int base) {
            if (value.empty()) throw FormatException("Input string was not in a correct format.");
            errno = 0;
            char* end = nullptr;
            long result = std::strtol(value.c_str(), &end, base);
            if (end == value.c_str() || *end != '\0')
                throw FormatException("Input string was not in a correct format.");
            if (errno == ERANGE || result > INT_MAX || result < INT_MIN)
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

    double Convert::ToDouble(const std::string& value) {
        if (value.empty()) throw FormatException();
        errno = 0;
        char* end = nullptr;
        double result = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0') throw FormatException();
        if (errno == ERANGE) throw OverflowException();
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
    std::string Convert::ToString(double value)  { return std::to_string(value); }
    std::string Convert::ToString(float value)   { return std::to_string(value); }
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

} // namespace System
