// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <stdexcept>

namespace System::Globalization {

/// <summary>
/// Supports the use of non-ASCII characters for Internet domain names.
/// Implements Punycode (RFC 3492) / IDNA (RFC 3490) encoding and decoding.
///
/// GetAscii() converts a Unicode domain name to its ACE (xn--) ASCII representation.
/// GetUnicode() reverses the conversion.
///
/// ASCII-only labels are passed through unchanged.
/// </summary>
class IdnMapping {
public:
    IdnMapping() = default;

    /// When true, unassigned Unicode code points are allowed.
    [[nodiscard]] bool getAllowUnassignedProperty() const { return allowUnassigned_; }
    void setAllowUnassignedProperty(bool v) { allowUnassigned_ = v; }

    /// When true, validates labels against RFC 1123 Std3 name rules.
    [[nodiscard]] bool getUseStd3AsciiRulesProperty() const { return useStd3_; }
    void setUseStd3AsciiRulesProperty(bool v) { useStd3_ = v; }

    /// Converts a Unicode (UTF-8) domain name to its Punycode ASCII form.
    [[nodiscard]] std::string GetAscii(const std::string& unicode) const;

    /// Converts a Punycode ASCII domain name back to Unicode (UTF-8).
    [[nodiscard]] std::string GetUnicode(const std::string& ascii) const;

    bool operator==(const IdnMapping& o) const {
        return allowUnassigned_ == o.allowUnassigned_ && useStd3_ == o.useStd3_;
    }

private:
    bool allowUnassigned_ = false;
    bool useStd3_         = false;

    // ---- Punycode constants (RFC 3492) ----
    static constexpr int Base      = 36;
    static constexpr int Tmin      = 1;
    static constexpr int Tmax      = 26;
    static constexpr int Skew      = 38;
    static constexpr int Damp      = 700;
    static constexpr int InitialBias = 72;
    static constexpr int InitialN  = 0x80;
    static constexpr int LabelMax  = 63;
    static constexpr int NameMax   = 255;

    // ---- UTF-8 helpers ----
    static std::u32string utf8ToCodePoints(const std::string& s);
    static std::string codePointsToUtf8(const std::u32string& cp);
    static bool isBasic(char32_t c) { return c < 0x80; }

    // ---- Punycode core ----
    static int  adapt(int delta, int numpoints, bool firsttime);
    static char encodeDigit(int d);
    static int  decodeDigit(char c);

    static std::string encodeLabel(const std::u32string& label);
    static std::u32string decodeLabel(const std::string& label);
};

} // namespace System::Globalization
