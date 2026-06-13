// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Globalization/IdnMapping.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>

namespace System::Globalization {

// ---------------------------------------------------------------------------
// UTF-8 helpers
// ---------------------------------------------------------------------------

std::u32string IdnMapping::utf8ToCodePoints(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        if (c < 0x80) {
            cp = c; ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = (char32_t(c & 0x1F) << 6) | (s[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = (char32_t(c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            cp = (char32_t(c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12)
               | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
            i += 4;
        } else {
            throw std::invalid_argument("IdnMapping: invalid UTF-8 sequence.");
        }
        out.push_back(cp);
    }
    return out;
}

std::string IdnMapping::codePointsToUtf8(const std::u32string& cp) {
    std::string out;
    out.reserve(cp.size());
    for (char32_t c : cp) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Punycode core (RFC 3492)
// ---------------------------------------------------------------------------

int IdnMapping::adapt(int delta, int numpoints, bool firsttime) {
    delta = firsttime ? delta / Damp : delta / 2;
    delta += delta / numpoints;
    int k = 0;
    while (delta > ((Base - Tmin) * Tmax) / 2) {
        delta /= Base - Tmin;
        k += Base;
    }
    return k + (Base - Tmin + 1) * delta / (delta + Skew);
}

char IdnMapping::encodeDigit(int d) {
    return d < 26 ? static_cast<char>('a' + d) : static_cast<char>('0' + d - 26);
}

int IdnMapping::decodeDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0' + 26;
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    throw std::invalid_argument("IdnMapping: invalid Punycode digit.");
}

// Encode one label (no dots) to Punycode without the "xn--" prefix.
// Returns the empty string if the label is already pure ASCII.
std::string IdnMapping::encodeLabel(const std::u32string& label) {
    // Count basic code points
    std::string out;
    int numBasic = 0;
    for (char32_t c : label) {
        if (isBasic(c)) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            ++numBasic;
        }
    }

    // If all basic, no encoding needed
    if (numBasic == static_cast<int>(label.size()))
        return out;

    // Non-ASCII present — output basic chars then delimiter
    if (numBasic > 0) out += '-';

    int n = InitialN, delta = 0, bias = InitialBias;
    int processed = numBasic;
    int inputLen = static_cast<int>(label.size());

    while (processed < inputLen) {
        // Find smallest non-basic code point >= n
        int m = std::numeric_limits<int>::max();
        for (char32_t c : label)
            if (static_cast<int>(c) >= n && static_cast<int>(c) < m)
                m = static_cast<int>(c);

        delta += (m - n) * (processed + 1);
        n = m;

        for (char32_t c : label) {
            if (static_cast<int>(c) < n) ++delta;
            if (static_cast<int>(c) == n) {
                int q = delta;
                for (int k = Base; ; k += Base) {
                    int t = k <= bias ? Tmin : k >= bias + Tmax ? Tmax : k - bias;
                    if (q < t) break;
                    out += encodeDigit(t + (q - t) % (Base - t));
                    q = (q - t) / (Base - t);
                }
                out += encodeDigit(q);
                bias = adapt(delta, processed + 1, processed == numBasic);
                delta = 0;
                ++processed;
            }
        }
        ++delta;
        ++n;
    }
    return out;
}

// Decode one Punycode-encoded label (without "xn--" prefix, after the prefix is stripped).
std::u32string IdnMapping::decodeLabel(const std::string& label) {
    std::u32string out;

    // Find last delimiter
    int delimPos = -1;
    for (int i = static_cast<int>(label.size()) - 1; i >= 0; --i) {
        if (label[i] == '-') { delimPos = i; break; }
    }

    // Copy basic code points
    int asciiStart = 0;
    if (delimPos > 0) {
        for (int i = 0; i < delimPos; ++i) {
            if (static_cast<unsigned char>(label[i]) > 0x7F)
                throw std::invalid_argument("IdnMapping: non-ASCII in basic portion.");
            out += static_cast<char32_t>(std::tolower(static_cast<unsigned char>(label[i])));
        }
        asciiStart = delimPos + 1;
    }

    int n = InitialN, bias = InitialBias, i = 0;
    int pos = asciiStart;
    int len = static_cast<int>(label.size());

    while (pos < len) {
        int oldi = i;
        int w = 1;
        for (int k = Base; ; k += Base) {
            if (pos >= len) throw std::invalid_argument("IdnMapping: malformed Punycode.");
            int digit = decodeDigit(label[pos++]);
            i += digit * w;
            int t = k <= bias ? Tmin : k >= bias + Tmax ? Tmax : k - bias;
            if (digit < t) break;
            w *= Base - t;
        }
        int outLen = static_cast<int>(out.size()) + 1;
        bias = adapt(i - oldi, outLen, oldi == 0);
        n += i / outLen;
        i %= outLen;
        if (n < 0x80 || (n >= 0xD800 && n <= 0xDFFF) || n > 0x10FFFF)
            throw std::invalid_argument("IdnMapping: invalid code point in Punycode.");
        out.insert(out.begin() + i, static_cast<char32_t>(n));
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string IdnMapping::GetAscii(const std::string& unicode) const {
    if (unicode.empty()) throw std::invalid_argument("IdnMapping: empty input.");

    std::u32string input = utf8ToCodePoints(unicode);
    std::string result;
    result.reserve(unicode.size());

    size_t start = 0;
    while (start <= input.size()) {
        // Find next label separator (dot)
        size_t end = start;
        while (end < input.size() && input[end] != U'.' &&
               input[end] != U'。' && input[end] != U'．' && input[end] != U'｡')
            ++end;

        if (end == start) {
            // Trailing dot → keep it and stop
            if (end == input.size()) break;
            throw std::invalid_argument("IdnMapping: empty label.");
        }

        std::u32string label(input.begin() + static_cast<long>(start),
                              input.begin() + static_cast<long>(end));

        // Check if label is all ASCII
        bool allAscii = true;
        for (char32_t c : label) if (!isBasic(c)) { allAscii = false; break; }

        if (allAscii) {
            // Pass through lowercased
            for (char32_t c : label)
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            // Encode as Punycode
            result += "xn--";
            result += encodeLabel(label);
        }

        if (end < input.size()) result += '.'; // keep dot separator

        if (end == input.size()) break;
        start = end + 1;
    }

    if (result.size() > static_cast<size_t>(NameMax))
        throw std::invalid_argument("IdnMapping: encoded name exceeds 255 characters.");

    return result;
}

std::string IdnMapping::GetUnicode(const std::string& ascii) const {
    if (ascii.empty()) throw std::invalid_argument("IdnMapping: empty input.");

    std::string result;
    result.reserve(ascii.size());

    size_t start = 0;
    while (start <= ascii.size()) {
        size_t end = ascii.find('.', start);
        if (end == std::string::npos) end = ascii.size();

        if (end == start) {
            if (end == ascii.size()) break;
            throw std::invalid_argument("IdnMapping: empty label.");
        }

        std::string label = ascii.substr(start, end - start);

        // Case-insensitive check for "xn--" prefix
        bool isPunycode = label.size() >= 4 &&
            (label[0] == 'x' || label[0] == 'X') &&
            (label[1] == 'n' || label[1] == 'N') &&
            label[2] == '-' && label[3] == '-';

        if (isPunycode) {
            std::u32string decoded = decodeLabel(label.substr(4));
            result += codePointsToUtf8(decoded);
        } else {
            result += label;
        }

        if (end < ascii.size()) result += '.';

        if (end == ascii.size()) break;
        start = end + 1;
    }

    return result;
}

} // namespace System::Globalization
