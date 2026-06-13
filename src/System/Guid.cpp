// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Guid.hpp"
#include "System/FormatException.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace System {

    const Guid Guid::Empty{};

    Guid::Guid() : bytes_{} {}

    Guid::Guid(const std::array<uint8_t, 16>& bytes) : bytes_(bytes) {}

    Guid::Guid(const std::string& guidString) : bytes_{} {
        std::string s = guidString;
        // strip braces
        if (!s.empty() && (s.front() == '{' || s.front() == '(')) {
            s = s.substr(1, s.size() - 2);
        }
        // remove dashes
        s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
        if (s.size() != 32)
            throw FormatException("Unrecognized Guid format.");
        for (int i = 0; i < 16; ++i) {
            unsigned int byte = 0;
            std::istringstream ss(s.substr(i * 2, 2));
            ss >> std::hex >> byte;
            bytes_[i] = static_cast<uint8_t>(byte);
        }
    }

    Guid Guid::NewGuid() {
        static std::mt19937_64 rng{std::random_device{}()};
        static std::uniform_int_distribution<uint32_t> dist(0, 255);

        std::array<uint8_t, 16> b;
        for (auto& byte : b) byte = static_cast<uint8_t>(dist(rng));
        // RFC 4122 version 4
        b[6] = (b[6] & 0x0F) | 0x40;
        b[8] = (b[8] & 0x3F) | 0x80;
        return Guid(b);
    }

    Guid Guid::Parse(const std::string& s) {
        return Guid(s); // constructor already validates and throws FormatException
    }

    bool Guid::TryParse(const std::string& s, Guid& result) {
        try {
            result = Guid(s);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string Guid::ToString() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) oss << '-';
            oss << std::setw(2) << static_cast<unsigned>(bytes_[i]);
        }
        return oss.str();
    }

    std::string Guid::ToString(const std::string& format) const {
        std::string d = ToString(); // "D" format (default with hyphens)
        if (format == "D" || format.empty()) return d;
        if (format == "N") {
            std::string n = d;
            n.erase(std::remove(n.begin(), n.end(), '-'), n.end());
            return n;
        }
        if (format == "B") return "{" + d + "}";
        if (format == "P") return "(" + d + ")";
        throw FormatException("Invalid Guid format specifier.");
    }

} // namespace System
