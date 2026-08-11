// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-174, the two clauses that need no Unicode character database (ticket #2316).
//
// (1) LOCALE INDEPENDENCE. CharUnicodeInfo::GetUnicodeCategory used to classify through
//     std::iswupper/iswlower/iswdigit/iswspace/iswpunct/iswalpha/iswcntrl, which are
//     locale-sensitive. Measured before the repair: installing "C.utf8" as the global
//     locale -- exactly what CultureInvariantFormattingTests' ScopedGlobalLocale does --
//     changed the category of 287,218 of the 1,114,112 code points. .NET's CharUnicodeInfo
//     is culture-insensitive by definition (it reads the Unicode character database), so
//     the ambient locale must not be reachable from the answer at all.
//
// (2) SURROGATES. Char::IsSurrogate fixes U+D800-U+DFFF inside this port, while
//     GetUnicodeCategory answered OtherNotAssigned -- "no category assigned" -- for the
//     same code points. The header contradicted itself; that needs no external reference
//     to see, and the audit's own managed probe records the expected value, Surrogate.
//
// DELIBERATELY NOT PINNED HERE: any category for a non-ASCII, non-surrogate code point.
// Answering those needs a Unicode character database, its attribution and a stated Unicode
// version -- ticket #2315, gated on Approval F / #2018. The tests below pin the *declared
// reduction* (OtherNotAssigned) for that region, so granting Approval F is expected to
// change them; hand-authoring a partial table is not.
#include <gtest/gtest.h>
#include <locale>
#include <string>
#include <vector>
#include "System/Char.hpp"
#include "System/Globalization/CharUnicodeInfo.hpp"

using System::Char;
using System::Globalization::CharUnicodeInfo;
using System::Globalization::UnicodeCategory;

namespace {

    // RAII guard restoring the previous global locale even if the test body throws.
    class ScopedGlobalLocale {
        std::locale prev_;
    public:
        explicit ScopedGlobalLocale(const std::locale& loc) : prev_(std::locale::global(loc)) {}
        ~ScopedGlobalLocale() { std::locale::global(prev_); }
        ScopedGlobalLocale(const ScopedGlobalLocale&) = delete;
        ScopedGlobalLocale& operator=(const ScopedGlobalLocale&) = delete;
    };

    // Any installed locale other than the invariant one. "C.utf8" is present in the
    // reference container; the others are probed so the test still bites on a host that
    // ships different ones. Unlike CultureInvariantFormattingTests, "C.utf8" is included:
    // it is a non-invariant locale for wide-character classification, which is what this
    // suite measures.
    std::string findNonInvariantLocaleName() {
        for (const char* name : {"C.utf8", "C.UTF-8", "en_US.utf8", "en_US.UTF-8",
                                 "de_DE.utf8", "de_DE.UTF-8"}) {
            try {
                std::locale probe(name);
                return name;
            } catch (const std::exception&) {
                // not installed; try the next
            }
        }
        return {};
    }

} // namespace

// ---------------------------------------------------------------------------
// (2) Surrogates
// ---------------------------------------------------------------------------

TEST(CharUnicodeInfoClassificationTests, SurrogateRange_BoundariesAreSurrogate) {
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xD7FF), UnicodeCategory::OtherNotAssigned);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xD800), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDBFF), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDC00), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDFFF), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xE000), UnicodeCategory::OtherNotAssigned);
}

// The contradiction itself, stated as an equivalence over every code unit rather than as
// a handful of samples: whichever side moved, this fails.
TEST(CharUnicodeInfoClassificationTests, SurrogateCategory_AgreesWithCharIsSurrogate_EverywhereInBmp) {
    int disagreements = 0;
    int firstDisagreement = -1;
    for (int cp = 0; cp <= 0xFFFF; ++cp) {
        const auto c = static_cast<SharpRuntime::charcs>(cp);
        const bool byChar = Char::IsSurrogate(c);
        const bool byCategory = CharUnicodeInfo::GetUnicodeCategory(cp) == UnicodeCategory::Surrogate;
        if (byChar != byCategory) {
            ++disagreements;
            if (firstDisagreement < 0) firstDisagreement = cp;
        }
    }
    EXPECT_EQ(disagreements, 0) << "first disagreement at U+" << std::hex << firstDisagreement;
}

// The char16_t and std::u16string overloads reach the same answer as the code-point one.
TEST(CharUnicodeInfoClassificationTests, SurrogateCategory_ReachedByEveryOverload) {
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(static_cast<SharpRuntime::charcs>(0xD800)),
              UnicodeCategory::Surrogate);
    const std::u16string s{static_cast<char16_t>(0xD83D), static_cast<char16_t>(0xDE00)};
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(s, 0), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(s, 1), UnicodeCategory::Surrogate);
    EXPECT_EQ(Char::GetUnicodeCategory(static_cast<SharpRuntime::charcs>(0xDBFF)),
              UnicodeCategory::Surrogate);
}

// Naming the surrogates changes no predicate built on top of the category: measured over
// the whole BMP before and after the repair, no Char answer moved in the invariant locale.
TEST(CharUnicodeInfoClassificationTests, SurrogatesAreNeitherNumberNorSeparator) {
    for (int cp : {0xD800, 0xDBFF, 0xDC00, 0xDFFF}) {
        const auto c = static_cast<SharpRuntime::charcs>(cp);
        EXPECT_FALSE(Char::IsNumber(c)) << "U+" << std::hex << cp;
        EXPECT_FALSE(Char::IsSeparator(c)) << "U+" << std::hex << cp;
    }
}

// ---------------------------------------------------------------------------
// (1) Locale independence
// ---------------------------------------------------------------------------

TEST(CharUnicodeInfoClassificationTests, Category_DoesNotDependOnTheGlobalLocale) {
    const std::string localeName = findNonInvariantLocaleName();
    if (localeName.empty()) {
        GTEST_SKIP() << "No non-invariant locale installed on this system";
    }

    std::vector<int> invariant;
    invariant.reserve(0x10000);
    for (int cp = 0; cp <= 0xFFFF; ++cp)
        invariant.push_back(static_cast<int>(CharUnicodeInfo::GetUnicodeCategory(cp)));

    int changed = 0;
    int firstChanged = -1;
    {
        ScopedGlobalLocale guard{std::locale(localeName)};
        for (int cp = 0; cp <= 0xFFFF; ++cp) {
            if (static_cast<int>(CharUnicodeInfo::GetUnicodeCategory(cp))
                != invariant[static_cast<size_t>(cp)]) {
                ++changed;
                if (firstChanged < 0) firstChanged = cp;
            }
        }
    }
    EXPECT_EQ(changed, 0) << "locale " << localeName << " changed " << changed
                          << " categories, first at U+" << std::hex << firstChanged;
}

// Supplementary code points are the region where the old ladder was platform-dependent as
// well as locale-dependent: with a 16-bit wchar_t it substituted L'\0' and answered
// Control, with a 32-bit one it answered whatever the ambient locale said.
TEST(CharUnicodeInfoClassificationTests, SupplementaryCodePoints_AreTheDeclaredReduction) {
    for (int cp : {0x10000, 0x1F600, 0x2000B, 0x10FFFF}) {
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), UnicodeCategory::OtherNotAssigned)
            << "U+" << std::hex << cp;
    }
    const std::string localeName = findNonInvariantLocaleName();
    if (localeName.empty()) return;
    ScopedGlobalLocale guard{std::locale(localeName)};
    for (int cp : {0x10000, 0x1F600, 0x2000B, 0x10FFFF}) {
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), UnicodeCategory::OtherNotAssigned)
            << "U+" << std::hex << cp << " under locale " << localeName;
    }
}

// The consumers reached through Char must be locale-independent for the same reason.
TEST(CharUnicodeInfoClassificationTests, CharPredicates_DoNotDependOnTheGlobalLocale) {
    const std::string localeName = findNonInvariantLocaleName();
    if (localeName.empty()) {
        GTEST_SKIP() << "No non-invariant locale installed on this system";
    }
    std::vector<unsigned char> invariant;
    invariant.reserve(0x10000);
    for (int cp = 0; cp <= 0xFFFF; ++cp) {
        const auto c = static_cast<SharpRuntime::charcs>(cp);
        invariant.push_back(static_cast<unsigned char>(Char::IsNumber(c) * 2 + Char::IsSeparator(c)));
    }
    int changed = 0;
    {
        ScopedGlobalLocale guard{std::locale(localeName)};
        for (int cp = 0; cp <= 0xFFFF; ++cp) {
            const auto c = static_cast<SharpRuntime::charcs>(cp);
            const auto now = static_cast<unsigned char>(Char::IsNumber(c) * 2 + Char::IsSeparator(c));
            if (now != invariant[static_cast<size_t>(cp)]) ++changed;
        }
    }
    EXPECT_EQ(changed, 0) << "locale " << localeName << " changed " << changed
                          << " Char::IsNumber/IsSeparator answers";
}

// ---------------------------------------------------------------------------
// The ASCII table this port does answer from its own knowledge
// ---------------------------------------------------------------------------

// Counts, not a restatement of the implementation: 128 ASCII code points split 33 Control
// (U+0000-U+001F and U+007F), 26 + 26 letters, 10 digits, 1 SpaceSeparator (U+0020 alone --
// TAB/LF/VT/FF/CR are Cc, not Zs) and 32 punctuation. This is exactly what the removed
// locale-sensitive ladder produced in the "C" locale, over all 1,114,112 code points.
TEST(CharUnicodeInfoClassificationTests, AsciiCategoryCensus) {
    int control = 0, upper = 0, lower = 0, digit = 0, space = 0, punct = 0, other = 0;
    for (int cp = 0; cp <= 0x7F; ++cp) {
        switch (CharUnicodeInfo::GetUnicodeCategory(cp)) {
            case UnicodeCategory::Control:            ++control; break;
            case UnicodeCategory::UppercaseLetter:    ++upper;   break;
            case UnicodeCategory::LowercaseLetter:    ++lower;   break;
            case UnicodeCategory::DecimalDigitNumber: ++digit;   break;
            case UnicodeCategory::SpaceSeparator:     ++space;   break;
            case UnicodeCategory::OtherPunctuation:   ++punct;   break;
            default:                                  ++other;   break;
        }
    }
    EXPECT_EQ(control, 33);
    EXPECT_EQ(upper, 26);
    EXPECT_EQ(lower, 26);
    EXPECT_EQ(digit, 10);
    EXPECT_EQ(space, 1);
    EXPECT_EQ(punct, 32);
    EXPECT_EQ(other, 0);
    EXPECT_EQ(control + upper + lower + digit + space + punct, 128);
}

TEST(CharUnicodeInfoClassificationTests, AsciiBoundaries) {
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x1F), UnicodeCategory::Control);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x20), UnicodeCategory::SpaceSeparator);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x21), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x2F), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x30), UnicodeCategory::DecimalDigitNumber);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x39), UnicodeCategory::DecimalDigitNumber);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x40), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x41), UnicodeCategory::UppercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x5A), UnicodeCategory::UppercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x5B), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x60), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x61), UnicodeCategory::LowercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7A), UnicodeCategory::LowercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7B), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7E), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7F), UnicodeCategory::Control);
}

// The frozen finding's own non-ASCII probe rows. Every one of them is still divergent from
// .NET and stays that way until Approval F / #2018 unblocks #2315; pinning them as the
// declared reduction is what keeps SR-AUD-174 honestly open rather than quietly forgotten.
TEST(CharUnicodeInfoClassificationTests, NonAsciiRemainsTheDeclaredReduction) {
    for (int cp : {0x00A0, 0x00C9, 0x0301, 0x2014, 0x2028, 0xE000, 0xFFFD}) {
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), UnicodeCategory::OtherNotAssigned)
            << "U+" << std::hex << cp;
    }
}

// U+E000 is deliberately NOT classified PrivateUse. The audit's managed probe records that
// value for that single code point, but no private-use *range* is fixed anywhere in this
// repository -- unlike the surrogate range, which Char::IsSurrogate fixes. Extending one
// probed point to a range would be an inference, so it waits for the real table.
TEST(CharUnicodeInfoClassificationTests, PrivateUseIsNotInferredFromOneProbedCodePoint) {
    EXPECT_NE(CharUnicodeInfo::GetUnicodeCategory(0xE000), UnicodeCategory::PrivateUse);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xE000), UnicodeCategory::OtherNotAssigned);
}
