// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cctype>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Buffers/OperationStatus.hpp"

namespace System::Text {

using SharpRuntime::bytecs;
using SharpRuntime::charcs;
using SharpRuntime::intcs;

/// <summary>Provides static methods for working with ASCII characters and text.</summary>
class Ascii {
public:
    Ascii() = delete;

    // --- IsValid ---

    static bool IsValid(bytecs value) { return value <= 127; }
    static bool IsValid(charcs value) { return value <= 127; }

    static bool IsValid(const std::vector<bytecs>& value) {
        for (bytecs b : value) if (b > 127) return false;
        return true;
    }

    static bool IsValid(const std::u16string& value) {
        for (char16_t c : value) if (c > 127) return false;
        return true;
    }

    static bool IsValid(const std::string& value) {
        for (unsigned char c : value) if (c > 127) return false;
        return true;
    }

    // --- Equals ---

    static bool Equals(const std::vector<bytecs>& left, const std::vector<bytecs>& right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i)
            if (left[i] != right[i]) return false;
        return true;
    }

    static bool EqualsIgnoreCase(const std::vector<bytecs>& left, const std::vector<bytecs>& right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i)
            if (std::tolower(left[i]) != std::tolower(right[i])) return false;
        return true;
    }

    static bool EqualsIgnoreCase(const std::string& left, const std::string& right) {
        if (left.size() != right.size()) return false;
        for (size_t i = 0; i < left.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(left[i])) !=
                std::tolower(static_cast<unsigned char>(right[i]))) return false;
        return true;
    }

    // --- ToUpper / ToLower in-place ---

    static System::Buffers::OperationStatus ToUpperInPlace(std::vector<bytecs>& value, intcs& bytesWritten) {
        for (bytecs& b : value)
            b = static_cast<bytecs>(std::toupper(b));
        bytesWritten = static_cast<intcs>(value.size());
        return System::Buffers::OperationStatus::Done;
    }

    static System::Buffers::OperationStatus ToLowerInPlace(std::vector<bytecs>& value, intcs& bytesWritten) {
        for (bytecs& b : value)
            b = static_cast<bytecs>(std::tolower(b));
        bytesWritten = static_cast<intcs>(value.size());
        return System::Buffers::OperationStatus::Done;
    }

    static System::Buffers::OperationStatus ToUpperInPlace(std::string& value, intcs& charsWritten) {
        for (char& c : value)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        charsWritten = static_cast<intcs>(value.size());
        return System::Buffers::OperationStatus::Done;
    }

    static System::Buffers::OperationStatus ToLowerInPlace(std::string& value, intcs& charsWritten) {
        for (char& c : value)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        charsWritten = static_cast<intcs>(value.size());
        return System::Buffers::OperationStatus::Done;
    }

    // --- ToUpper / ToLower with destination ---

    static System::Buffers::OperationStatus ToUpper(const std::vector<bytecs>& source,
                                                    std::vector<bytecs>& destination,
                                                    intcs& bytesWritten) {
        size_t n = std::min(source.size(), destination.size());
        for (size_t i = 0; i < n; ++i)
            destination[i] = static_cast<bytecs>(std::toupper(source[i]));
        bytesWritten = static_cast<intcs>(n);
        return (n == source.size()) ? System::Buffers::OperationStatus::Done
                                    : System::Buffers::OperationStatus::DestinationTooSmall;
    }

    static System::Buffers::OperationStatus ToLower(const std::vector<bytecs>& source,
                                                    std::vector<bytecs>& destination,
                                                    intcs& bytesWritten) {
        size_t n = std::min(source.size(), destination.size());
        for (size_t i = 0; i < n; ++i)
            destination[i] = static_cast<bytecs>(std::tolower(source[i]));
        bytesWritten = static_cast<intcs>(n);
        return (n == source.size()) ? System::Buffers::OperationStatus::Done
                                    : System::Buffers::OperationStatus::DestinationTooSmall;
    }

    // --- Trim helpers (return trimmed string) ---

    static std::string Trim(const std::string& value) {
        size_t s = 0, e = value.size();
        while (s < e && value[s] <= 32) ++s;
        while (e > s && value[e - 1] <= 32) --e;
        return value.substr(s, e - s);
    }

    static std::string TrimStart(const std::string& value) {
        size_t s = 0;
        while (s < value.size() && value[s] <= 32) ++s;
        return value.substr(s);
    }

    static std::string TrimEnd(const std::string& value) {
        size_t e = value.size();
        while (e > 0 && value[e - 1] <= 32) --e;
        return value.substr(0, e);
    }
};

} // namespace System::Text
