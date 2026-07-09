// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/IPAddress.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/HashCode.hpp"
#include "System/Net/Sockets/SocketError.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <sstream>

namespace System::Net {

    using System::Net::Sockets::AddressFamily;
    using System::Net::Sockets::SocketError;
    using System::Net::Sockets::SocketException;

    namespace {
        bool tryParseIPv4Groups(const std::string& s, uint32_t& outAddr) {
            unsigned a, b, c, d;
            char extra;
            if (std::sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
            if (a > 255 || b > 255 || c > 255 || d > 255) return false;
            outAddr = (a << 24) | (b << 16) | (c << 8) | d;
            return true;
        }

        std::vector<std::string> splitOn(const std::string& s, char sep) {
            std::vector<std::string> parts;
            size_t start = 0;
            while (true) {
                size_t pos = s.find(sep, start);
                if (pos == std::string::npos) {
                    parts.push_back(s.substr(start));
                    break;
                }
                parts.push_back(s.substr(start, pos - start));
                start = pos + 1;
            }
            return parts;
        }

        bool parseHexGroup(const std::string& s, uint16_t& out) {
            if (s.empty() || s.size() > 4) return false;
            uint32_t value = 0;
            for (char c : s) {
                value <<= 4;
                if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
                else return false;
            }
            out = static_cast<uint16_t>(value);
            return true;
        }

        // Expands a list of ':'-separated group tokens into uint16 groups, handling
        // an embedded IPv4 dotted-quad as the last token (e.g. "ffff:192.168.1.1").
        bool expandGroups(const std::vector<std::string>& tokens, std::vector<uint16_t>& out) {
            for (size_t i = 0; i < tokens.size(); ++i) {
                const std::string& tok = tokens[i];
                if (tok.find('.') != std::string::npos) {
                    if (i != tokens.size() - 1) return false;
                    uint32_t v4;
                    if (!tryParseIPv4Groups(tok, v4)) return false;
                    out.push_back(static_cast<uint16_t>(v4 >> 16));
                    out.push_back(static_cast<uint16_t>(v4 & 0xFFFF));
                } else {
                    uint16_t g;
                    if (!parseHexGroup(tok, g)) return false;
                    out.push_back(g);
                }
            }
            return true;
        }

        bool tryParseIPv6(const std::string& input, std::array<uint16_t, 8>& groups, uint32_t& scopeId) {
            std::string s = input;
            scopeId = 0;

            size_t pctPos = s.find('%');
            if (pctPos != std::string::npos) {
                std::string scopeStr = s.substr(pctPos + 1);
                if (scopeStr.empty() || !std::all_of(scopeStr.begin(), scopeStr.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
                    return false;
                scopeId = static_cast<uint32_t>(std::stoul(scopeStr));
                s = s.substr(0, pctPos);
            }

            if (s.find(':') == std::string::npos) return false;

            size_t dcPos = s.find("::");
            std::vector<uint16_t> allGroups;

            if (dcPos != std::string::npos) {
                if (s.find("::", dcPos + 1) != std::string::npos) return false; // more than one "::"

                std::string left = s.substr(0, dcPos);
                std::string right = s.substr(dcPos + 2);

                std::vector<uint16_t> leftGroups, rightGroups;
                if (!left.empty() && !expandGroups(splitOn(left, ':'), leftGroups)) return false;
                if (!right.empty() && !expandGroups(splitOn(right, ':'), rightGroups)) return false;

                size_t total = leftGroups.size() + rightGroups.size();
                if (total > 7) return false; // "::" must represent at least one group

                allGroups = leftGroups;
                allGroups.resize(allGroups.size() + (8 - total), 0);
                allGroups.insert(allGroups.end(), rightGroups.begin(), rightGroups.end());
            } else {
                if (!expandGroups(splitOn(s, ':'), allGroups)) return false;
                if (allGroups.size() != 8) return false;
            }

            if (allGroups.size() != 8) return false;
            std::copy(allGroups.begin(), allGroups.end(), groups.begin());
            return true;
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
        isIPv6_ = true;
        addressOrScopeId_ = static_cast<uint32_t>(scopeId);
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
        if (!isIPv6_)
            throw SocketException(SocketError::OperationNotSupported,
                                   "The requested property is not supported for the 'InterNetwork' AddressFamily.");
        addressOrScopeId_ = static_cast<uint32_t>(value);
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
            return System::HashCode::Combine(numbers_[0], numbers_[1], numbers_[2], numbers_[3], addressOrScopeId_);
        }
        return System::HashCode::Combine(addressOrScopeId_);
    }

    bool IPAddress::IsLoopback(const IPAddress& address) {
        if (address.isIPv6_) {
            return address == IPv6Loopback;
        }
        uint32_t loopbackMask = LoopbackMaskHostOrder;
        return (address.addressOrScopeId_ & loopbackMask) == (Loopback.addressOrScopeId_ & loopbackMask);
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
