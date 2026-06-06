// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <cstdint>
#include <sstream>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net {

    using SharpRuntime::longcs;

    /**
     * @brief Provides an Internet Protocol (IP) address.
     *
     * Partial C++ counterpart of .NET System.Net.IPAddress.
     * Only IPv4 is currently handled.
     *
     * @note Status: Partial — IPv4 only; no DNS resolution.
     */
    class IPAddress {
        uint32_t addr_ = 0; // host-byte-order
        bool     isV6_ = false;
    public:
        IPAddress() = default;
        explicit IPAddress(uint32_t address) : addr_(address) {}

        [[nodiscard]] uint32_t getAddressProperty() const { return addr_; }

        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << ((addr_ >> 24) & 0xFF) << '.'
                << ((addr_ >> 16) & 0xFF) << '.'
                << ((addr_ >>  8) & 0xFF) << '.'
                << ( addr_        & 0xFF);
            return oss.str();
        }

        static IPAddress Parse(const std::string& s) {
            unsigned a, b, c, d;
            if (std::sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
                throw std::invalid_argument("Invalid IP address: " + s);
            return IPAddress((a << 24) | (b << 16) | (c << 8) | d);
        }

        bool operator==(const IPAddress& o) const { return addr_ == o.addr_; }
        bool operator!=(const IPAddress& o) const { return addr_ != o.addr_; }

        static const IPAddress Any;
        static const IPAddress Loopback;
        static const IPAddress Broadcast;
        static const IPAddress None;
    };

    inline const IPAddress IPAddress::Any       { 0x00000000u };
    inline const IPAddress IPAddress::Loopback  { 0x7F000001u }; // 127.0.0.1
    inline const IPAddress IPAddress::Broadcast { 0xFFFFFFFFu }; // 255.255.255.255
    inline const IPAddress IPAddress::None      { 0xFFFFFFFFu };

} // namespace System::Net
