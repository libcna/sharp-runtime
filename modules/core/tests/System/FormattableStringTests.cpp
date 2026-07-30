// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/FormattableString.hpp"

using System::FormattableString;

TEST(FormattableStringTests2, FormatProperty_ReturnsFormat) {
    FormattableString fs("Hello {0}!", {"World"});
    EXPECT_EQ(fs.getFormatProperty(), "Hello {0}!");
}

TEST(FormattableStringTests2, ArgumentCount_One) {
    FormattableString fs("{0}", {"x"});
    EXPECT_EQ(fs.getArgumentCountProperty(), 1);
}

TEST(FormattableStringTests2, ArgumentCount_Zero) {
    FormattableString fs("no args");
    EXPECT_EQ(fs.getArgumentCountProperty(), 0);
}

TEST(FormattableStringTests2, GetArgument_ReturnsCorrect) {
    FormattableString fs("{0} {1}", {"hello", "world"});
    EXPECT_EQ(fs.GetArgument(0), "hello");
    EXPECT_EQ(fs.GetArgument(1), "world");
}

TEST(FormattableStringTests2, GetArguments_ReturnsAll) {
    FormattableString fs("{0}", {"a"});
    auto args = fs.GetArguments();
    ASSERT_EQ(args.size(), 1u);
    EXPECT_EQ(args[0], "a");
}

TEST(FormattableStringTests2, ToString_SubstitutesPlaceholders) {
    FormattableString fs("Hello {0}!", {"World"});
    EXPECT_EQ(fs.ToString(), "Hello World!");
}

TEST(FormattableStringTests2, ToString_TwoPlaceholders) {
    FormattableString fs("{0} + {1}", {"2", "3"});
    EXPECT_EQ(fs.ToString(), "2 + 3");
}

TEST(FormattableStringTests2, ToString_NoPlaceholders) {
    FormattableString fs("static text");
    EXPECT_EQ(fs.ToString(), "static text");
}

TEST(FormattableStringTests2, Invariant_SameAsToString) {
    FormattableString fs("value={0}", {"42"});
    EXPECT_EQ(FormattableString::Invariant(fs), fs.ToString());
}

TEST(FormattableStringTests2, CurrentCulture_SameAsToString) {
    FormattableString fs("x={0}", {"1"});
    EXPECT_EQ(FormattableString::CurrentCulture(fs), fs.ToString());
}

TEST(FormattableStringTests2, GetArgument_OutOfRange_Throws) {
    FormattableString fs("{0}", {"a"});
    EXPECT_THROW(fs.GetArgument(5), System::IndexOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Composite-format boundary — ticket #1883 (SR-AUD-015 extended, CCF-012)
//
// ToString used to run one full find/replace sweep over the *result* per
// argument index, so text inserted for an earlier index was re-read as format
// syntax while a later index was substituted. It is now one left-to-right pass
// over the format string that appends to a separate output.
//
// The accepted grammar is deliberately UNCHANGED by this ticket. The
// "PinsCurrentGrammar" group asserts today's behaviour precisely so that ticket
// #1884 (which adopts .NET's grammar and needs explicit user approval) cannot
// land silently. Design record: docs/CompositeFormatBoundaryPlan.md.
// ---------------------------------------------------------------------------
#include "System/IFormatProvider.hpp"

// --- Argument isolation: this is the defect the ticket repairs ---

TEST(FormattableStringBoundaryTests, ArgumentTextIsNotReinterpretedAsAFormatItem) {
    // Was "second": argument 1 overwrote the literal text argument 0 had inserted.
    FormattableString fs("{0}", {"{1}", "second"});
    EXPECT_EQ(fs.ToString(), "{1}");
}

TEST(FormattableStringBoundaryTests, ArgumentNamingALaterIndex_MultiplePositions) {
    FormattableString fs("{0}|{1}", {"{1}", "second"});
    EXPECT_EQ(fs.ToString(), "{1}|second");
}

TEST(FormattableStringBoundaryTests, ArgumentNamingItsOwnIndex) {
    FormattableString fs("{0}", {"{0}"});
    EXPECT_EQ(fs.ToString(), "{0}");
}

TEST(FormattableStringBoundaryTests, ArgumentNamingAnEarlierIndex) {
    FormattableString fs("{0}{1}", {"A", "{0}"});
    EXPECT_EQ(fs.ToString(), "A{0}");
}

TEST(FormattableStringBoundaryTests, ArgumentContainingBraceRunsIsEmittedVerbatim) {
    FormattableString fs("<{0}>", {"{{a}}"});
    EXPECT_EQ(fs.ToString(), "<{{a}}>");
    FormattableString fs2("<{0}>", {"}{"});
    EXPECT_EQ(fs2.ToString(), "<}{>");
}

TEST(FormattableStringBoundaryTests, ArgumentEqualToTheWholeFormat) {
    FormattableString fs("{0}{1}", {"{0}{1}", "x"});
    EXPECT_EQ(fs.ToString(), "{0}{1}x");
}

// --- Multi-digit indices must not be aliased by a single-digit token ---

TEST(FormattableStringBoundaryTests, MultiDigitIndexResolves) {
    std::vector<std::string> args;
    for (int i = 0; i < 11; ++i) args.push_back("a" + std::to_string(i));
    EXPECT_EQ(FormattableString("{10}", args).ToString(), "a10");
    EXPECT_EQ(FormattableString("{1}{10}", args).ToString(), "a1a10");
    EXPECT_EQ(FormattableString("{0}{10}{1}", args).ToString(), "a0a10a1");
}

TEST(FormattableStringBoundaryTests, AbsurdIndexRunDoesNotOverflow) {
    FormattableString fs("{99999999999999999999}", {"a"});
    EXPECT_EQ(fs.ToString(), "{99999999999999999999}");
}

// --- Preserved behaviour (the section 9.3 invariant) ---

TEST(FormattableStringBoundaryTests, PreservedSuccess_OrdinarySubstitution) {
    EXPECT_EQ(FormattableString("Hello {0}!", {"World"}).ToString(), "Hello World!");
    EXPECT_EQ(FormattableString("{0} and {1}", {"a", "b"}).ToString(), "a and b");
    EXPECT_EQ(FormattableString("{1}{0}", {"A", "B"}).ToString(), "BA");
    EXPECT_EQ(FormattableString("{0}{0}", {"z"}).ToString(), "zz");
    EXPECT_EQ(FormattableString("no items", {"a"}).ToString(), "no items");
    EXPECT_EQ(FormattableString("", {"a"}).ToString(), "");
    EXPECT_EQ(FormattableString("{0}", {""}).ToString(), "");
}

TEST(FormattableStringBoundaryTests, PinsCurrentGrammar_BracesAreNotEscapes) {
    // .NET returns "{0}". Adopting that grammar is ticket #1884.
    EXPECT_EQ(FormattableString("{{0}}", {"value"}).ToString(), "{value}");
}

TEST(FormattableStringBoundaryTests, PinsCurrentGrammar_MissingIndexStaysLiteral) {
    // .NET throws FormatException. #1884.
    EXPECT_EQ(FormattableString("{1}", {"only"}).ToString(), "{1}");
    EXPECT_EQ(FormattableString("{0}{5}", {"a"}).ToString(), "a{5}");
}

TEST(FormattableStringBoundaryTests, PinsCurrentGrammar_StrayClosingBraceIsLiteral) {
    // .NET throws FormatException. #1884.
    EXPECT_EQ(FormattableString("value}", {"a"}).ToString(), "value}");
}

TEST(FormattableStringBoundaryTests, PinsCurrentGrammar_AlignmentAndSpecifierStayLiteral) {
    // .NET pads and applies the specifier. #1884.
    EXPECT_EQ(FormattableString("{0,6}", {"a"}).ToString(), "{0,6}");
    EXPECT_EQ(FormattableString("{0:D4}", {"a"}).ToString(), "{0:D4}");
}

TEST(FormattableStringBoundaryTests, LoneBracesAtBufferEdges) {
    EXPECT_EQ(FormattableString("{", {"a"}).ToString(), "{");
    EXPECT_EQ(FormattableString("}", {"a"}).ToString(), "}");
    EXPECT_EQ(FormattableString("{0", {"a"}).ToString(), "{0");
    EXPECT_EQ(FormattableString("{}", {"a"}).ToString(), "{}");
}

TEST(FormattableStringBoundaryTests, NoArgumentsAtAll) {
    EXPECT_EQ(FormattableString("{0}", {}).ToString(), "{0}");
    EXPECT_EQ(FormattableString("plain", {}).ToString(), "plain");
}

// --- Every entry that reaches ToString honours a subclass override ---

namespace {
class OverridingFormattableString : public FormattableString {
public:
    using FormattableString::FormattableString;
    [[nodiscard]] std::string ToString() const override { return "overridden"; }
};
} // namespace

TEST(FormattableStringBoundaryTests, StaticWrappersHonourAnOverride) {
    const OverridingFormattableString fs("{0}", {"{1}", "second"});
    EXPECT_EQ(fs.ToString(), "overridden");
    EXPECT_EQ(FormattableString::Invariant(fs), "overridden");
    EXPECT_EQ(FormattableString::CurrentCulture(fs), "overridden");
}

TEST(FormattableStringBoundaryTests, ProviderOverloadMatchesTheNoArgumentOverload) {
    FormattableString fs("{0}{1}", {"{1}", "X"});
    EXPECT_EQ(fs.ToString(nullptr), fs.ToString());
    EXPECT_EQ(fs.ToString(nullptr), "{1}X");
}

TEST(FormattableStringBoundaryTests, StaticWrappersMatchTheBaseBody) {
    FormattableString fs("{0}", {"{1}", "second"});
    EXPECT_EQ(FormattableString::Invariant(fs), "{1}");
    EXPECT_EQ(FormattableString::CurrentCulture(fs), "{1}");
}

TEST(FormattableStringBoundaryTests, ArgumentAccessBoundsAreUnchanged) {
    FormattableString fs("{0}", {"a"});
    EXPECT_THROW(fs.GetArgument(-1), System::IndexOutOfRangeException);
    EXPECT_THROW(fs.GetArgument(1), System::IndexOutOfRangeException);
    EXPECT_EQ(fs.GetArgument(0), "a");
    EXPECT_EQ(fs.getArgumentCountProperty(), 1);
}

TEST(FormattableStringBoundaryTests, LargeFormatWithItemAtTheEnd) {
    std::string big(100000, 'a');
    big += "{0}";
    const std::string r = FormattableString(big, {"tail"}).ToString();
    EXPECT_EQ(r.size(), 100004u);
    EXPECT_EQ(r.substr(100000), "tail");
}
