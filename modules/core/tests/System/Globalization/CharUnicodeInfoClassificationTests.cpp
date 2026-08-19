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
#include <map>
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

TEST(CharUnicodeInfoClassificationTests, Fix2315_SurrogateRangeBoundaries) {
    // #2315 moved the UPPER boundary off OtherNotAssigned -- U+E000 is the first private-use
    // code point -- and left the lower one alone. U+D7FF really is unassigned in UCD 16.0
    // (the Hangul Jamo Extended-B block stops at U+D7FB), so the value #2316 pinned there was
    // right for a reason it could not have known, and is kept rather than "updated".
    //
    // The surrogate range itself is also unchanged: #2316 fixed it from Char::IsSurrogate and
    // the UCD agrees, which is the cheapest possible confirmation that #2316 was right.
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xD7FF), UnicodeCategory::OtherNotAssigned);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xD800), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDBFF), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDC00), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xDFFF), UnicodeCategory::Surrogate);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0xE000), UnicodeCategory::PrivateUse);
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
TEST(CharUnicodeInfoClassificationTests, Fix2315_SupplementaryCodePointsAreClassifiedForReal) {
    // #2316 pinned all four as the declared reduction. #2315 answers them, and the locale
    // half of #2316's statement is KEPT -- a table lookup consults no locale facet at all,
    // which is a stronger guarantee than the ASCII ladder's, not a weaker one.
    const std::vector<std::pair<int, UnicodeCategory>> rows = {
        {0x10000,  UnicodeCategory::OtherLetter},        // LINEAR B SYLLABLE B008 A
        {0x1F600,  UnicodeCategory::OtherSymbol},        // GRINNING FACE
        {0x2000B,  UnicodeCategory::OtherLetter},        // CJK Extension B ideograph
        {0x10FFFF, UnicodeCategory::OtherNotAssigned},   // genuinely unassigned, and still is
    };
    for (const auto& [cp, expected] : rows)
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), expected) << "U+" << std::hex << cp;

    const std::string localeName = findNonInvariantLocaleName();
    if (localeName.empty()) return;
    ScopedGlobalLocale guard{std::locale(localeName)};
    for (const auto& [cp, expected] : rows)
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), expected)
            << "U+" << std::hex << cp << " under locale " << localeName;
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

// Counts, not a restatement of the implementation. #2316's version of this census had one
// bucket for all 32 punctuation and symbol characters, because the ladder it pinned answered
// OtherPunctuation for every one of them. UCD 16.0 splits those 32 across SEVEN categories,
// and that split is the whole of what #2315 changed inside ASCII: 17 of the 128 moved, and
// every letter, digit, control and the single space stayed exactly where they were.
TEST(CharUnicodeInfoClassificationTests, Fix2315_AsciiCategoryCensus) {
    std::map<UnicodeCategory, int> census;
    for (int cp = 0; cp <= 0x7F; ++cp) ++census[CharUnicodeInfo::GetUnicodeCategory(cp)];

    EXPECT_EQ(census[UnicodeCategory::Control], 33);              // U+0000-U+001F and U+007F
    EXPECT_EQ(census[UnicodeCategory::UppercaseLetter], 26);
    EXPECT_EQ(census[UnicodeCategory::LowercaseLetter], 26);
    EXPECT_EQ(census[UnicodeCategory::DecimalDigitNumber], 10);
    EXPECT_EQ(census[UnicodeCategory::SpaceSeparator], 1);        // U+0020 alone; TAB..CR are Cc
    // The seven-way split of the 32 the ladder lumped together.
    EXPECT_EQ(census[UnicodeCategory::OtherPunctuation], 15);
    EXPECT_EQ(census[UnicodeCategory::MathSymbol], 6);            // + < = > | ~
    EXPECT_EQ(census[UnicodeCategory::OpenPunctuation], 3);       // ( [ {
    EXPECT_EQ(census[UnicodeCategory::ClosePunctuation], 3);      // ) ] }
    EXPECT_EQ(census[UnicodeCategory::ModifierSymbol], 2);        // ^ `
    EXPECT_EQ(census[UnicodeCategory::CurrencySymbol], 1);        // $
    EXPECT_EQ(census[UnicodeCategory::DashPunctuation], 1);       // -
    EXPECT_EQ(census[UnicodeCategory::ConnectorPunctuation], 1);  // _

    int total = 0;
    for (const auto& [_, n] : census) total += n;
    EXPECT_EQ(total, 128);
    EXPECT_EQ(census[UnicodeCategory::OtherNotAssigned], 0)
        << "no ASCII code point is unassigned";
}

TEST(CharUnicodeInfoClassificationTests, Fix2315_AsciiBoundaries) {
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x1F), UnicodeCategory::Control);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x20), UnicodeCategory::SpaceSeparator);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x21), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x2F), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x30), UnicodeCategory::DecimalDigitNumber);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x39), UnicodeCategory::DecimalDigitNumber);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x40), UnicodeCategory::OtherPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x41), UnicodeCategory::UppercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x5A), UnicodeCategory::UppercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x5B), UnicodeCategory::OpenPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x60), UnicodeCategory::ModifierSymbol);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x61), UnicodeCategory::LowercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7A), UnicodeCategory::LowercaseLetter);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7B), UnicodeCategory::OpenPunctuation);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7E), UnicodeCategory::MathSymbol);
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x7F), UnicodeCategory::Control);
}

// The frozen finding's own non-ASCII probe rows, with the values its MANAGED PROBE recorded.
// #2316 could only pin them as the declared reduction; #2315 answers every one of them, and
// each now matches what the finding said .NET returns. That is the closure of SR-AUD-174's
// table clause, expressed as the finding's own evidence rather than as new assertions.
TEST(CharUnicodeInfoClassificationTests, Fix2315_TheFindingsOwnProbeRowsNowAgreeWithDotNet) {
    const std::vector<std::pair<int, UnicodeCategory>> rows = {
        {0x00A0, UnicodeCategory::SpaceSeparator},     // NO-BREAK SPACE
        {0x00C9, UnicodeCategory::UppercaseLetter},    // LATIN CAPITAL LETTER E WITH ACUTE
        {0x0301, UnicodeCategory::NonSpacingMark},     // COMBINING ACUTE ACCENT
        {0x2014, UnicodeCategory::DashPunctuation},    // EM DASH
        {0x2028, UnicodeCategory::LineSeparator},      // LINE SEPARATOR
        {0xE000, UnicodeCategory::PrivateUse},         // the probe's own private-use row
        {0xFFFD, UnicodeCategory::OtherSymbol},        // REPLACEMENT CHARACTER
    };
    for (const auto& [cp, expected] : rows)
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), expected) << "U+" << std::hex << cp;
}

// #2316 pinned U+E000 as deliberately NOT PrivateUse, on the ground that one probed point is
// not a range and extending it would be an INFERENCE. That reasoning was right and its
// conclusion is now obsolete for the right reason: the range is no longer inferred, it is
// read from the UCD. All three private-use ranges answer, which is what distinguishes a table
// from the single-point generalisation #2316 refused to make.
TEST(CharUnicodeInfoClassificationTests, Fix2315_PrivateUseIsReadFromTheTableNotInferred) {
    for (int cp : {0xE000, 0xF8FF,                    // BMP private use area
                   0xF0000, 0xFFFFD,                  // Plane 15
                   0x100000, 0x10FFFD}) {             // Plane 16
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), UnicodeCategory::PrivateUse)
            << "U+" << std::hex << cp;
    }
    // ...and the code points just outside each range are not, which is what makes the above a
    // range rather than six lucky points.
    EXPECT_NE(CharUnicodeInfo::GetUnicodeCategory(0xDFFF), UnicodeCategory::PrivateUse);
    EXPECT_NE(CharUnicodeInfo::GetUnicodeCategory(0xF900), UnicodeCategory::PrivateUse);
    EXPECT_NE(CharUnicodeInfo::GetUnicodeCategory(0x10FFFE), UnicodeCategory::PrivateUse);
}

// ===========================================================================
// #2315 — the table itself: version, coverage and the corpora cross-check
//
// SA-4 requires the Unicode version to be "named in the generated header and
// in every test file", pinned until an explicit bump ticket, and every
// disagreement with the cross-check corpora listed rather than averaged away.
// These cases are that requirement expressed as assertions.
// ===========================================================================

TEST(CharUnicodeInfoTableTests, Fix2315_TheUnicodeVersionIsPinnedAtUcd16) {
    // SA-4 pins the version until an explicit bump ticket. If this fails, either the source
    // of record moved or someone regenerated against a different UCD -- both of which are
    // decisions, not maintenance, and both of which must move
    // docs/Migration-UnicodeCategoryTable.md's cross-check numbers with them.
    EXPECT_STREQ(System::Globalization::detail::kUnicodeVersion, "16.0");
}

TEST(CharUnicodeInfoTableTests, Fix2315_EveryCodePointIsAnswerableAndInRange) {
    // The trie must not walk off any of its three levels, and every answer must be a real
    // UnicodeCategory. Sweeping the whole code space is the only way to say that about a
    // three-level index; sampling cannot.
    std::map<UnicodeCategory, int> census;
    for (int cp = 0; cp <= 0x10FFFF; ++cp) {
        const auto cat = CharUnicodeInfo::GetUnicodeCategory(cp);
        ASSERT_GE(static_cast<int>(cat), 0) << "U+" << std::hex << cp;
        ASSERT_LE(static_cast<int>(cat), static_cast<int>(UnicodeCategory::OtherNotAssigned))
            << "U+" << std::hex << cp;
        ++census[cat];
    }
    int total = 0;
    for (const auto& [_, n] : census) total += n;
    ASSERT_EQ(total, 1114112);

    // The counts, DECOMPOSED rather than aggregated -- and the decomposition is what makes
    // this external evidence rather than the table agreeing with itself. Subtract the three
    // categories that are ranges rather than characters (private use, surrogates, controls)
    // and what is left is the count of graphic and format characters:
    //
    //   1,114,112 - 819,533 Cn = 294,579 assigned
    //   294,579 - 137,468 Co - 2,048 Cs - 65 Cc = 154,998
    //
    // and **154,998 is the number of characters Unicode 16.0 itself publishes**. An
    // all-unassigned table, an off-by-one level index, or a regeneration against a different
    // UCD all move it.
    const int cn = census[UnicodeCategory::OtherNotAssigned];
    const int co = census[UnicodeCategory::PrivateUse];
    const int cs = census[UnicodeCategory::Surrogate];
    const int cc = census[UnicodeCategory::Control];
    EXPECT_EQ(cn, 819533);
    EXPECT_EQ(co, 137468);   // 6,400 in the BMP plus two full planes less two noncharacters
    EXPECT_EQ(cs, 2048);     // U+D800..U+DFFF, and Char::IsSurrogate agrees (case above)
    EXPECT_EQ(cc, 65);       // C0 and C1
    EXPECT_EQ(1114112 - cn - co - cs - cc, 154998)
        << "this is Unicode 16.0's own published character count";
}

TEST(CharUnicodeInfoTableTests, Fix2315_TheCategoriesAreNotAllTheSame) {
    // The control for the sweep above: a table that answered one value everywhere would pass
    // a range check. All thirty categories must actually occur.
    std::map<UnicodeCategory, int> seen;
    for (int cp = 0; cp <= 0x10FFFF; ++cp) ++seen[CharUnicodeInfo::GetUnicodeCategory(cp)];
    EXPECT_EQ(seen.size(), 30u) << "every UnicodeCategory value must be reachable";
}

TEST(CharUnicodeInfoTableTests, Fix2315_SpotRowsAcrossEveryCategory) {
    // One row per category, so a mutation that corrupts a single level-3 entry has somewhere
    // to show up. Values are the UCD's, cross-checked against Python 3.13.5 unicodedata.
    const std::vector<std::pair<int, UnicodeCategory>> rows = {
        {0x0041, UnicodeCategory::UppercaseLetter},        {0x0061, UnicodeCategory::LowercaseLetter},
        {0x01C5, UnicodeCategory::TitlecaseLetter},        {0x02B0, UnicodeCategory::ModifierLetter},
        {0x00AA, UnicodeCategory::OtherLetter},            {0x0300, UnicodeCategory::NonSpacingMark},
        {0x0903, UnicodeCategory::SpacingCombiningMark},   {0x0488, UnicodeCategory::EnclosingMark},
        {0x0030, UnicodeCategory::DecimalDigitNumber},     {0x16EE, UnicodeCategory::LetterNumber},
        {0x00B2, UnicodeCategory::OtherNumber},            {0x0020, UnicodeCategory::SpaceSeparator},
        {0x2028, UnicodeCategory::LineSeparator},          {0x2029, UnicodeCategory::ParagraphSeparator},
        {0x0000, UnicodeCategory::Control},                {0x00AD, UnicodeCategory::Format},
        {0xD800, UnicodeCategory::Surrogate},              {0xE000, UnicodeCategory::PrivateUse},
        {0x005F, UnicodeCategory::ConnectorPunctuation},   {0x002D, UnicodeCategory::DashPunctuation},
        {0x0028, UnicodeCategory::OpenPunctuation},        {0x0029, UnicodeCategory::ClosePunctuation},
        {0x00AB, UnicodeCategory::InitialQuotePunctuation},{0x00BB, UnicodeCategory::FinalQuotePunctuation},
        {0x0021, UnicodeCategory::OtherPunctuation},       {0x002B, UnicodeCategory::MathSymbol},
        {0x0024, UnicodeCategory::CurrencySymbol},         {0x005E, UnicodeCategory::ModifierSymbol},
        {0x00A6, UnicodeCategory::OtherSymbol},            {0x0378, UnicodeCategory::OtherNotAssigned},
    };
    EXPECT_EQ(rows.size(), 30u) << "one row per category, or the coverage claim is false";
    for (const auto& [cp, expected] : rows)
        EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(cp), expected) << "U+" << std::hex << cp;
}

TEST(CharUnicodeInfoTableTests, Fix2315_TheCorporaDisagreementsAreExactlyTheOnesRecorded) {
    // SA-4: "every disagreement must be listed in the design record rather than averaged
    // away". The full listing is docs/Migration-UnicodeCategoryTable.md; what is asserted
    // here is the SHAPE of the disagreement, because that is the part a future regeneration
    // could silently change.
    //
    // Against Python 3.13.5 (UCD 15.1.0): 5,186 disagreements, 5,185 of them code points
    // ASSIGNED in 16.0 and unassigned in 15.1, and exactly ONE genuine reclassification.
    // Against Perl 5.40.1 unicore (UCD 15.0.0): 5,813, again with exactly one.
    // The same one, in both: U+1171E AHOM CONSONANT SIGN MEDIAL RA, Mn -> Mc in Unicode 16.0.
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x1171E),
              UnicodeCategory::SpacingCombiningMark)
        << "the single cross-corpus reclassification; both corpora say NonSpacingMark and "
           "both are at an older UCD than the pinned one";

    // The 627-code-point gap between the two disagreement counts is Unicode 15.1's own
    // additions, which Python has and Perl does not -- so the two corpora corroborate each
    // other rather than merely both differing from the table. One 15.1 addition, asserted so
    // that claim is not just arithmetic in a document.
    EXPECT_EQ(CharUnicodeInfo::GetUnicodeCategory(0x2FFC), UnicodeCategory::OtherSymbol)
        << "U+2FFC was added in Unicode 15.1";
}

// ===========================================================================
// #2336 — Numeric_Type / Numeric_Value, the second table SA-4 unlocks
// ===========================================================================

TEST(CharUnicodeInfoNumericTests, Fix2336_TheFindingsOwnRowsNowAgreeWithDotNet) {
    // SR-AUD-173's own examples, which the header documented as answering -1 under the
    // declared reduction. Each now answers what the finding said .NET answers.
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'\u0665'), 5);   // ARABIC-INDIC DIGIT FIVE
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u216B'), 12.0);  // ROMAN NUMERAL TWELVE
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2153'), 1.0 / 3.0, 1e-9);  // ONE THIRD

    // The sixteen code points the reduction did cover are unchanged, which is what says this
    // is a widening rather than a replacement.
    for (char16_t c = u'0'; c <= u'9'; ++c) {
        EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(c), c - u'0');
        EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(c), static_cast<double>(c - u'0'));
    }
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u00B9'), 1);
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u00B2'), 2);
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u00B3'), 3);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u00BC'), 0.25);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u00BD'), 0.5);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u00BE'), 0.75);
}

TEST(CharUnicodeInfoNumericTests, Fix2336_DecimalAndDigitAreDifferentPropertiesNotOneBuiltOnTheOther) {
    // They are two NIBBLES of the same table byte -- the high one and the low one
    // (CharUnicodeInfo.cs:151, 185) -- so they are independent properties. The old code
    // computed GetDigitValue as "decimal value, else three hard-coded superscripts", which
    // happened to agree over the thirteen code points it covered and cannot in general.
    //
    // The superscripts are the discriminator: Numeric_Type is Digit, not Decimal.
    for (char16_t c : {u'\u00B9', u'\u00B2', u'\u00B3'}) {
        EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(c), -1) << "Numeric_Type is Digit";
        EXPECT_NE(CharUnicodeInfo::GetDigitValue(c), -1);
    }
    // U+2460 CIRCLED DIGIT ONE is Numeric_Type=Digit in the same way, and was -1 for both
    // before this ticket.
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'\u2460'), -1);
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u2460'), 1);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u2460'), 1.0);

    // ...and a Numeric_Type=Numeric character is neither: U+216B has a value but no digit.
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'\u216B'), -1);
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u216B'), -1);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u216B'), 12.0);

    // A decimal digit is all three, which is the row that fails if the nibbles are swapped.
    EXPECT_EQ(CharUnicodeInfo::GetDecimalDigitValue(u'\u0665'), 5);
    EXPECT_EQ(CharUnicodeInfo::GetDigitValue(u'\u0665'), 5);
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u0665'), 5.0);
}

TEST(CharUnicodeInfoNumericTests, Fix2336_TheRationalsAreRealRationals) {
    // The reason the table is doubles and not a digit lookup, and the reason a "fraction
    // table" of quarters and halves would not have done.
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2153'), 1.0 / 3.0, 1e-9);   // 1/3
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2154'), 2.0 / 3.0, 1e-9);   // 2/3
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2159'), 1.0 / 6.0, 1e-9);   // 1/6
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2150'), 1.0 / 7.0, 1e-9);   // 1/7
    EXPECT_NEAR(CharUnicodeInfo::GetNumericValue(u'\u2151'), 1.0 / 9.0, 1e-9);   // 1/9
    // The ONLY negative numeric value in the BMP, and the reason -1 cannot be the sentinel
    // for "no value" in a naive reading: U+0F33 TIBETAN DIGIT HALF ZERO really is -0.5, so a
    // caller must not test `value < 0`. It is -0.5 and not -1.0, so the sentinel survives --
    // but only just, and that is worth an assertion rather than an assumption.
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u0F33'), -0.5);
    // ...and U+2189 VULGAR FRACTION ZERO THIRDS is exactly 0.0, which a "nonzero means it has
    // a value" test would get wrong in the other direction.
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u2189'), 0.0);
    // Large values, where a byte-sized digit table would have been impossible.
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u4EBF'), -1.0)
        << "CJK ideographs carry Unihan values .NET's table deliberately omits";
    EXPECT_DOUBLE_EQ(CharUnicodeInfo::GetNumericValue(u'\u2188'), 100000.0);  // ROMAN 100000
}

TEST(CharUnicodeInfoNumericTests, Fix2336_TheCountsAndTheCorporaCrossCheck) {
    // SA-4's cross-check for this table. Counts over the BMP, since these overloads take a
    // charcs; the whole-code-space figures are in docs/Migration-UnicodeNumericTable.md.
    int decimals = 0, digits = 0, numerics = 0;
    for (int cp = 0; cp <= 0xFFFF; ++cp) {
        const auto c = static_cast<SharpRuntime::charcs>(cp);
        if (CharUnicodeInfo::GetDecimalDigitValue(c) != -1) ++decimals;
        if (CharUnicodeInfo::GetDigitValue(c) != -1) ++digits;
        if (CharUnicodeInfo::GetNumericValue(c) != -1.0) ++numerics;
    }
    // Decimal is a subset of Digit is a subset of Numeric -- a structural property of
    // Numeric_Type that no single row can express, and one an off-by-one nibble breaks.
    EXPECT_LE(decimals, digits);
    EXPECT_LE(digits, numerics);
    EXPECT_EQ(decimals, 370);
    EXPECT_EQ(digits, 465);
    EXPECT_EQ(numerics, 742);

    // Every value is in range: a decimal digit is 0-9, a digit is 0-9.
    for (int cp = 0; cp <= 0xFFFF; ++cp) {
        const auto c = static_cast<SharpRuntime::charcs>(cp);
        const auto d = CharUnicodeInfo::GetDecimalDigitValue(c);
        const auto g = CharUnicodeInfo::GetDigitValue(c);
        ASSERT_TRUE(d == -1 || (d >= 0 && d <= 9)) << "U+" << std::hex << cp << " decimal " << d;
        ASSERT_TRUE(g == -1 || (g >= 0 && g <= 9)) << "U+" << std::hex << cp << " digit " << g;
    }
}
