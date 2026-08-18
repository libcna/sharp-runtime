// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Globalization/IdnMapping.hpp"
#include "System/detail/Utf8Scalar.hpp"
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
    // Each branch here previously trusted the continuation bytes' low 6 bits without checking
    // that their top 2 bits are 10xxxxxx, and never rejected overlong encodings, surrogate code
    // points, or out-of-range results -- so malformed UTF-8 was silently misinterpreted as some
    // other, unrelated code point. Confirmed with a standalone repro: "\xC2\x41" (a valid
    // 2-byte lead byte followed by 'A') produced a garbage Punycode result with no exception.
    // That matters here specifically, since domain names routinely originate from untrusted
    // input.
    //
    // Ticket #2354 (2026-08-18) found this copy, which the ticket itself did not name. It is a
    // THIRD contract over the same rule -- Rune reports, an Encoding substitutes, and this one
    // throws -- and all three are now spellings of System/detail/Utf8Scalar.hpp's one decode.
    // The two extra rejections this copy spelled out by hand are subsumed exactly: a 2-byte
    // lead below 0xC2 is an overlong encoding, and a 4-byte lead above 0xF4 is above U+10FFFF.
    size_t i = 0;
    while (i < s.size()) {
        std::uint32_t cp  = 0;
        std::size_t   len = 0;
        if (!System::detail::TryDecodeUtf8Scalar(s, i, cp, len))
            throw System::ArgumentException("IdnMapping: invalid UTF-8 sequence.");
        out.push_back(static_cast<char32_t>(cp));
        i += len;
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
    throw System::ArgumentException("IdnMapping: invalid Punycode digit.");
}

// Verified against IdnMapping.cs's ValidateStd3: only alphanumerics and a hyphen not
// adjacent to a label boundary are allowed; codepoints above 0x7F are unrestricted here
// (Std3 only constrains the ASCII subset -- non-ASCII chars go through Nameprep instead,
// which this port does not implement).
void IdnMapping::validateStd3Char(char32_t c, bool nextToLabelBoundary) {
    if (c > 0x7F) return;
    if (c <= ',' || c == '/' || (c >= ':' && c <= '@') ||
        (c >= '[' && c <= '`') || (c >= '{' && c <= 0x7F) ||
        (c == '-' && nextToLabelBoundary))
        throw System::ArgumentException("IdnMapping: character is not allowed under UseStd3AsciiRules.");
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

    // A delimiter as the very last character means zero extended code points followed it --
    // never produced by encodeLabel() and rejected by real .NET (IdnMapping.cs's
    // PunycodeDecode: "Trailing - not allowed").
    if (delimPos == static_cast<int>(label.size()) - 1)
        throw System::ArgumentException("IdnMapping: malformed Punycode (trailing delimiter).");

    // Copy basic code points
    int asciiStart = 0;
    if (delimPos > 0) {
        for (int i = 0; i < delimPos; ++i) {
            if (static_cast<unsigned char>(label[i]) > 0x7F)
                throw System::ArgumentException("IdnMapping: non-ASCII in basic portion.");
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
            if (pos >= len) throw System::ArgumentException("IdnMapping: malformed Punycode.");
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
            throw System::ArgumentException("IdnMapping: invalid code point in Punycode.");
        out.insert(out.begin() + i, static_cast<char32_t>(n));
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace {
    bool equalsIgnoreCaseAscii(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }
}

std::string IdnMapping::GetAscii(const std::string& unicode) const {
    if (unicode.empty()) throw System::ArgumentException("IdnMapping: empty input.");

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
            throw System::ArgumentException("IdnMapping: empty label.");
        }

        std::u32string label(input.begin() + static_cast<long>(start),
                              input.begin() + static_cast<long>(end));

        if (label.size() > static_cast<size_t>(LabelMax))
            throw System::ArgumentException("IdnMapping: label exceeds 63 characters.");

        // Check if label is all ASCII
        bool allAscii = true;
        for (char32_t c : label) if (!isBasic(c)) { allAscii = false; break; }

        if (useStd3_) {
            for (size_t i = 0; i < label.size(); ++i)
                validateStd3Char(label[i], i == 0 || i == label.size() - 1);
        }

        if (allAscii) {
            // Pass through lowercased
            for (char32_t c : label)
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            // Encode as Punycode
            std::string encoded = "xn--";
            encoded += encodeLabel(label);
            if (encoded.size() > static_cast<size_t>(LabelMax))
                throw System::ArgumentException("IdnMapping: encoded label exceeds 63 characters.");
            result += encoded;
        }

        if (end < input.size()) result += '.'; // keep dot separator

        if (end == input.size()) break;
        start = end + 1;
    }

    if (result.size() > static_cast<size_t>(NameMax))
        throw System::ArgumentException("IdnMapping: encoded name exceeds 255 characters.");

    return result;
}

std::string IdnMapping::GetAscii(const std::string& unicode, SharpRuntime::intcs index) const {
    if (index < 0) throw System::ArgumentOutOfRangeException("index");
    if (static_cast<size_t>(index) > unicode.size()) throw System::ArgumentOutOfRangeException("index");
    return GetAscii(unicode.substr(static_cast<size_t>(index)));
}

std::string IdnMapping::GetAscii(const std::string& unicode, SharpRuntime::intcs index, SharpRuntime::intcs count) const {
    if (index < 0) throw System::ArgumentOutOfRangeException("index");
    if (count < 0) throw System::ArgumentOutOfRangeException("count");
    if (static_cast<size_t>(index) > unicode.size()) throw System::ArgumentOutOfRangeException("index");
    if (static_cast<size_t>(index) + static_cast<size_t>(count) > unicode.size())
        throw System::ArgumentOutOfRangeException("unicode");
    return GetAscii(unicode.substr(static_cast<size_t>(index), static_cast<size_t>(count)));
}

std::string IdnMapping::GetUnicode(const std::string& ascii) const {
    if (ascii.empty()) throw System::ArgumentException("IdnMapping: empty input.");

    std::string result;
    result.reserve(ascii.size());

    size_t start = 0;
    while (start <= ascii.size()) {
        size_t end = ascii.find('.', start);
        if (end == std::string::npos) end = ascii.size();

        if (end == start) {
            if (end == ascii.size()) break;
            throw System::ArgumentException("IdnMapping: empty label.");
        }

        std::string label = ascii.substr(start, end - start);

        if (label.size() > static_cast<size_t>(LabelMax))
            throw System::ArgumentException("IdnMapping: label exceeds 63 characters.");

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

    // Output name MUST obey IDNA rules & round-trip through GetAscii (casing differences
    // allowed) -- verified against IdnMapping.cs's GetUnicodeInvariant.
    std::string roundtrip = GetAscii(result);
    if (!equalsIgnoreCaseAscii(roundtrip, ascii))
        throw System::ArgumentException("IdnMapping: decoded name does not round-trip through GetAscii.");

    return result;
}

std::string IdnMapping::GetUnicode(const std::string& ascii, SharpRuntime::intcs index) const {
    if (index < 0) throw System::ArgumentOutOfRangeException("index");
    if (static_cast<size_t>(index) > ascii.size()) throw System::ArgumentOutOfRangeException("index");
    return GetUnicode(ascii.substr(static_cast<size_t>(index)));
}

std::string IdnMapping::GetUnicode(const std::string& ascii, SharpRuntime::intcs index, SharpRuntime::intcs count) const {
    if (index < 0) throw System::ArgumentOutOfRangeException("index");
    if (count < 0) throw System::ArgumentOutOfRangeException("count");
    if (static_cast<size_t>(index) > ascii.size()) throw System::ArgumentOutOfRangeException("index");
    if (static_cast<size_t>(index) + static_cast<size_t>(count) > ascii.size())
        throw System::ArgumentOutOfRangeException("ascii");
    return GetUnicode(ascii.substr(static_cast<size_t>(index), static_cast<size_t>(count)));
}

} // namespace System::Globalization
