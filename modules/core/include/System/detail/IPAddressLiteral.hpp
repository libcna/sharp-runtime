// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

/**
 * @file
 * @brief The one definition of this runtime's IPv4 and IPv6 literal scanners.
 *
 * MOVED HERE BY #1997 GROUP A-2, AND THE MOVE IS WHAT MADE THAT GROUP POSSIBLE AT ALL.
 * `Uri::CheckHostName` classifies a host by asking whether it is a valid IPv6 literal, a valid
 * IPv4 literal, or a valid DNS name (`Uri.cs:1286-1325`), and the first two answers lived in
 * `modules/net`'s `IPAddress.cpp` in an anonymous namespace.
 *
 * **THE OBVIOUS ROUTE IS NOT MERELY DEAR, IT IS A CYCLE.** `modules/net` declares
 * `PUBLIC_DEPENDENCIES ... Uri`, so an edge from `modules/uri` to `System::Net::IPAddress` would
 * invert an existing dependency -- the shape `Guid.cpp` refused for cryptography. #1997's own
 * record priced A-2 as *"a new public module edge to reach `System::Net::IPAddress`, or a second
 * address-literal parser inside this module"*; **the first of those is impossible rather than
 * expensive**, and the second is the duplication #2354 spent a ticket removing.
 *
 * So the scanners move to where both modules can already reach them. `modules/net` and
 * `modules/uri` both depend on `Core.Base` today, so **the module graph does not change**, and
 * there is exactly one definition rather than two.
 *
 * The bodies below are moved **verbatim**, comments included, from
 * `modules/net/src/System/Net/IPAddress.cpp`. They are pure string-to-number scanners: no platform
 * call, no allocation beyond `std::string`/`std::vector`, and no dependency on `IPAddress` itself
 * -- which is why they could move. `IPAddress`'s own `validatedScopeId`, `formatIPv4` and
 * `formatIPv6` stayed behind, because they are about that type rather than about the grammar.
 */

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace System::detail {

    // Returns the numeric value of a hex digit (0-9/a-f/A-F), or a sentinel >= any
    // supported base (8/10/16) for a character that isn't a valid digit in any of them.
    inline int hexDigitValue(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return 255;
    }

    // Verified against IPv4AddressHelper.Common.cs's ParseNonCanonical, which
    // IPAddressParser.cs's IPAddress.TryParse delegates to (requiring the entire string be
    // consumed, matching this function's semantics). Parses any canonical (4-part decimal)
    // or non-canonical (octal/hex-prefixed segments, "short forms" with fewer than 3 dots
    // where the last segment absorbs the remaining bytes, e.g. "0xFF.0xFFFFFF" or a single
    // "3232235777") IPv4 literal into a 32-bit host-order value.
    //
    // Replaces the previous sscanf("%u.%u.%u.%u%c", ...) implementation, which: (1) invoked
    // undefined behavior per C11 7.21.6.2p10 on a %u conversion whose value doesn't fit in
    // unsigned int (a long enough digit run); (2) accepted a leading '-' via %u's
    // implementation-defined sign handling, which real .NET rejects; (3) rejected octal/hex
    // segments and short forms that real .NET accepts.
    inline bool tryParseIPv4Groups(const std::string& s, uint32_t& outAddr) {
        uint32_t parts[3] = {0, 0, 0};
        uint64_t currentValue = 0;
        bool atLeastOneChar = false;
        int dotCount = 0;
        size_t current = 0;
        char ch = 0;

        while (current < s.size()) {
            ch = s[current];
            currentValue = 0;
            int numberBase = 10;

            if (ch == '0') {
                ++current;
                atLeastOneChar = true;
                if (current < s.size()) {
                    ch = s[current];
                    if (ch == 'x' || ch == 'X') {
                        numberBase = 16;
                        ++current;
                        atLeastOneChar = false;
                    } else {
                        numberBase = 8;
                    }
                }
            }

            for (; current < s.size(); ++current) {
                ch = s[current];
                int digitValue = hexDigitValue(ch);
                if (digitValue >= numberBase) break;
                currentValue = currentValue * static_cast<uint64_t>(numberBase) + static_cast<uint64_t>(digitValue);
                if (currentValue > 0xFFFFFFFFULL) return false; // overflow past uint.MaxValue
                atLeastOneChar = true;
            }

            if (current < s.size() && ch == '.') {
                if (dotCount >= 3 || !atLeastOneChar || currentValue > 0xFF) return false;
                parts[dotCount] = static_cast<uint32_t>(currentValue);
                ++dotCount;
                atLeastOneChar = false;
                ++current; // consume the dot
                continue;
            }
            break;
        }

        if (!atLeastOneChar) return false;      // empty segment, e.g. "1.1.1." or ""
        if (current != s.size()) return false;   // trailing garbage (IPAddress.Parse requires full consumption)

        switch (dotCount) {
            case 0: // e.g. "3232235777" -- the whole 32-bit value in one segment
                outAddr = static_cast<uint32_t>(currentValue);
                return true;
            case 1: // e.g. "192.11534091" -- parts[0].rest
                if (currentValue > 0xFFFFFFULL) return false;
                outAddr = (parts[0] << 24) | static_cast<uint32_t>(currentValue);
                return true;
            case 2: // e.g. "192.168.257" -- parts[0].parts[1].rest
                if (currentValue > 0xFFFFULL) return false;
                outAddr = (parts[0] << 24) | (parts[1] << 16) | static_cast<uint32_t>(currentValue);
                return true;
            case 3: // standard four-octet form
                if (currentValue > 0xFFULL) return false;
                outAddr = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | static_cast<uint32_t>(currentValue);
                return true;
            default:
                return false;
        }
    }

    inline std::vector<std::string> splitOn(const std::string& s, char sep) {
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

    inline bool parseHexGroup(const std::string& s, uint16_t& out) {
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
    inline bool expandGroups(const std::vector<std::string>& tokens, std::vector<uint16_t>& out) {
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

    inline bool tryParseIPv6(const std::string& input, std::array<uint16_t, 8>& groups, uint32_t& scopeId) {
        std::string s = input;
        scopeId = 0;

        size_t pctPos = s.find('%');
        if (pctPos != std::string::npos) {
            std::string scopeStr = s.substr(pctPos + 1);
            if (scopeStr.empty() || !std::all_of(scopeStr.begin(), scopeStr.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
                return false;
            // Verified against IPAddressParser.cs: real .NET parses the numeric scope ID
            // with uint.TryParse (non-throwing) and fails the whole address parse on
            // overflow. std::stoul here previously threw std::out_of_range -- an unrelated
            // std:: exception type, uncaught anywhere in this call chain -- for a
            // many-all-digit scope string exceeding unsigned long's range, violating
            // tryParseIPv6's (and TryParse's) "never throws" contract; it would also have
            // silently truncated any value between UINT32_MAX and ULONG_MAX when narrowed
            // to uint32_t instead of failing the parse.
            uint32_t parsedScope = 0;
            auto scopeResult = std::from_chars(scopeStr.data(), scopeStr.data() + scopeStr.size(), parsedScope);
            if (scopeResult.ec != std::errc() || scopeResult.ptr != scopeStr.data() + scopeStr.size())
                return false;
            scopeId = parsedScope;
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

} // namespace System::detail
