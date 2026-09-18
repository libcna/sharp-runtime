// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cctype>
#include <string_view>

namespace System::Globalization::detail {

/**
 * @brief The per-culture data this runtime carries: identity plus the three separators that
 *        decide how a list of numbers is written and read.
 *
 * .NET obtains this from ICU or NLS at run time (`CultureData`). This port has no locale
 * database, and deliberately does not fabricate one; what it carries instead is a small, explicit
 * table of the cultures its consumers name, limited to the fields a consumer has actually needed:
 *
 *  - `englishName` -- the identity metadata `CultureInfo::EnglishName` reported before this
 *    table existed (six entries), now extended to every listed culture;
 *  - `listSeparator` -- `TextInfo::ListSeparator`, the separator `Microsoft.Xna.Framework.Design`'s
 *    converters split and join `"X, Y, Z"` on;
 *  - `numberDecimalSeparator` / `numberGroupSeparator` -- applied to the number, currency and
 *    percent families of the culture's `NumberFormatInfo`.
 *
 * The values are CLDR's, which is what modern .NET reports on every platform since it moved to
 * ICU; where the .NET Framework's NLS data differed (French group separator U+00A0 rather than
 * U+202F; localised NaN and infinity words in several cultures) the CLDR value is used and the
 * difference is recorded in `docs/ComponentModelDesign.md`. Every field not in this table --
 * signs, digit shapes, currency symbol, patterns, calendar data -- keeps the invariant value for
 * every culture, and a culture name that is not listed is accepted (it is a well-formed tag) and
 * behaves as the invariant culture, exactly as before.
 *
 * Lookup is ASCII case-insensitive, as .NET's culture-name comparison is.
 */
struct CultureDataRecord {
    std::string_view name;
    std::string_view englishName;
    std::string_view listSeparator;
    std::string_view numberDecimalSeparator;
    std::string_view numberGroupSeparator;
};

/** @brief The invariant culture's row; the fallback for every name the table does not list. */
inline constexpr CultureDataRecord kInvariantCultureData{
    "", "Invariant Language (Invariant Country)", ",", ".", ","};

/** @brief The cultures this runtime carries data for. Keep sorted by name. */
inline constexpr std::array<CultureDataRecord, 22> kCultureData{{
    {"cs-CZ", "Czech (Czechia)",                  ";", ",", "\xc2\xa0"},     // U+00A0
    {"da-DK", "Danish (Denmark)",                 ";", ",", "."},
    {"de-DE", "German (Germany)",                 ";", ",", "."},
    {"en-GB", "English (United Kingdom)",         ",", ".", ","},
    {"en-US", "English (United States)",          ",", ".", ","},
    {"es-ES", "Spanish (Spain)",                  ";", ",", "."},
    {"fi-FI", "Finnish (Finland)",                ";", ",", "\xc2\xa0"},     // U+00A0
    {"fr-FR", "French (France)",                  ";", ",", "\xe2\x80\xaf"}, // U+202F
    {"hu-HU", "Hungarian (Hungary)",              ";", ",", "\xc2\xa0"},     // U+00A0
    {"it-IT", "Italian (Italy)",                  ";", ",", "."},
    {"ja-JP", "Japanese (Japan)",                 ",", ".", ","},
    {"ko-KR", "Korean (Korea)",                   ",", ".", ","},
    {"nb-NO", "Norwegian Bokm\xc3\xa5l (Norway)", ";", ",", "\xc2\xa0"},     // U+00A0
    {"nl-NL", "Dutch (Netherlands)",              ";", ",", "."},
    {"pl-PL", "Polish (Poland)",                  ";", ",", "\xc2\xa0"},     // U+00A0
    {"pt-BR", "Portuguese (Brazil)",              ";", ",", "."},
    {"ru-RU", "Russian (Russia)",                 ";", ",", "\xc2\xa0"},     // U+00A0
    {"sk-SK", "Slovak (Slovakia)",                ";", ",", "\xc2\xa0"},     // U+00A0
    {"sv-SE", "Swedish (Sweden)",                 ";", ",", "\xc2\xa0"},     // U+00A0
    {"tr-TR", "Turkish (T\xc3\xbcrkiye)",         ";", ",", "."},
    {"uk-UA", "Ukrainian (Ukraine)",              ";", ",", "\xc2\xa0"},     // U+00A0
    {"zh-CN", "Chinese (Simplified, China)",      ",", ".", ","},
}};

/**
 * @brief Finds the row for @p name, comparing ASCII case-insensitively.
 * @return The matching row, the invariant row for the empty name, or nullptr when unlisted.
 */
[[nodiscard]] inline const CultureDataRecord* FindCultureData(std::string_view name) noexcept {
    if (name.empty()) return &kInvariantCultureData;
    const auto equalsIgnoreCase = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    };
    for (const auto& record : kCultureData) {
        if (equalsIgnoreCase(record.name, name)) return &record;
    }
    return nullptr;
}

/** @brief The row @p name resolves to, falling back to the invariant row. */
[[nodiscard]] inline const CultureDataRecord& CultureDataOrInvariant(std::string_view name) noexcept {
    const CultureDataRecord* record = FindCultureData(name);
    return record != nullptr ? *record : kInvariantCultureData;
}

} // namespace System::Globalization::detail
