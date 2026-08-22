// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Coverage for XmlQualifiedName, XmlConvert, NameTable, and the small XML interfaces.
#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <mutex>
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"
#include "System/TimeSpan.hpp"
#include "System/Xml/IHasXmlNode.hpp"
#include "System/Xml/IXmlLineInfo.hpp"
#include "System/Xml/IXmlNamespaceResolver.hpp"
#include "System/Xml/NameTable.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/DateTime.hpp"
#include "System/DateTimeKind.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/TimeZone.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"
#include "System/Xml/XmlDateTimeSerializationMode.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlNodeChangedEventArgs.hpp"
#include "System/Xml/XmlQualifiedName.hpp"

using namespace System::Xml;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
namespace {

class ScopedXmlTimeZone final {
    std::string saved_;
    bool wasSet_;

public:
    explicit ScopedXmlTimeZone(const char* zone) {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        const char* current = ::getenv("TZ");
        wasSet_ = current != nullptr;
        if (wasSet_) saved_ = current;
        ::setenv("TZ", zone, 1);
        ::tzset();
    }

    ScopedXmlTimeZone(const ScopedXmlTimeZone&) = delete;
    ScopedXmlTimeZone& operator=(const ScopedXmlTimeZone&) = delete;

    ~ScopedXmlTimeZone() {
        std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
        if (wasSet_) ::setenv("TZ", saved_.c_str(), 1);
        else         ::unsetenv("TZ");
        ::tzset();
    }
};

} // namespace
#endif

// ===========================================================================
// XmlQualifiedName
// ===========================================================================

TEST(XmlQualifiedNameTests, DefaultCtor_IsEmpty) {
    XmlQualifiedName qn;
    EXPECT_TRUE(qn.getIsEmptyProperty());
}

TEST(XmlQualifiedNameTests, NameOnlyCtor_NamespaceIsEmpty) {
    XmlQualifiedName qn("foo");
    EXPECT_EQ(qn.getNameProperty(), "foo");
    EXPECT_EQ(qn.getNamespaceProperty(), "");
    EXPECT_FALSE(qn.getIsEmptyProperty());
}

TEST(XmlQualifiedNameTests, ToString_WithNamespace_UsesColonForm) {
    XmlQualifiedName qn("foo", "urn:bar");
    EXPECT_EQ(qn.ToString(), "urn:bar:foo");
}

TEST(XmlQualifiedNameTests, ToString_WithoutNamespace_IsJustName) {
    XmlQualifiedName qn("foo");
    EXPECT_EQ(qn.ToString(), "foo");
}

TEST(XmlQualifiedNameTests, Equals_SameNameAndNamespace) {
    XmlQualifiedName a("foo", "urn:bar");
    XmlQualifiedName b("foo", "urn:bar");
    EXPECT_TRUE(a == b);
}

TEST(XmlQualifiedNameTests, Equals_DifferentNamespace_NotEqual) {
    XmlQualifiedName a("foo", "urn:bar");
    XmlQualifiedName b("foo", "urn:baz");
    EXPECT_TRUE(a != b);
}

TEST(XmlQualifiedNameTests, Empty_IsStaticEmptyInstance) {
    EXPECT_TRUE(XmlQualifiedName::Empty.getIsEmptyProperty());
}

// ===========================================================================
// NameTable
// ===========================================================================

TEST(NameTableTests, Add_ThenGet_ReturnsAtomizedString) {
    NameTable nt;
    nt.Add("foo");
    auto result = nt.Get(std::string("foo"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "foo");
}

TEST(NameTableTests, Get_NotAdded_ReturnsNullopt) {
    NameTable nt;
    EXPECT_FALSE(nt.Get(std::string("missing")).has_value());
}

TEST(NameTableTests, Add_SubstringOverload) {
    NameTable nt;
    const char* buf = "helloworld";
    std::string result = nt.Add(buf, 0, 5);
    EXPECT_EQ(result, "hello");
    EXPECT_TRUE(nt.Get(std::string("hello")).has_value());
}

// ===========================================================================
// XmlConvert — name encoding
// ===========================================================================

TEST(XmlConvertTests, EncodeName_ValidName_Unchanged) {
    EXPECT_EQ(XmlConvert::EncodeName("valid_name"), "valid_name");
}

TEST(XmlConvertTests, EncodeName_InvalidChar_Escaped) {
    std::string encoded = XmlConvert::EncodeName("a b");
    EXPECT_EQ(encoded, "a_x0020_b");
}

TEST(XmlConvertTests, DecodeName_ReversesEncodeName) {
    std::string original = "a b";
    std::string encoded = XmlConvert::EncodeName(original);
    EXPECT_EQ(XmlConvert::DecodeName(encoded), original);
}

TEST(XmlConvertTests, VerifyName_Valid_ReturnsUnchanged) {
    EXPECT_EQ(XmlConvert::VerifyName("foo:bar"), "foo:bar");
}

TEST(XmlConvertTests, VerifyName_StartsWithDigit_Throws) {
    EXPECT_THROW(XmlConvert::VerifyName("1abc"), XmlException);
}

TEST(XmlConvertTests, VerifyNCName_ContainsColon_Throws) {
    EXPECT_THROW(XmlConvert::VerifyNCName("foo:bar"), XmlException);
}

TEST(XmlConvertTests, VerifyName_Empty_ThrowsArgumentException) {
    EXPECT_THROW(XmlConvert::VerifyName(""), System::ArgumentException);
}

TEST(XmlConvertTests, VerifyNCName_Empty_ThrowsArgumentException) {
    EXPECT_THROW(XmlConvert::VerifyNCName(""), System::ArgumentException);
}

TEST(XmlConvertTests, VerifyNMTOKEN_Empty_ThrowsXmlException) {
    EXPECT_THROW(XmlConvert::VerifyNMTOKEN(""), XmlException);
}

TEST(XmlConvertTests, VerifyNMTOKEN_BadChar_ThrowsXmlException) {
    EXPECT_THROW(XmlConvert::VerifyNMTOKEN("foo bar"), XmlException);
}

TEST(XmlConvertTests, VerifyNMTOKEN_Valid_ReturnsUnchanged) {
    EXPECT_EQ(XmlConvert::VerifyNMTOKEN("foo.bar-1"), "foo.bar-1");
}

TEST(XmlConvertTests, VerifyTOKEN_Empty_ReturnsUnchanged) {
    EXPECT_EQ(XmlConvert::VerifyTOKEN(""), "");
}

TEST(XmlConvertTests, VerifyTOKEN_LeadingSpace_Throws) {
    EXPECT_THROW(XmlConvert::VerifyTOKEN(" foo"), XmlException);
}

TEST(XmlConvertTests, VerifyTOKEN_TrailingSpace_Throws) {
    EXPECT_THROW(XmlConvert::VerifyTOKEN("foo "), XmlException);
}

TEST(XmlConvertTests, VerifyTOKEN_DoubleSpace_Throws) {
    EXPECT_THROW(XmlConvert::VerifyTOKEN("foo  bar"), XmlException);
}

TEST(XmlConvertTests, VerifyTOKEN_Tab_Throws) {
    EXPECT_THROW(XmlConvert::VerifyTOKEN("foo\tbar"), XmlException);
}

TEST(XmlConvertTests, VerifyTOKEN_Valid_ReturnsUnchanged) {
    EXPECT_EQ(XmlConvert::VerifyTOKEN("foo bar"), "foo bar");
}

TEST(XmlConvertTests, IsStartNCNameChar_Letter_True) {
    EXPECT_TRUE(XmlConvert::IsStartNCNameChar('a'));
}

TEST(XmlConvertTests, IsStartNCNameChar_Digit_False) {
    EXPECT_FALSE(XmlConvert::IsStartNCNameChar('1'));
}

TEST(XmlConvertTests, IsWhitespaceChar_Space_True) {
    EXPECT_TRUE(XmlConvert::IsWhitespaceChar(' '));
}

// ===========================================================================
// XmlConvert — ToString/ToXxx round-trips
// ===========================================================================

TEST(XmlConvertTests, BooleanRoundTrip) {
    EXPECT_EQ(XmlConvert::ToString(true), "true");
    EXPECT_EQ(XmlConvert::ToString(false), "false");
    EXPECT_TRUE(XmlConvert::ToBoolean("true"));
    EXPECT_TRUE(XmlConvert::ToBoolean("1"));
    EXPECT_FALSE(XmlConvert::ToBoolean("0"));
}

TEST(XmlConvertTests, ToBoolean_InvalidString_ThrowsFormatException) {
    EXPECT_THROW(XmlConvert::ToBoolean("yes"), System::FormatException);
}

TEST(XmlConvertTests, Int32RoundTrip) {
    EXPECT_EQ(XmlConvert::ToString(static_cast<SharpRuntime::intcs>(-42)), "-42");
    EXPECT_EQ(XmlConvert::ToInt32("-42"), -42);
}

TEST(XmlConvertTests, Int64RoundTrip) {
    SharpRuntime::longcs v = 1234567890123LL;
    EXPECT_EQ(XmlConvert::ToInt64(XmlConvert::ToString(v)), v);
}

TEST(XmlConvertTests, DoubleRoundTrip) {
    double v = 3.14159;
    EXPECT_DOUBLE_EQ(XmlConvert::ToDouble(XmlConvert::ToString(v)), v);
}

TEST(XmlConvertTests, ToString_Double_PositiveInfinity_UsesXmlSchemaToken) {
    EXPECT_EQ(XmlConvert::ToString(std::numeric_limits<double>::infinity()), "INF");
}

TEST(XmlConvertTests, ToString_Double_NegativeInfinity_UsesXmlSchemaToken) {
    EXPECT_EQ(XmlConvert::ToString(-std::numeric_limits<double>::infinity()), "-INF");
}

TEST(XmlConvertTests, ToString_Float_Infinity_UsesXmlSchemaToken) {
    EXPECT_EQ(XmlConvert::ToString(std::numeric_limits<float>::infinity()), "INF");
    EXPECT_EQ(XmlConvert::ToString(-std::numeric_limits<float>::infinity()), "-INF");
}

TEST(XmlConvertTests, ToDouble_ParsesXmlSchemaInfinityTokens) {
    EXPECT_EQ(XmlConvert::ToDouble("INF"), std::numeric_limits<double>::infinity());
    EXPECT_EQ(XmlConvert::ToDouble("-INF"), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(XmlConvert::ToDouble(" INF \t"), std::numeric_limits<double>::infinity());
}

TEST(XmlConvertTests, ToSingle_ParsesXmlSchemaInfinityTokens) {
    EXPECT_EQ(XmlConvert::ToSingle("INF"), std::numeric_limits<float>::infinity());
    EXPECT_EQ(XmlConvert::ToSingle("-INF"), -std::numeric_limits<float>::infinity());
}

TEST(XmlConvertTests, InfinityRoundTrip_Double) {
    double posInf = std::numeric_limits<double>::infinity();
    double negInf = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(XmlConvert::ToDouble(XmlConvert::ToString(posInf)), posInf);
    EXPECT_EQ(XmlConvert::ToDouble(XmlConvert::ToString(negInf)), negInf);
}

// Regression tests for ticket 350: ToSingle/ToDouble/ToDecimal computed a trimmed copy of the
// input for the INF/-INF token check but then called Parse() with the ORIGINAL untrimmed string
// for the general numeric fallback path. Single::Parse/Double::Parse delegate to
// std::from_chars, which -- unlike .NET's float.Parse/double.Parse -- does not skip leading or
// trailing whitespace at all; Decimal::TryParse tolerates no whitespace whatsoever either. XML
// element/attribute text content commonly has surrounding whitespace from document formatting
// (e.g. "<value> 3.14 </value>"), so these previously threw FormatException for perfectly valid,
// common XML Schema numeric content. Verified against XmlConvert.cs, whose ToSingle/ToDouble/
// ToDecimal all explicitly pass NumberStyles.AllowLeadingWhite | AllowTrailingWhite.
TEST(XmlConvertTests, ToDouble_TrimsSurroundingWhitespaceBeforeNumericParse) {
    EXPECT_DOUBLE_EQ(XmlConvert::ToDouble(" 3.14 "), 3.14);
    EXPECT_DOUBLE_EQ(XmlConvert::ToDouble("\t3.14\n"), 3.14);
}

TEST(XmlConvertTests, ToSingle_TrimsSurroundingWhitespaceBeforeNumericParse) {
    EXPECT_FLOAT_EQ(XmlConvert::ToSingle(" 3.14 "), 3.14f);
}

TEST(XmlConvertTests, ToDecimal_TrimsSurroundingWhitespaceBeforeNumericParse) {
    EXPECT_NO_THROW(XmlConvert::ToDecimal(" 3.14 "));
    EXPECT_EQ(XmlConvert::ToDecimal(" 3.14 ").ToString(), "3.14");
}

TEST(XmlConvertTests, GuidRoundTrip) {
    System::Guid g = System::Guid::NewGuid();
    EXPECT_EQ(XmlConvert::ToGuid(XmlConvert::ToString(g)), g);
}

TEST(XmlConvertTests, DateTimeRoundTrip) {
    System::DateTime dt = System::DateTime::Parse("2024-03-15T10:30:00");
    System::DateTime roundtripped = XmlConvert::ToDateTime(XmlConvert::ToString(dt));
    EXPECT_EQ(roundtripped.ToString(), dt.ToString());
}

// ===========================================================================
// XmlNodeChangedEventArgs
// ===========================================================================

TEST(XmlNodeChangedEventArgsTests, StoresConstructorArguments) {
    XmlNodeChangedEventArgs args(nullptr, nullptr, nullptr, "old", "new", XmlNodeChangedAction::Change);
    EXPECT_EQ(args.getActionProperty(), XmlNodeChangedAction::Change);
    EXPECT_EQ(args.getOldValueProperty(), "old");
    EXPECT_EQ(args.getNewValueProperty(), "new");
}

// ===========================================================================
// XmlConvert::ToTimeSpan -- the third public door onto TimeSpan's parse core
// ===========================================================================
//
// Ticket #1836 (SR-AUD-008, CCF-004 class C). docs/DefinedArithmeticBoundaryPlan.md section 15
// requires enumerating every public door onto a repaired site, including the ones in other
// modules. XmlConvert::ToTimeSpan used to forward straight to System::TimeSpan::Parse, so before
// that repair it returned a wrapped negative duration for a large positive day count.
//
// TICKET #2080 REPLACED THE GRAMMAR UNDER THIS DOOR. XmlConvert::ToTimeSpan now parses the XML
// Schema `duration` form, as .NET's does, and no longer accepts the native colon form at all --
// so these two cases are rewritten in the new grammar rather than deleted. The property #1836
// cares about is unchanged and is what they still assert: an out-of-range duration RAISES
// instead of wrapping to a negative value.

TEST(XmlConvertTests, ToTimeSpan_ValidDuration_1836) {
    // "1.02:03:04" in the old grammar; the same instant is "P1DT2H3M4S" in this one.
    const System::TimeSpan ts = XmlConvert::ToTimeSpan("P1DT2H3M4S");
    EXPECT_EQ(ts.getTicksProperty(), 937840000000LL);
}

TEST(XmlConvertTests, ToTimeSpan_DayCountBeyondRange_Throws_1836) {
    // #1836's property: an out-of-range day count must RAISE, not wrap to a negative duration.
    // The exception TYPE changed with the grammar and that is .NET's own behaviour --
    // XmlConvert.ToTimeSpan wraps every XsdDuration failure in a FormatException
    // ("Remap exception for v1 compatibility", XmlConvert.cs:1118-1122), so an overflow that
    // XsdDuration reports as OverflowException reaches the caller as FormatException.
    EXPECT_THROW((void)XmlConvert::ToTimeSpan("P2147483647DT0H0M0S"), System::FormatException);
    // ...and the wrapped value is not produced under any spelling.
    EXPECT_THROW((void)XmlConvert::ToTimeSpan("P999999999999999999999D"), System::FormatException);
}

// ===========================================================================
// #1945 -- the four XmlConvert arguments that were accepted and discarded
// ===========================================================================
//
// Two `format` parameters and two `XmlDateTimeSerializationMode` parameters were spelled
// `/*format*/` and `/*mode*/` and thrown away, so a caller could hand `ToDateTime(s, "HH:mm:ss")`
// a date and get it back -- parsed by an entirely different grammar, with no diagnostic. That is
// the SR-AUD-168 shape four times over.
//
// The mode half additionally carried a premise that had STOPPED BEING TRUE: its comment said
// `System::DateTime` does not track a `DateTimeKind`. #1941 phase 1 gave it one and phase 2 made
// it convert by that kind.

TEST(XmlConvertDateTimeMode1945Tests, TheFormatIsHonouredRatherThanDiscarded) {
    using System::Xml::XmlConvert;

    EXPECT_EQ(XmlConvert::ToDateTime("2024-06-15 13:45:30", "yyyy-MM-dd HH:mm:ss"),
              System::DateTime(2024, 6, 15, 13, 45, 30));

    // THE ROW THAT SEPARATES "HONOURED" FROM "DISCARDED": a string the general parser accepts but
    // the FORMAT does not. Before this it returned a value; now it throws. A test that only
    // asserted a matching pair would pass against the discarding body.
    EXPECT_THROW(XmlConvert::ToDateTime("2024-06-15", "HH:mm:ss"), System::FormatException);
    EXPECT_THROW(XmlConvert::ToDateTime("2024-06-15T13:45:30", "yyyy-MM-dd"),
                 System::FormatException);

    // XmlConvert's exact door deliberately permits OUTER whitespace. Calling the convenient
    // zone-less DateTime::ParseExact overload loses that XmlConvert-specific style contract.
    const auto surrounded = XmlConvert::ToDateTime(
        " \t2024-06-15 13:45:30\r\n", "yyyy-MM-dd HH:mm:ss");
    EXPECT_EQ(surrounded, System::DateTime(2024, 6, 15, 13, 45, 30));
}

TEST(XmlConvertDateTimeMode1945Tests, TheModeMatrixStampsTwiceAndConvertsTwice) {
    using System::Xml::XmlConvert;
    using System::Xml::XmlDateTimeSerializationMode;
    using System::DateTimeKind;

    const System::DateTime unspecified(2024, 6, 15, 12, 0, 0);
    ASSERT_EQ(unspecified.getKindProperty(), DateTimeKind::Unspecified);
    const auto utc   = System::DateTime::SpecifyKind(unspecified, DateTimeKind::Utc);
    const auto local = System::DateTime::SpecifyKind(unspecified, DateTimeKind::Local);

    // STAMPING cells: the kind changes and THE TICKS DO NOT. A body that converted here instead
    // would move an unspecified value by the local offset, which is wrong in .NET too.
    EXPECT_EQ(XmlConvert::ToDateTime(unspecified.ToString(),
                                     XmlDateTimeSerializationMode::Utc).getTicksProperty(),
              unspecified.getTicksProperty());
    EXPECT_EQ(XmlConvert::ToDateTime(unspecified.ToString(),
                                     XmlDateTimeSerializationMode::Utc).getKindProperty(),
              DateTimeKind::Utc);
    EXPECT_EQ(XmlConvert::ToDateTime(unspecified.ToString(),
                                     XmlDateTimeSerializationMode::Local).getKindProperty(),
              DateTimeKind::Local);

    // IDENTITY cells: same kind in, unchanged out.
    EXPECT_EQ(XmlConvert::ToString(utc, XmlDateTimeSerializationMode::Utc),
              XmlConvert::ToString(utc, XmlDateTimeSerializationMode::RoundtripKind));
    EXPECT_EQ(XmlConvert::ToString(local, XmlDateTimeSerializationMode::Local),
              XmlConvert::ToString(local, XmlDateTimeSerializationMode::RoundtripKind));

    // UNSPECIFIED strips the kind while keeping the ticks -- `new DateTime(Ticks, Unspecified)`.
    EXPECT_EQ(XmlConvert::ToString(utc, XmlDateTimeSerializationMode::Unspecified),
              XmlConvert::ToString(unspecified, XmlDateTimeSerializationMode::RoundtripKind));
}

// THE TWO CELLS THAT ACTUALLY MOVE THE TICKS, and the reason this ticket could land before #1942:
// `Core.Base` cannot name a time zone, so #1941 phase 2 had to take one as a
// parameter -- but `modules/xml` CAN, `TimeZone` depending on `Core.Base` alone. So XmlConvert's
// signatures stay exactly .NET's, with no zone parameter, because none is needed here.
TEST(XmlConvertDateTimeMode1945Tests, UtcAndLocalConvertAgainstThisProcessZone) {
    using System::Xml::XmlConvert;
    using System::Xml::XmlDateTimeSerializationMode;
    using System::DateTimeKind;

    const System::DateTime base(2024, 6, 15, 12, 0, 0);
    const auto utc    = System::DateTime::SpecifyKind(base, DateTimeKind::Utc);
    const auto offset = System::TimeZone::CurrentTimeZone().GetUtcOffset(base);

    // Utc -> Local is a CONVERSION: the rendered text moves by this zone's offset, where the
    // identity cell does not. Comparing the two RENDERINGS is what makes the conversion visible
    // without a literal -- a fixed expectation would be the tzdata mistake #2351 repaired, and
    // this way the case is also correct in a UTC container, where both strings simply agree.
    const auto asLocalText = XmlConvert::ToString(utc, XmlDateTimeSerializationMode::Local);
    const auto identity    = XmlConvert::ToString(utc, XmlDateTimeSerializationMode::RoundtripKind);
    const auto shifted     = XmlConvert::ToString(
        System::DateTime::SpecifyKind(base.AddTicks(offset.getTicksProperty()),
                                      DateTimeKind::Local),
        XmlDateTimeSerializationMode::RoundtripKind);
    EXPECT_EQ(asLocalText, shifted);
    if (offset.getTicksProperty() != 0) { EXPECT_NE(asLocalText, identity); }

    // ...and Local -> Utc is its inverse, so the pair returns to the instant it started from.
    // A one-directional cell cannot show that.
    const auto local = System::DateTime::SpecifyKind(
        base.AddTicks(offset.getTicksProperty()), DateTimeKind::Local);
    EXPECT_EQ(XmlConvert::ToString(local, XmlDateTimeSerializationMode::Utc), identity);
}

// INVERTED BY SA-16.3, NOT DELETED. #1945 measured that a kind could not cross a string here and
// DECLARED it, writing that the pin "fails the day #1942 teaches Parse to read a Z". The decision
// went further than that sentence: the reading half does not go through `DateTime::Parse` at all.
//
// SA-16.4 left the general `Parse` alone -- it still parses a zone and DISCARDS it -- so a round
// trip built on it could never carry a kind however the writing half rendered. **.NET does not use
// `DateTime.Parse` here either**: `XmlConvert.ToDateTime` builds an `XsdDateTime`, which parses the
// zone itself. So the marker is split off before `Parse` ever sees the text, and the two halves
// now meet.
TEST(XmlConvertDateTimeMode1945Tests, RoundtripKindNowRoundtripsThroughAString) {
    using System::Xml::XmlConvert;
    using System::Xml::XmlDateTimeSerializationMode;
    using System::DateTimeKind;

    const System::DateTime base(2024, 6, 15, 12, 0, 0);
    const auto utc = System::DateTime::SpecifyKind(base, DateTimeKind::Utc);

    // THE WRITING HALF (SA-16.5): the full XsdDateTime form -- `T` AND the marker, two changes
    // rather than one, because an XSD `dateTime` literal requires the `T` and appending only the
    // marker would have repaired the round trip while leaving the document wrong.
    const std::string written =
        XmlConvert::ToString(utc, XmlDateTimeSerializationMode::RoundtripKind);
    EXPECT_EQ(written, "2024-06-15T12:00:00Z");

    // THE READING HALF: the kind comes back.
    const auto readBack =
        XmlConvert::ToDateTime(written, XmlDateTimeSerializationMode::RoundtripKind);
    EXPECT_EQ(readBack.getKindProperty(), DateTimeKind::Utc);
    EXPECT_EQ(readBack, utc);

    // AN UNSPECIFIED VALUE WRITES NO MARKER AND COMES BACK UNSPECIFIED, which is what stops a
    // value acquiring a kind it never had -- the easy over-correction in the other direction.
    const std::string plain =
        XmlConvert::ToString(base, XmlDateTimeSerializationMode::RoundtripKind);
    EXPECT_EQ(plain, "2024-06-15T12:00:00");
    EXPECT_EQ(XmlConvert::ToDateTime(plain, XmlDateTimeSerializationMode::RoundtripKind)
                  .getKindProperty(),
              DateTimeKind::Unspecified);

    // ...and `RoundtripKind` and `Unspecified` ARE NOW DISTINGUISHABLE, which #1945 recorded as a
    // proven equivalence and which its mutations M4 and M6 relied on. That equivalence is over.
    EXPECT_NE(XmlConvert::ToString(utc, XmlDateTimeSerializationMode::RoundtripKind),
              XmlConvert::ToString(utc, XmlDateTimeSerializationMode::Unspecified));
}

// The fraction is emitted only when non-zero and its trailing zeroes are TRIMMED, so `.5` rather
// than `.5000000`. Always writing seven digits would be a different literal for the same instant.
TEST(XmlConvertDateTimeMode1945Tests, TheFractionIsTrimmedAndOmittedWhenZero) {
    using System::Xml::XmlConvert;

    EXPECT_EQ(XmlConvert::ToString(System::DateTime(2024, 6, 15, 12, 0, 0)),
              "2024-06-15T12:00:00");
    EXPECT_EQ(XmlConvert::ToString(System::DateTime(2024, 6, 15, 12, 0, 0).AddTicks(5000000)),
              "2024-06-15T12:00:00.5");
    EXPECT_EQ(XmlConvert::ToString(System::DateTime(2024, 6, 15, 12, 0, 0).AddTicks(1234567)),
              "2024-06-15T12:00:00.1234567");
}

// A NUMERIC OFFSET NAMES AN INSTANT, so it is converted rather than stamped. Merely stamping would
// make `+05:00` and `+02:00` produce the same local wall-clock time -- the offset read and thrown
// away again, which is the defect this whole ticket exists to end.
TEST(XmlConvertDateTimeMode1945Tests, ANumericOffsetIsConvertedRatherThanStamped) {
    using System::Xml::XmlConvert;
    using System::DateTimeKind;

    const auto a = XmlConvert::ToDateTime("2024-06-15T12:00:00+05:00");
    const auto b = XmlConvert::ToDateTime("2024-06-15T12:00:00+02:00");
    EXPECT_EQ(a.getKindProperty(), DateTimeKind::Local);
    EXPECT_NE(a, b);
    // Three hours apart, whatever this container's zone is -- asserted as a DIFFERENCE so the row
    // holds in any tzdata, which is #2351's lesson.
    EXPECT_EQ((b - a), System::TimeSpan::FromHours(3));

    // A date's own `-` separator must not be read as an offset sign. The marker is matched as a
    // SHAPE (`+hh:mm` / `-hh:mm`), not by scanning backwards for a sign -- `2024-06-15` ends in
    // `06-15`, which a looser rule accepts.
    EXPECT_EQ(XmlConvert::ToDateTime("2024-06-15").getKindProperty(), DateTimeKind::Unspecified);
    EXPECT_EQ(XmlConvert::ToDateTime("2024-06-15"), System::DateTime(2024, 6, 15));
}

// The `default:` arm is reachable ONLY by casting a value in from outside the enumeration, which
// is exactly why it needs its own case -- no ordinary call can reach it.
TEST(XmlConvertDateTimeMode1945Tests, AnUndefinedModeRaisesDotNetsOwnMessage) {
    using System::Xml::XmlConvert;
    using System::Xml::XmlDateTimeSerializationMode;

    const auto bogus = static_cast<XmlDateTimeSerializationMode>(99);
    const System::DateTime value(2024, 6, 15, 12, 0, 0);

    EXPECT_THROW(XmlConvert::ToString(value, bogus), System::ArgumentException);
    EXPECT_THROW(XmlConvert::ToDateTime("2024-06-15T12:00:00", bogus), System::ArgumentException);

    try {
        XmlConvert::ToString(value, bogus);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("XmlDateTimeSerializationMode"), std::string::npos) << what;
        EXPECT_NE(what.find("dateTimeOption"), std::string::npos) << what;
        EXPECT_EQ(e.getParamNameProperty(), "dateTimeOption");
    }
}

TEST(XmlConvertDateTimeMode1945Tests, ToDateTimeOffsetHonoursItsFormatToo) {
    using System::Xml::XmlConvert;

    const auto parsed = XmlConvert::ToDateTimeOffset("2024-06-15 13:45:30",
                                                     "yyyy-MM-dd HH:mm:ss");
    EXPECT_EQ(parsed.getDateTimeProperty(), System::DateTime(2024, 6, 15, 13, 45, 30));
    // With no zone token in the format -- and this port's exact grammar has none at all -- .NET's
    // DateTimeStyles.None gives the result the LOCAL offset. Asserted against the zone rather
    // than a literal, for the same reason as above.
    EXPECT_EQ(parsed.getOffsetProperty(),
              System::TimeZone::CurrentTimeZone().GetUtcOffset(parsed.getDateTimeProperty()));

    EXPECT_THROW(XmlConvert::ToDateTimeOffset("2024-06-15", "HH:mm:ss"), System::FormatException);

    // The real DateTimeOffset exact parser must see the zone token. The obsolete composition via
    // DateTime::ParseExact could neither capture +05:30 nor preserve the visible wall clock, and
    // also rejected XmlConvert's documented outer whitespace.
    const auto explicitOffset = XmlConvert::ToDateTimeOffset(
        " \t2024-06-15T13:45:30+05:30\r\n", "yyyy-MM-dd'T'HH:mm:sszzz");
    EXPECT_EQ(explicitOffset.getDateTimeProperty(),
              System::DateTime(2024, 6, 15, 13, 45, 30));
    EXPECT_EQ(explicitOffset.getOffsetProperty(), System::TimeSpan::FromMinutes(330));
}

TEST(XmlConvertDateTimeMode1945Tests, DateTimeOffsetUsesTheXsdRoundTripShape) {
    using System::Xml::XmlConvert;

    const System::DateTimeOffset value(
        System::DateTime(2024, 6, 15, 13, 45, 30).AddTicks(5'000'000),
        System::TimeSpan::FromMinutes(330));
    const std::string written = XmlConvert::ToString(value);
    EXPECT_EQ(written, "2024-06-15T13:45:30.5+05:30");

    const auto roundtrip = XmlConvert::ToDateTimeOffset(written);
    EXPECT_TRUE(roundtrip.EqualsExact(value));

    const System::DateTimeOffset wholeSecond(
        System::DateTime(2024, 6, 15, 13, 45, 30), System::TimeSpan::FromHours(-4));
    EXPECT_EQ(XmlConvert::ToString(wholeSecond), "2024-06-15T13:45:30-04:00");

    const System::DateTimeOffset zulu(
        System::DateTime(2024, 6, 15, 13, 45, 30), System::TimeSpan::Zero);
    const std::string zuluText = XmlConvert::ToString(zulu);
    EXPECT_EQ(zuluText, "2024-06-15T13:45:30Z");
    EXPECT_TRUE(XmlConvert::ToDateTimeOffset(zuluText).EqualsExact(zulu));
}

TEST(XmlConvertDateTimeMode1945Tests, XsdNumericZoneIsBoundedAtExactlyFourteenHours) {
    using System::Xml::XmlConvert;

    EXPECT_NO_THROW((void)XmlConvert::ToDateTime("2024-06-15T12:00:00+14:00"));
    EXPECT_NO_THROW((void)XmlConvert::ToDateTime("2024-06-15T12:00:00-14:00"));
    EXPECT_THROW((void)XmlConvert::ToDateTime("2024-06-15T12:00:00+14:01"),
                 System::FormatException);
    EXPECT_THROW((void)XmlConvert::ToDateTime("2024-06-15T12:00:00-14:01"),
                 System::FormatException);
    EXPECT_THROW((void)XmlConvert::ToDateTime("2024-06-15T12:00:00+14:59"),
                 System::FormatException);

    // XML outer whitespace is collapsed before the marker is classified. It must neither erase
    // a valid kind/offset nor hide an invalid offset from the XSD-specific bound.
    const auto zulu = XmlConvert::ToDateTime(
        " \t2024-06-15T12:00:00Z \r\n",
        System::Xml::XmlDateTimeSerializationMode::RoundtripKind);
    EXPECT_EQ(zulu.getKindProperty(), System::DateTimeKind::Utc);
    EXPECT_EQ(zulu.getTicksProperty(), System::DateTime(2024, 6, 15, 12, 0, 0).getTicksProperty());

    EXPECT_EQ(XmlConvert::ToDateTimeOffset(" 2024-06-15T12:00:00Z \t")
                  .getOffsetProperty(),
              System::TimeSpan::Zero);
    EXPECT_EQ(XmlConvert::ToDateTimeOffset(" 2024-06-15T12:00:00+05:30 \r\n")
                  .getOffsetProperty(),
              System::TimeSpan::FromMinutes(330));
    EXPECT_THROW((void)XmlConvert::ToDateTime(" 2024-06-15T12:00:00+14:01 "),
                 System::FormatException);
    EXPECT_THROW((void)XmlConvert::ToDateTimeOffset(" 2024-06-15T12:00:00+14:01 "),
                 System::FormatException);
}

TEST(XmlConvertDateTimeMode1945Tests, XsdRejectsNonCanonicalTimezoneSuffixesAtBothDoors) {
    using System::Xml::XmlConvert;

    for (const char* suffix : {"+8", "+2:5", "+800", "+0800", "z"}) {
        const std::string value = std::string("2024-06-15T10:20:30") + suffix;
        EXPECT_THROW((void)XmlConvert::ToDateTime(value), System::FormatException) << suffix;
        EXPECT_THROW((void)XmlConvert::ToDateTimeOffset(value), System::FormatException) << suffix;
    }
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST(XmlConvertDateTimeMode1945Tests, NumericOffsetRangeEdgesUseXsdCompatibilityClamping) {
    using System::Xml::XmlConvert;
    ScopedXmlTimeZone tz("UTC-02"); // fixed process-local UTC+02:00

    const auto rescued = XmlConvert::ToDateTime("0001-01-01T00:00:00+01:00");
    EXPECT_EQ(rescued.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(rescued.getTicksProperty(), System::TimeSpan::TicksPerHour);

    const auto belowMin = XmlConvert::ToDateTime("0001-01-01T00:00:00+05:00");
    EXPECT_EQ(belowMin.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(belowMin.getTicksProperty(), System::DateTime::MinValue.getTicksProperty());

    const auto aboveMax = XmlConvert::ToDateTime("9999-12-31T23:59:59-05:00");
    EXPECT_EQ(aboveMax.getKindProperty(), System::DateTimeKind::Local);
    EXPECT_EQ(aboveMax.getTicksProperty(), System::DateTime::MaxValue.getTicksProperty());
}

TEST(XmlConvertDateTimeMode1945Tests, OffsetlessDateTimeOffsetUsesTheParsedDatesDstOffset) {
    using System::Xml::XmlConvert;
    ScopedXmlTimeZone tz("EST5EDT,M3.2.0/2,M11.1.0/2");

    const auto winter = XmlConvert::ToDateTimeOffset("2025-01-15T12:00:00");
    const auto summer = XmlConvert::ToDateTimeOffset("2025-07-15T12:00:00");
    EXPECT_EQ(winter.getOffsetProperty(), System::TimeSpan::FromHours(-5));
    EXPECT_EQ(summer.getOffsetProperty(), System::TimeSpan::FromHours(-4));

    EXPECT_EQ(XmlConvert::ToDateTimeOffset("2025-07-15T12:00:00Z").getOffsetProperty(),
              System::TimeSpan::Zero);
    EXPECT_EQ(XmlConvert::ToDateTimeOffset("2025-07-15T12:00:00+05:30")
                  .getOffsetProperty(),
              System::TimeSpan::FromMinutes(330));
}
#endif
