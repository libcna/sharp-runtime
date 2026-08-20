// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/IPAddress.hpp"
#include "System/detail/IPAddressLiteral.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/HashCode.hpp"
#include "System/Net/Sockets/SocketError.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <exception>
#include <sstream>

namespace System::Net {

    using System::Net::Sockets::AddressFamily;
    using System::Net::Sockets::SocketError;
    using System::Net::Sockets::SocketException;

    namespace {
        // MOVED TO Core.Base BY #1997 GROUP A-2. These scanners are now
        // System::detail::tryParseIPv4Groups / tryParseIPv6 in
        // System/detail/IPAddressLiteral.hpp, because Uri::CheckHostName needs the same two
        // answers and modules/uri CANNOT reach this module -- modules/net declares
        // PUBLIC_DEPENDENCIES ... Uri, so the edge would be a cycle rather than a cost. Both
        // modules already depend on Core.Base, so the graph is unchanged and there is one
        // definition instead of two.
        using System::detail::hexDigitValue;
        using System::detail::tryParseIPv4Groups;
        using System::detail::tryParseIPv6;

        // The IPv6 scope id is stored in a uint32_t, but both public doors that set it take a
        // longcs. Before ticket #2036 each simply wrote static_cast<uint32_t>(value), so every
        // out-of-domain value wrapped modulo 2^32 instead of being rejected -- and the wrap does
        // not merely clamp, it produces a PLAUSIBLE scope id: measured before the check existed
        // (build-probe/2036_probe1_before_after.log), -1 became 4294967295, -5 became 4294967291,
        // 4294967296 became 0, 4294967297 became 1 and -4294967295 became 1, so a caller could
        // not tell a narrowed value from one it actually asked for.
        //
        // The domain is the storage's own: [0, UInt32.MaxValue]. Both callers pass their own
        // parameter's name, matching .NET's nameof(...) convention, so a caller is told which
        // argument it got wrong rather than a shared placeholder.
        uint32_t validatedScopeId(longcs value, const char* paramName) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan<longcs>(value, 0, paramName);
            System::ArgumentOutOfRangeException::ThrowIfGreaterThan<longcs>(value, 0xFFFFFFFFLL, paramName);
            return static_cast<uint32_t>(value);
        }

        std::string formatIPv4(uint32_t addr) {
            std::ostringstream oss;
            oss << ((addr >> 24) & 0xFF) << '.' << ((addr >> 16) & 0xFF) << '.'
                << ((addr >> 8) & 0xFF) << '.' << (addr & 0xFF);
            return oss.str();
        }

        std::string formatIPv6(const std::array<uint16_t, 8>& groups, uint32_t scopeId) {
            // IPv4-mapped: ::FFFF:a.b.c.d
            bool isMapped = groups[0] == 0 && groups[1] == 0 && groups[2] == 0 &&
                             groups[3] == 0 && groups[4] == 0 && groups[5] == 0xFFFF;
            std::ostringstream oss;
            if (isMapped) {
                oss << "::ffff:" << (groups[6] >> 8) << '.' << (groups[6] & 0xFF) << '.'
                    << (groups[7] >> 8) << '.' << (groups[7] & 0xFF);
            } else {
                // Find longest run of zero groups (length >= 2) to compress.
                int bestStart = -1, bestLen = 0;
                int curStart = -1, curLen = 0;
                for (int i = 0; i < 8; ++i) {
                    if (groups[i] == 0) {
                        if (curStart < 0) curStart = i;
                        curLen++;
                        if (curLen > bestLen) { bestLen = curLen; bestStart = curStart; }
                    } else {
                        curStart = -1; curLen = 0;
                    }
                }
                if (bestLen < 2) { bestStart = -1; bestLen = 0; }

                char buf[8];
                bool first = true;
                for (int i = 0; i < 8; ) {
                    if (i == bestStart) {
                        oss << "::";
                        i += bestLen;
                        first = true;
                        continue;
                    }
                    if (!first) oss << ':';
                    std::snprintf(buf, sizeof(buf), "%x", groups[i]);
                    oss << buf;
                    first = false;
                    ++i;
                }
                if (bestStart == 0 && bestLen == 8) {
                    // already emitted "::" and nothing else
                }
            }
            if (scopeId != 0) oss << '%' << scopeId;
            return oss.str();
        }
    } // namespace

    IPAddress::IPAddress(const std::vector<bytecs>& addressBytes) {
        if (addressBytes.size() == 4) {
            addressOrScopeId_ = (static_cast<uint32_t>(addressBytes[0]) << 24) |
                                 (static_cast<uint32_t>(addressBytes[1]) << 16) |
                                 (static_cast<uint32_t>(addressBytes[2]) << 8) |
                                 static_cast<uint32_t>(addressBytes[3]);
            isIPv6_ = false;
        } else if (addressBytes.size() == 16) {
            isIPv6_ = true;
            for (int i = 0; i < 8; ++i) {
                numbers_[static_cast<size_t>(i)] = static_cast<uint16_t>(
                    (static_cast<uint16_t>(addressBytes[static_cast<size_t>(i * 2)]) << 8) |
                    static_cast<uint16_t>(addressBytes[static_cast<size_t>(i * 2 + 1)]));
            }
        } else {
            throw System::ArgumentException("An invalid IP address was specified.", "address");
        }
    }

    IPAddress::IPAddress(const std::array<bytecs, 16>& addressBytes, longcs scopeId) {
        // Validated before any field is written, so a rejected construction leaves nothing
        // half-initialised.
        const uint32_t validatedScope = validatedScopeId(scopeId, "scopeId");
        isIPv6_ = true;
        addressOrScopeId_ = validatedScope;
        for (int i = 0; i < 8; ++i) {
            numbers_[static_cast<size_t>(i)] = static_cast<uint16_t>(
                (static_cast<uint16_t>(addressBytes[static_cast<size_t>(i * 2)]) << 8) |
                static_cast<uint16_t>(addressBytes[static_cast<size_t>(i * 2 + 1)]));
        }
    }

    AddressFamily IPAddress::getAddressFamilyProperty() const {
        return isIPv6_ ? AddressFamily::InterNetworkV6 : AddressFamily::InterNetwork;
    }

    uint32_t IPAddress::getAddressProperty() const {
        if (isIPv6_)
            throw SocketException(SocketError::OperationNotSupported,
                                   "The requested property is not supported for the 'InterNetworkV6' AddressFamily.");
        return addressOrScopeId_;
    }

    longcs IPAddress::getScopeIdProperty() const {
        if (!isIPv6_)
            throw SocketException(SocketError::OperationNotSupported,
                                   "The requested property is not supported for the 'InterNetwork' AddressFamily.");
        return static_cast<longcs>(addressOrScopeId_);
    }

    void IPAddress::setScopeIdProperty(longcs value) {
        // The family guard stays first: an IPv4 address has no scope id at all, so "wrong
        // family" is the more specific answer and its SocketException is the pre-existing,
        // tested contract. Only once the property exists is its domain checked, and a rejected
        // set leaves the previous scope id in place.
        if (!isIPv6_)
            throw SocketException(SocketError::OperationNotSupported,
                                   "The requested property is not supported for the 'InterNetwork' AddressFamily.");
        addressOrScopeId_ = validatedScopeId(value, "value");
    }

    std::string IPAddress::ToString() const {
        return isIPv6_ ? formatIPv6(numbers_, addressOrScopeId_) : formatIPv4(addressOrScopeId_);
    }

    std::vector<bytecs> IPAddress::GetAddressBytes() const {
        std::vector<bytecs> result;
        if (isIPv6_) {
            result.resize(16);
            for (int i = 0; i < 8; ++i) {
                result[static_cast<size_t>(i * 2)] = static_cast<bytecs>(numbers_[static_cast<size_t>(i)] >> 8);
                result[static_cast<size_t>(i * 2 + 1)] = static_cast<bytecs>(numbers_[static_cast<size_t>(i)] & 0xFF);
            }
        } else {
            result = {
                static_cast<bytecs>((addressOrScopeId_ >> 24) & 0xFF),
                static_cast<bytecs>((addressOrScopeId_ >> 16) & 0xFF),
                static_cast<bytecs>((addressOrScopeId_ >> 8) & 0xFF),
                static_cast<bytecs>(addressOrScopeId_ & 0xFF),
            };
        }
        return result;
    }

    bool IPAddress::getIsIPv6MulticastProperty() const {
        return isIPv6_ && (numbers_[0] & 0xFF00) == 0xFF00;
    }

    bool IPAddress::getIsIPv6LinkLocalProperty() const {
        return isIPv6_ && (numbers_[0] & 0xFFC0) == 0xFE80;
    }

    bool IPAddress::getIsIPv6SiteLocalProperty() const {
        return isIPv6_ && (numbers_[0] & 0xFFC0) == 0xFEC0;
    }

    bool IPAddress::getIsIPv6TeredoProperty() const {
        return isIPv6_ && numbers_[0] == 0x2001 && numbers_[1] == 0;
    }

    bool IPAddress::getIsIPv6UniqueLocalProperty() const {
        return isIPv6_ && (numbers_[0] & 0xFE00) == 0xFC00;
    }

    bool IPAddress::getIsIPv4MappedToIPv6Property() const {
        return isIPv6_ && numbers_[0] == 0 && numbers_[1] == 0 && numbers_[2] == 0 &&
               numbers_[3] == 0 && numbers_[4] == 0 && numbers_[5] == 0xFFFF;
    }

    IPAddress IPAddress::MapToIPv6() const {
        if (isIPv6_) return *this;
        std::array<uint16_t, 8> labels{};
        labels[5] = 0xFFFF;
        labels[6] = static_cast<uint16_t>(addressOrScopeId_ >> 16);
        labels[7] = static_cast<uint16_t>(addressOrScopeId_ & 0xFFFF);
        return IPAddress(labels, 0);
    }

    IPAddress IPAddress::MapToIPv4() const {
        if (!isIPv6_) return *this;
        uint32_t address = (static_cast<uint32_t>(numbers_[6]) << 16) | static_cast<uint32_t>(numbers_[7]);
        return IPAddress(address);
    }

    bool IPAddress::operator==(const IPAddress& o) const {
        if (isIPv6_ != o.isIPv6_) return false;
        if (isIPv6_) return numbers_ == o.numbers_ && addressOrScopeId_ == o.addressOrScopeId_;
        return addressOrScopeId_ == o.addressOrScopeId_;
    }

    intcs IPAddress::GetHashCode() const {
        if (isIPv6_) {
            // Verified against IPAddress.cs's GetHashCode: real .NET combines all 128 bits of
            // the address (packed as four uint32 values spanning all 8 numbers_ groups) plus
            // the scope ID. This previously combined only numbers_[0..3] (the first 64 bits),
            // ignoring numbers_[4..7] entirely -- two different IPv6 addresses sharing the same
            // /64 prefix but differing only in their host suffix (the common case for
            // SLAAC/EUI-64-derived addresses) always hashed identically.
            uint32_t p0 = (static_cast<uint32_t>(numbers_[0]) << 16) | numbers_[1];
            uint32_t p1 = (static_cast<uint32_t>(numbers_[2]) << 16) | numbers_[3];
            uint32_t p2 = (static_cast<uint32_t>(numbers_[4]) << 16) | numbers_[5];
            uint32_t p3 = (static_cast<uint32_t>(numbers_[6]) << 16) | numbers_[7];
            return System::HashCode::Combine(p0, p1, p2, p3, addressOrScopeId_);
        }
        return System::HashCode::Combine(addressOrScopeId_);
    }

    bool IPAddress::IsLoopback(const IPAddress& address) {
        if (address.isIPv6_) {
            // Verified against IPAddress.cs's IsLoopback: real .NET also treats the IPv4-mapped
            // representation of 127.0.0.1 (::ffff:127.0.0.1, s_loopbackMappedToIPv6) as
            // loopback, not just the canonical ::1 -- this port previously checked only
            // IPv6Loopback, silently reporting false for a real loopback address.
            //
            // Both loopback constants below are function-local statics (lazily initialized on
            // first call, guaranteed thread-safe since C++11) rather than references to the
            // IPv6Loopback class-level static: referencing another class-level static object
            // from a function that could itself run during another translation unit's dynamic
            // initialization is a static-initialization-order hazard -- the exact bug class
            // already found and fixed for DateTimeOffset::MinValue/MaxValue/UnixEpoch (see
            // DateTimeOffset.cpp) -- if some other TU's global constructor called IsLoopback()
            // before this TU's IPv6Loopback had run its own constructor, IPv6Loopback would
            // still be in its zero-initialized (0.0.0.0-equivalent, isIPv6_=false) state,
            // silently misclassifying every address.
            static const IPAddress loopbackMappedToIPv6(
                std::array<bytecs, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 127, 0, 0, 1}, 0);
            static const IPAddress ipv6Loopback(std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 1}, 0);
            return address == ipv6Loopback || address == loopbackMappedToIPv6;
        }
        // Same reasoning as above: compare against the literal 127.0.0.1 value directly rather
        // than the Loopback class-level static, avoiding the identical SIOF hazard.
        constexpr uint32_t loopbackMask = LoopbackMaskHostOrder;
        constexpr uint32_t loopbackAddress = 0x7F000001u;
        return (address.addressOrScopeId_ & loopbackMask) == (loopbackAddress & loopbackMask);
    }

    IPAddress IPAddress::Parse(const std::string& s) {
        IPAddress result;
        if (!TryParse(s, result)) {
            throw System::FormatException("An invalid IP address was specified.",
                                           std::make_exception_ptr(SocketException(SocketError::InvalidArgument)));
        }
        return result;
    }

    bool IPAddress::TryParse(const std::string& s, IPAddress& address) {
        if (s.find(':') != std::string::npos) {
            std::array<uint16_t, 8> groups{};
            uint32_t scopeId = 0;
            if (!tryParseIPv6(s, groups, scopeId)) return false;
            address = IPAddress(groups, scopeId);
            return true;
        }

        uint32_t v4;
        if (!tryParseIPv4Groups(s, v4)) return false;
        address = IPAddress(v4);
        return true;
    }

    shortcs IPAddress::HostToNetworkOrder(shortcs host) {
        return static_cast<shortcs>(((static_cast<uint16_t>(host) & 0xFF) << 8) | ((static_cast<uint16_t>(host) >> 8) & 0xFF));
    }

    intcs IPAddress::HostToNetworkOrder(intcs host) {
        uint32_t h = static_cast<uint32_t>(host);
        return static_cast<intcs>(((h & 0xFF) << 24) | ((h & 0xFF00) << 8) | ((h & 0xFF0000) >> 8) | ((h >> 24) & 0xFF));
    }

    longcs IPAddress::HostToNetworkOrder(longcs host) {
        uint64_t h = static_cast<uint64_t>(host);
        uint64_t result = 0;
        for (int i = 0; i < 8; ++i) {
            result = (result << 8) | ((h >> (i * 8)) & 0xFF);
        }
        return static_cast<longcs>(result);
    }

    const IPAddress IPAddress::Any { 0x00000000u };
    const IPAddress IPAddress::Loopback { 0x7F000001u };
    const IPAddress IPAddress::Broadcast { 0xFFFFFFFFu };
    const IPAddress IPAddress::None { 0xFFFFFFFFu };
    const IPAddress IPAddress::IPv6Any { std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 0}, 0 };
    const IPAddress IPAddress::IPv6Loopback { std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 1}, 0 };
    const IPAddress IPAddress::IPv6None { std::array<uint16_t, 8>{0, 0, 0, 0, 0, 0, 0, 0}, 0 };

} // namespace System::Net
