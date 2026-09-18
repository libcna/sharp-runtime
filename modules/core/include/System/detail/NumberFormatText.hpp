// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "System/IFormatProvider.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Globalization/NumberStyles.hpp"

namespace System::detail {

/**
 * @brief Bridges a culture's `NumberFormatInfo` and the invariant numeric grammar every
 *        `Parse`/`ToString` core in this runtime is written against.
 *
 * The numeric parsers and formatters of `Core.Base` (`Single`, `Double`, the eight integer
 * types) implement the invariant spelling only: `.` as the decimal separator, `,` as the group
 * separator, `-`/`+` as the signs, `¤` as the currency symbol and `NaN`/`Infinity` as the special
 * symbols. A `NumberFormatInfo` handed in through an `IFormatProvider` (typically a
 * `CultureInfo`) can spell each of those tokens differently. Rather than teaching every grammar
 * about every symbol, this helper rewrites the text once, in a single left-to-right pass that
 * consumes each input position exactly once:
 *
 *  - `NormalizeForParsing` maps the culture's spellings to the invariant ones before a parse, so
 *    `"1.234,5"` under a `de-DE`-shaped info becomes `"1,234.5"` -- the group separator and the
 *    decimal separator are swapped simultaneously, which a sequential replace could not do;
 *  - `LocalizeFormatted` maps the invariant spellings a formatter produced back to the culture's,
 *    so `"-1.5"` becomes `"-1,5"` under the same info.
 *
 * An info whose relevant symbols already equal the invariant ones is detected up front and the
 * text is passed through untouched, so a caller that never hands over a culture pays nothing.
 *
 * This mirrors what .NET's `Number.Parsing` and `Number.Formatting` do with the same symbols; the
 * mechanics differ (they match symbols inside the grammar), the accepted language does not for the
 * separators, signs and special symbols listed above. What is deliberately **not** reproduced: the
 * .NET Framework-era tolerance that let a plain space match a non-breaking-space group separator
 * (modern .NET dropped it), and native-digit substitution.
 */
struct NumberFormatText {
    /**
     * @brief Resolves the `NumberFormatInfo` a provider designates, or the invariant one.
     *
     * Equivalent to `NumberFormatInfo::GetInstance(provider)`; kept here so the numeric headers
     * have one spelling for "the info this call formats or parses with".
     */
    [[nodiscard]] static const System::Globalization::NumberFormatInfo& Resolve(
            const System::IFormatProvider* provider) {
        return System::Globalization::NumberFormatInfo::GetInstance(provider);
    }

    /**
     * @brief Reports whether every symbol the normalization consults is spelled as the invariant
     *        culture spells it, in which case both rewrites are the identity.
     */
    [[nodiscard]] static bool IsInvariantSpelling(const System::Globalization::NumberFormatInfo& info) {
        const auto& inv = System::Globalization::NumberFormatInfo::getInvariantInfoProperty();
        return info.getNumberDecimalSeparatorProperty() == inv.getNumberDecimalSeparatorProperty()
            && info.getNumberGroupSeparatorProperty() == inv.getNumberGroupSeparatorProperty()
            && info.getCurrencyDecimalSeparatorProperty() == inv.getCurrencyDecimalSeparatorProperty()
            && info.getCurrencyGroupSeparatorProperty() == inv.getCurrencyGroupSeparatorProperty()
            && info.getCurrencySymbolProperty() == inv.getCurrencySymbolProperty()
            && info.getNegativeSignProperty() == inv.getNegativeSignProperty()
            && info.getPositiveSignProperty() == inv.getPositiveSignProperty()
            && info.getNaNSymbolProperty() == inv.getNaNSymbolProperty()
            && info.getPositiveInfinitySymbolProperty() == inv.getPositiveInfinitySymbolProperty()
            && info.getNegativeInfinitySymbolProperty() == inv.getNegativeInfinitySymbolProperty();
    }

    /**
     * @brief Rewrites @p text from the spelling of @p info to the invariant spelling.
     *
     * The currency separators and symbol take part only when @p style allows a currency symbol,
     * so a `NumberStyles::Float` parse under a culture whose currency and number separators
     * differ sees only the number family, exactly as .NET's grammar consults only the symbols the
     * style admits.
     *
     * @param text  The caller's text, in the culture's spelling.
     * @param style The style the subsequent parse will apply.
     * @param info  The culture's number format.
     * @return The invariant spelling, or @p text itself when @p info is spelled invariantly.
     */
    [[nodiscard]] static std::string NormalizeForParsing(std::string_view text,
                                                         System::Globalization::NumberStyles style,
                                                         const System::Globalization::NumberFormatInfo& info) {
        if (IsInvariantSpelling(info)) return std::string(text);
        using System::Globalization::NumberStyles;
        const bool currency = (style & NumberStyles::AllowCurrencySymbol) != NumberStyles::None;

        // Every spelling the culture uses, identical to the invariant one or not: an invariant
        // token the culture does not use for any role must NOT survive the rewrite as a token,
        // or "1.5" would parse under a culture whose decimal separator is "," and which has no
        // role for "." at all (.NET rejects it; a de-DE-shaped info, which uses "." as the group
        // separator, accepts it as 15 -- and so does this).
        std::vector<std::string> used{
            info.getNegativeInfinitySymbolProperty(), info.getPositiveInfinitySymbolProperty(),
            info.getNaNSymbolProperty(), info.getNumberDecimalSeparatorProperty(),
            info.getNumberGroupSeparatorProperty(), info.getNegativeSignProperty(),
            info.getPositiveSignProperty()};
        if (currency) {
            used.push_back(info.getCurrencyDecimalSeparatorProperty());
            used.push_back(info.getCurrencyGroupSeparatorProperty());
            used.push_back(info.getCurrencySymbolProperty());
        }

        std::vector<Replacement> table;
        table.reserve(16);
        add(table, info.getNegativeInfinitySymbolProperty(), "-Infinity");
        add(table, info.getPositiveInfinitySymbolProperty(), "Infinity");
        add(table, info.getNaNSymbolProperty(), "NaN");
        add(table, info.getNumberDecimalSeparatorProperty(), ".");
        add(table, info.getNumberGroupSeparatorProperty(), ",");
        if (currency) {
            add(table, info.getCurrencyDecimalSeparatorProperty(), ".");
            add(table, info.getCurrencyGroupSeparatorProperty(), ",");
            add(table, info.getCurrencySymbolProperty(), "\xc2\xa4");
        }
        add(table, info.getNegativeSignProperty(), "-");
        add(table, info.getPositiveSignProperty(), "+");

        // An unused invariant token becomes a control character no grammar accepts.
        static constexpr std::array<std::string_view, 7> invariantTokens{
            "-Infinity", "Infinity", "NaN", ".", ",", "-", "+"};
        for (const std::string_view token : invariantTokens) {
            bool isUsed = false;
            for (const auto& spelling : used) {
                if (spelling == token) { isUsed = true; break; }
            }
            if (!isUsed) add(table, std::string(token), "\x01");
        }
        if (currency) {
            bool symbolUsed = false;
            for (const auto& spelling : used) {
                if (spelling == "\xc2\xa4") { symbolUsed = true; break; }
            }
            if (!symbolUsed) add(table, "\xc2\xa4", "\x01");
        }
        return rewrite(text, table);
    }

    /**
     * @brief Convenience overload resolving @p provider first.
     * @see NormalizeForParsing(std::string_view, NumberStyles, const NumberFormatInfo&)
     */
    [[nodiscard]] static std::string NormalizeForParsing(std::string_view text,
                                                         System::Globalization::NumberStyles style,
                                                         const System::IFormatProvider* provider) {
        if (provider == nullptr) return std::string(text);
        return NormalizeForParsing(text, style, Resolve(provider));
    }

    /**
     * @brief Rewrites formatter output from the invariant spelling to that of @p info.
     *
     * @param invariantText Text a `Core.Base` formatter produced.
     * @param info          The culture's number format.
     * @return The culture's spelling, or @p invariantText itself when @p info is invariant.
     */
    [[nodiscard]] static std::string LocalizeFormatted(std::string_view invariantText,
                                                       const System::Globalization::NumberFormatInfo& info) {
        if (IsInvariantSpelling(info)) return std::string(invariantText);
        std::vector<Replacement> table;
        table.reserve(7);
        add(table, "-Infinity", info.getNegativeInfinitySymbolProperty());
        add(table, "Infinity", info.getPositiveInfinitySymbolProperty());
        add(table, "NaN", info.getNaNSymbolProperty());
        add(table, ".", info.getNumberDecimalSeparatorProperty());
        add(table, ",", info.getNumberGroupSeparatorProperty());
        add(table, "-", info.getNegativeSignProperty());
        add(table, "+", info.getPositiveSignProperty());
        return rewrite(invariantText, table);
    }

    /**
     * @brief Convenience overload resolving @p provider first.
     * @see LocalizeFormatted(std::string_view, const NumberFormatInfo&)
     */
    [[nodiscard]] static std::string LocalizeFormatted(std::string_view invariantText,
                                                       const System::IFormatProvider* provider) {
        if (provider == nullptr) return std::string(invariantText);
        return LocalizeFormatted(invariantText, Resolve(provider));
    }

private:
    struct Replacement {
        std::string from;
        std::string to;
    };

    static void add(std::vector<Replacement>& table, std::string from, std::string to) {
        if (from.empty() || from == to) return;
        for (const auto& existing : table) {
            if (existing.from == from) return;   // first mapping of a spelling wins
        }
        table.push_back({std::move(from), std::move(to)});
    }

    // One pass, longest match first at every position, each input byte consumed once. A token that
    // was produced by a replacement is never re-examined, which is what keeps "group -> ',' " and
    // "',' is the invariant group separator" from feeding each other.
    static std::string rewrite(std::string_view text, const std::vector<Replacement>& table) {
        std::string out;
        out.reserve(text.size() + 8);
        std::size_t i = 0;
        while (i < text.size()) {
            const Replacement* best = nullptr;
            for (const auto& r : table) {
                if (text.compare(i, r.from.size(), r.from) == 0 &&
                    (best == nullptr || r.from.size() > best->from.size())) {
                    best = &r;
                }
            }
            if (best != nullptr) {
                out += best->to;
                i += best->from.size();
            } else {
                out.push_back(text[i]);
                ++i;
            }
        }
        return out;
    }
};

} // namespace System::detail
