// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <iomanip>
#include <sstream>
#include <string>

#include "System/FormatException.hpp"

namespace System::Text::Encodings::Web {

    /** Provides URL percent-encoding and decoding (RFC 3986 unreserved characters are not encoded). */
    class UrlEncoder {
        static bool isUnreserved(unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        }

        /** @return the value of one RFC 3986 `HEXDIG`, or -1 if @p c is not one. */
        static int hexDigit(unsigned char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

    public:
        /** Percent-encodes a string, leaving RFC 3986 unreserved characters unchanged. */
        static std::string Encode(const std::string& value) {
            std::ostringstream out;
            for (unsigned char c : value) {
                if (isUnreserved(c)) {
                    out << static_cast<char>(c);
                } else {
                    out << '%' << std::hex << std::uppercase
                        << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                }
            }
            return out.str();
        }

        /**
         * Decodes a percent-encoded URL string.
         *
         * A `%` followed by exactly two hexadecimal digits (RFC 3986 §2.1 `pct-encoded`)
         * decodes to the byte they denote; `+` decodes to a space; every other byte is
         * copied through. A `%` with fewer than two following bytes is copied literally,
         * as it always has been.
         *
         * @throws System::FormatException if a `%` is followed by two bytes that are not
         *         both hexadecimal digits.
         *
         * @note Ticket #2011 (SR-AUD-297, cause T-E). This body used
         *       `std::stoi(value.substr(i + 1, 2), nullptr, 16)`, which is not a
         *       two-hex-digit parser, and it produced four different wrong outcomes for
         *       malformed input (all measured in `build-probe/2006_probe1_before.log` §L):
         *       `"%zz"` threw `std::invalid_argument` — a `std::` exception escaping a
         *       `System`-shaped API, the class #1882 removed from `String::Format` — while
         *       `"%-1"` silently yielded byte `0xFF` (a negative value cast to `char`),
         *       `"% 1"` yielded `0x01` (`stoi` skips leading whitespace) and `"%+f"`
         *       yielded `0x0F`. The three silent ones are not named by the finding.
         *
         * @note This method has no .NET counterpart — `UrlEncoder` exposes an encoding
         *       policy, not a decoder — so its malformed-input contract is this port's to
         *       state. `FormatException` was chosen over a literal pass-through because the
         *       current behaviour is already a throw; widening a throw into a silent result
         *       would be the larger change, and is not this ticket's to make.
         */
        static std::string Decode(const std::string& value) {
            std::string out;
            out.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '%' && i + 2 < value.size()) {
                    const int hi = hexDigit(static_cast<unsigned char>(value[i + 1]));
                    const int lo = hexDigit(static_cast<unsigned char>(value[i + 2]));
                    if (hi < 0 || lo < 0) {
                        throw System::FormatException(
                            "Input string was not in a correct format. '%' must be followed "
                            "by two hexadecimal digits.");
                    }
                    out += static_cast<char>(static_cast<unsigned char>(hi * 16 + lo));
                    i += 2;
                } else if (value[i] == '+') {
                    out += ' ';
                } else {
                    out += value[i];
                }
            }
            return out;
        }

        /** Returns the default UrlEncoder singleton. */
        static const UrlEncoder& Default() {
            static UrlEncoder instance;
            return instance;
        }
    };

} // namespace System::Text::Encodings::Web
