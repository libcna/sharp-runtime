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
     * Provides an Internet Protocol (IP) address.
     * 
     * Partial C++ counterpart of .NET System.Net.IPAddress.
     * Only IPv4 is currently handled.
     * 
     * @note Status: Partial — IPv4 only; no DNS resolution.
     */
    class IPAddress {
        uint32_t addr_ = 0; ///< The address in host byte order.
    public:
        /** Default constructor — produces 0.0.0.0. */
        IPAddress() = default;

        /** Constructs an IPv4 address from a 32-bit host-byte-order integer. */
        explicit IPAddress(uint32_t address) : addr_(address) {}

        /** @return The raw 32-bit address in host byte order. */
        [[nodiscard]] uint32_t getAddressProperty() const { return addr_; }

        /** @return The dotted-decimal string representation (e.g. "192.168.1.1"). */
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << ((addr_ >> 24) & 0xFF) << '.'
                << ((addr_ >> 16) & 0xFF) << '.'
                << ((addr_ >>  8) & 0xFF) << '.'
                << ( addr_        & 0xFF);
            return oss.str();
        }

        /**
         * Parses a dotted-decimal IPv4 string.
         * @throws std::invalid_argument if @p s is not a valid IPv4 address.
         */
        static IPAddress Parse(const std::string& s) {
            unsigned a, b, c, d;
            if (std::sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
                throw std::invalid_argument("Invalid IP address: " + s);
            return IPAddress((a << 24) | (b << 16) | (c << 8) | d);
        }

        /** Equality operator. */
        bool operator==(const IPAddress& o) const { return addr_ == o.addr_; }
        /** Inequality operator. */
        bool operator!=(const IPAddress& o) const { return addr_ != o.addr_; }

        static const IPAddress Any;       ///< 0.0.0.0 — listens on all interfaces.
        static const IPAddress Loopback;  ///< 127.0.0.1
        static const IPAddress Broadcast; ///< 255.255.255.255
        static const IPAddress None;      ///< 255.255.255.255 (same as Broadcast)
    };

    inline const IPAddress IPAddress::Any       { 0x00000000u };
    inline const IPAddress IPAddress::Loopback  { 0x7F000001u }; // 127.0.0.1
    inline const IPAddress IPAddress::Broadcast { 0xFFFFFFFFu }; // 255.255.255.255
    inline const IPAddress IPAddress::None      { 0xFFFFFFFFu };

} // namespace System::Net
