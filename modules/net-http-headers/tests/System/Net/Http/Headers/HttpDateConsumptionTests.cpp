// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2125 / SR-AUD-321 / cause NH-H
// (docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.3).
//
// The defect: an `sscanf`-based HTTP-date parser that never checked how much of the value it had
// consumed, so `"Sun, 06 Nov 1994 08:49:37 GMT trailing"` parsed and the trailing text was
// silently discarded — at every door that reads a date.
//
// PREMISE CORRECTION: §4.3 counted SIX copies. There are SEVEN — it did not name
// `WarningHeaderValue`'s date field, which extracts the date from inside a quoted string and then
// hands it to its own copy of the same parser. All seven are now one body,
// `modules/net/include/System/Net/detail/HttpDateParser.hpp`.
//
// HISTORICAL #2125 BOUNDARY: that ticket did not narrow the grammar; it appended `%n` to the
// then-existing `sscanf` conversion. Later #2130/#2360 measured and widened the grammar, and
// #2418 replaced every remaining unbounded numeric conversion with the explicit bounded cursor.
#include <gtest/gtest.h>

#include <string>

#include "System/DateTimeOffset.hpp"
#include "System/TimeSpan.hpp"
#include "System/Net/Http/Headers/ContentDispositionHeaderValue.hpp"
#include "System/Net/Http/Headers/HttpContentHeaders.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/HttpResponseHeaders.hpp"
#include "System/Net/Http/Headers/RangeConditionHeaderValue.hpp"
#include "System/Net/Http/Headers/RetryConditionHeaderValue.hpp"
#include "System/Net/Http/Headers/WarningHeaderValue.hpp"

using namespace System::Net::Http::Headers;

namespace {

const char* const kValid = "Sun, 06 Nov 1994 08:49:37 GMT";
const char* const kTrailing = "Sun, 06 Nov 1994 08:49:37 GMT trailing";
const char* const kGarbage = "garbage";

} // namespace

TEST(HttpDateConsumptionTests, AllSevenDoorsRejectTrailingText) {
    // 1. RetryConditionHeaderValue::Parse — the finding's own example.
    {
        RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
        EXPECT_TRUE(RetryConditionHeaderValue::TryParse(kValid, parsed));
        EXPECT_FALSE(RetryConditionHeaderValue::TryParse(kTrailing, parsed));
        EXPECT_FALSE(RetryConditionHeaderValue::TryParse(kGarbage, parsed)) << "the control";
    }
    // 2. HttpContentHeaders — Expires, and 3. Last-Modified.
    {
        HttpContentHeaders h;
        h.Add("Expires", kValid);
        EXPECT_TRUE(h.getExpiresProperty().has_value());
        h.Remove("Expires");
        h.Add("Expires", kTrailing);
        EXPECT_FALSE(h.getExpiresProperty().has_value());

        h.Add("Last-Modified", kTrailing);
        EXPECT_FALSE(h.getLastModifiedProperty().has_value());
        h.Remove("Last-Modified");
        h.Add("Last-Modified", kValid);
        EXPECT_TRUE(h.getLastModifiedProperty().has_value());
    }
    // 4. HttpResponseHeaders — Date.
    {
        HttpResponseHeaders h;
        h.Add("Date", kTrailing);
        EXPECT_FALSE(h.getDateProperty().has_value());
        h.Remove("Date");
        h.Add("Date", kGarbage);
        EXPECT_FALSE(h.getDateProperty().has_value()) << "the control";
        h.Remove("Date");
        h.Add("Date", kValid);
        EXPECT_TRUE(h.getDateProperty().has_value());
    }
    // 5. HttpRequestHeaders — Date / If-Modified-Since / If-Unmodified-Since.
    {
        HttpRequestHeaders h;
        h.Add("Date", kTrailing);
        EXPECT_FALSE(h.getDateProperty().has_value());
        h.Add("If-Modified-Since", kTrailing);
        EXPECT_FALSE(h.getIfModifiedSinceProperty().has_value());
        h.Add("If-Unmodified-Since", kTrailing);
        EXPECT_FALSE(h.getIfUnmodifiedSinceProperty().has_value());
        HttpRequestHeaders ok;
        ok.Add("Date", kValid);
        EXPECT_TRUE(ok.getDateProperty().has_value());
    }
    // 6. RangeConditionHeaderValue — If-Range.
    {
        RangeConditionHeaderValue parsed{std::string("\"x\"")};
        EXPECT_TRUE(RangeConditionHeaderValue::TryParse(kValid, parsed));
        EXPECT_FALSE(RangeConditionHeaderValue::TryParse(kTrailing, parsed));
    }
    // 7. WarningHeaderValue — the date field. NOT named by the review; the seventh copy.
    {
        WarningHeaderValue parsed{112, "a", "\"t\""};
        const std::string valid = std::string("112 agent \"t\" \"") + kValid + "\"";
        const std::string trailing = std::string("112 agent \"t\" \"") + kTrailing + "\"";
        EXPECT_TRUE(WarningHeaderValue::TryParse(valid, parsed));
        EXPECT_FALSE(WarningHeaderValue::TryParse(trailing, parsed));
    }
}

TEST(HttpDateConsumptionTests, ContentDispositionsThreeDatePropertiesRejectTrailingText) {
    for (const char* param : {"creation-date", "modification-date", "read-date"}) {
        ContentDispositionHeaderValue bad("attachment");
        ContentDispositionHeaderValue::TryParse(
            std::string("attachment; ") + param + "=\"" + kTrailing + "\"", bad);
        ContentDispositionHeaderValue good("attachment");
        ASSERT_TRUE(ContentDispositionHeaderValue::TryParse(
            std::string("attachment; ") + param + "=\"" + kValid + "\"", good));
        if (std::string(param) == "creation-date") {
            EXPECT_FALSE(bad.getCreationDateProperty().has_value());
            EXPECT_TRUE(good.getCreationDateProperty().has_value());
        } else if (std::string(param) == "modification-date") {
            EXPECT_FALSE(bad.getModificationDateProperty().has_value());
            EXPECT_TRUE(good.getModificationDateProperty().has_value());
        } else {
            EXPECT_FALSE(bad.getReadDateProperty().has_value());
            EXPECT_TRUE(good.getReadDateProperty().has_value());
        }
    }
}

TEST(HttpDateConsumptionTests, AValidDateStillParsesToTheSameInstant) {
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse(kValid, parsed));
    const auto date = parsed.getDateProperty();
    ASSERT_TRUE(date.has_value());
    const System::DateTimeOffset& d = *date;
    EXPECT_EQ(d.getYearProperty(), 1994);
    EXPECT_EQ(d.getMonthProperty(), 11);
    EXPECT_EQ(d.getDayProperty(), 6);
    EXPECT_EQ(d.getHourProperty(), 8);
    EXPECT_EQ(d.getMinuteProperty(), 49);
    EXPECT_EQ(d.getSecondProperty(), 37);
}

TEST(HttpDateConsumptionTests, TrailingWHITESPACEIsStillAcceptedBecauseItIsNotTheDefect) {
    // The finding is about arbitrary trailing TEXT. Whitespace-only trailing was accepted before
    // #2125 and stays accepted; narrowing that would be a second, unasked-for change.
    //
    // This MUST be measured at a door that does not pre-trim, or it cannot discriminate:
    // RetryConditionHeaderValue::TryParse trims its input before the date parser ever sees it, so
    // asserting there passes whether or not the parser tolerates trailing whitespace. The
    // collection doors read the stored raw value untrimmed, so they are the honest witness. (An
    // over-repair mutation that demanded `consumed == size` passed the RetryCondition spelling of
    // this test and failed only this one.)
    HttpResponseHeaders spaces;
    spaces.Add("Date", std::string(kValid) + "   ");
    EXPECT_TRUE(spaces.getDateProperty().has_value());
    HttpResponseHeaders tab;
    tab.Add("Date", std::string(kValid) + "\t");
    EXPECT_TRUE(tab.getDateProperty().has_value());
}

TEST(HttpDateConsumptionTests, Fix2130_AllThreeFormsRFC9110RequiresAreAccepted) {
    // #2125 pinned that neither obsolete form had EVER been accepted, so its full-consumption
    // repair could not have narrowed a required form away -- and recorded that closing the gap
    // "is a WIDENING and belongs to #2130, which is deferred because /rv is absent and .NET's
    // own behaviour cannot be established here".
    //
    // It can now. HttpDateParser.TryParse tries strict "r" and then TWENTY-ONE format strings,
    // of which four are RFC 850 and one is ANSI C's asctime
    // (Common/src/System/Net/HttpDateParser.cs:9-32). RFC 9110 5.6.7 requires a RECIPIENT to
    // accept all three, and all three are now accepted.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};

    ASSERT_TRUE(RetryConditionHeaderValue::TryParse(kValid, parsed)) << "IMF-fixdate";
    const auto preferred = parsed.getDateProperty();
    ASSERT_TRUE(preferred.has_value());

    // All three spellings denote the SAME instant, which is the point of accepting them.
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sunday, 06-Nov-94 08:49:37 GMT", parsed))
        << "RFC 850 form";
    ASSERT_TRUE(parsed.getDateProperty().has_value());
    EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(), preferred->getUtcTicksProperty());

    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sun Nov  6 08:49:37 1994", parsed))
        << "ANSI C asctime form";
    ASSERT_TRUE(parsed.getDateProperty().has_value());
    EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(), preferred->getUtcTicksProperty());
}

TEST(HttpDateConsumptionTests, Fix2130_TheRFC850TwoDigitYearWindowIsDotNets) {
    // The runtime's invariant Gregorian policy has TwoDigitYearMax == 2049. The HTTP parser
    // used to retain an obsolete 2029 window and therefore disagreed with Calendar.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};

    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Saturday, 06-Nov-49 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 2049) << "49 is the last 20xx year";

    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Monday, 06-Nov-50 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 1950) << "50 is the first 19xx year";

    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sunday, 06-Nov-94 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 1994);
}

TEST(HttpDateConsumptionTests, Fix2130_TheObsoleteFormsObeyEveryRuleThePreferredFormDoes) {
    // Widening the grammar must not widen the CONSUMPTION rule #2125 established, nor the NUL
    // guard. Both obsolete forms go through the same OnlyTrailingWhitespace and the same
    // embedded-NUL rejection.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    for (const char* good : {"Sunday, 06-Nov-94 08:49:37 GMT", "Sun Nov  6 08:49:37 1994"}) {
        SCOPED_TRACE(good);
        EXPECT_TRUE(RetryConditionHeaderValue::TryParse(good, parsed));
        EXPECT_FALSE(RetryConditionHeaderValue::TryParse(std::string(good) + " trailing", parsed))
            << "trailing text must still invalidate the value";
        EXPECT_FALSE(
            RetryConditionHeaderValue::TryParse(std::string(good) + std::string("\0junk", 5), parsed))
            << "an embedded NUL must still not truncate the value into a valid date";
    }

    // A calendar-invalid date is still rejected in the obsolete forms too.
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sunday, 31-Feb-94 08:49:37 GMT", parsed));
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sun Feb 31 08:49:37 1994", parsed));
    // ...and so is a month name that is not one.
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sunday, 06-Xxx-94 08:49:37 GMT", parsed));
}

TEST(HttpDateConsumptionTests, Fix2130_AShortYearOnAnIMFDateWasSILENTLYWRONGNotRejected) {
    // A LATENT DEFECT the widening uncovered, and it is a correction rather than a widening.
    // The IMF conversion string read the year with %d, so "Sun, 06 Nov 94 08:49:37 GMT" was
    // ACCEPTED and reported the year **94 AD** -- a silently wrong instant, off by nineteen
    // centuries. .NET accepts the same text and reads 1994
    // ("ddd, d MMM yy H:m:s 'GMT'", HttpDateParser.cs:17), so the port's answer was WRONG
    // rather than merely strict, and no test had noticed.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sun, 06 Nov 94 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 1994)
        << "this used to report 94";

    // The same 2049 window as RFC 850's, because it is the same calendar rule.
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sat, 06 Nov 49 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 2049);

    // ONLY an exactly-two-digit token is expanded, so a four-digit year is untouched and a
    // three-digit one keeps whatever it had -- nothing else moves.
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sat, 06 Nov 0094 08:49:37 GMT", parsed));
    EXPECT_EQ(parsed.getDateProperty()->getYearProperty(), 94)
        << "a four-digit year means exactly what it says";
}

TEST(HttpDateConsumptionTests, Pin2360_TheLenientDotNetVariantsAreAcceptedWithCorrectInstants) {
    // .NET accepts sixteen further formats, and they are LENIENCY rather than required forms: a
    // UTC zone token instead of GMT, no zone token at all, a missing day-of-week, a two-digit
    // year on an IMF-fixdate, and RFC 5322 numeric offsets. Adopting them would accept text
    // RFC 9110 does not define as an HTTP-date -- a much larger widening than #2130 asked for,
    // and each has its own ambiguity (a bare time with no zone is only UTC because .NET ASSUMES
    // it is). That gap is ticket #2360, and this pin is what stops it landing by accident.
    // FLIPPED by #2360 (2026-08-18). Every row below now parses, and each carries the line of
    // HttpDateParser.cs that authorises it. The instant is asserted, not just acceptance, because
    // a lenient parser that accepts the right text and reports the wrong moment is worse than one
    // that rejects it.
    //
    // The door is RangeConditionHeaderValue rather than RetryConditionHeaderValue, and that is a
    // CORRECTION my first cut needed. Retry-After dispatches on the first character -- a digit
    // means delta-seconds and the whole value must then be digits -- so a date with no
    // day-of-week can never reach the date branch there. That is not a defect: .NET does exactly
    // the same, and says so ("We either have a timespan or a date/time value. Determine which one
    // we have by looking at the first char", RetryConditionHeaderValue.cs:94-98). If-Range has no
    // such ambiguity, so it is the door that can see the whole grammar.
    const System::DateTimeOffset expected(1994, 11, 6, 8, 49, 37, System::TimeSpan::Zero);
    RangeConditionHeaderValue parsed{std::string("\"x\"")};
    for (const char* lenient : {
             "Sun, 06 Nov 1994 08:49:37 UTC",       // HttpDateParser.cs:12
             "Sun, 06 Nov 1994 08:49:37",           // :13, no zone -> AssumeUniversal
             "06 Nov 1994 08:49:37 GMT",            // :14, no day-of-week
             "06 Nov 1994 08:49:37 UTC",            // :15
             "06 Nov 1994 08:49:37",                // :16
             "Sun, 06 Nov 94 08:49:37 UTC",         // :18, short year
             "Sun, 06 Nov 94 08:49:37",             // :19
             "06 Nov 94 08:49:37 GMT",              // :20
             "06 Nov 94 08:49:37 UTC",              // :21
             "06 Nov 94 08:49:37",                  // :22
             "Sunday, 06-Nov-94 08:49:37 UTC",      // :23, RFC 850 with UTC
             "Sunday, 06-Nov-94 08:49:37 +00:00",   // :24, RFC 850 with an offset
             "Sunday, 06-Nov-94 08:49:37",          // :25, RFC 850 with no zone
             "Sun, 06 Nov 1994 08:49:37 +00:00",    // :28, RFC 5322
             "06 Nov 1994 08:49:37 +00:00",         // :30, RFC 5322, no day-of-week
         }) {
        SCOPED_TRACE(lenient);
        ASSERT_TRUE(RangeConditionHeaderValue::TryParse(lenient, parsed));
        ASSERT_TRUE(parsed.getDateProperty().has_value());
        EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(), expected.getUtcTicksProperty());
    }

    // Retry-After still sees the forms that do NOT start with a digit, which is the half of the
    // widening that reaches it.
    RetryConditionHeaderValue retry{System::TimeSpan::Zero};
    ASSERT_TRUE(RetryConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 UTC", retry));
    ASSERT_TRUE(retry.getDateProperty().has_value());
    EXPECT_EQ(retry.getDateProperty()->getUtcTicksProperty(), expected.getUtcTicksProperty());
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("06 Nov 1994 08:49:37 GMT", retry))
        << "a leading digit means delta-seconds at THIS door, in .NET too "
           "(RetryConditionHeaderValue.cs:94-98)";

    // A numeric offset is APPLIED, not ignored -- otherwise "accepted" would be meaningless.
    ASSERT_TRUE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 -05:00", parsed));
    ASSERT_TRUE(parsed.getDateProperty().has_value());
    EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(),
              System::DateTimeOffset(1994, 11, 6, 13, 49, 37, System::TimeSpan::Zero).getUtcTicksProperty());
    // ...and .NET's zzz makes the ':' optional (DateTimeParse.cs:3315), so the RFC 5322 wire
    // spelling works too and means the same thing.
    ASSERT_TRUE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 -0500", parsed));
    ASSERT_TRUE(parsed.getDateProperty().has_value());
    EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(),
              System::DateTimeOffset(1994, 11, 6, 13, 49, 37, System::TimeSpan::Zero).getUtcTicksProperty());
}

// #2360. Twenty-one format strings are three shapes crossed with three axes, and the cross has
// TWO CELLS .NET'S LIST DOES NOT CONTAIN: a two-digit year with a numeric offset, with and
// without a day-of-week. Writing the parser as a cross would have added them silently, so they
// are rejected explicitly and pinned here -- "the obvious completion of the pattern" is exactly
// the widening that has no reference behind it.
TEST(HttpDateConsumptionTests, Fix2360_TheTwoCellsMissingFromDotNetsListAreRejected) {
    RangeConditionHeaderValue parsed{std::string("\"x\"")};
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 94 08:49:37 +00:00", parsed))
        << "no such format between HttpDateParser.cs:9 and :32";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("06 Nov 94 08:49:37 +00:00", parsed))
        << "nor this one";
    // The neighbouring cells that DO exist still parse, which is what makes the two above a
    // deliberate exclusion rather than a broken short year or a broken offset.
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 94 08:49:37 GMT", parsed));
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 +00:00", parsed));
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sunday, 06-Nov-94 08:49:37 +00:00", parsed));
}

// The shape axes are not interchangeable, and each rejection below is a line .NET does not have.
TEST(HttpDateConsumptionTests, Fix2360_TheShapesDoNotCrossContaminate) {
    RangeConditionHeaderValue parsed{std::string("\"x\"")};
    // RFC 850 requires a weekday at all, and requires a FULL name.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("06-Nov-94 08:49:37 GMT", parsed))
        << "no weekday on the hyphenated shape";
    // The abbreviated-name rejection must be probed through a zone the STRICT arm declines,
    // because that arm accepts an abbreviated name (#2376) and would otherwise answer first.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06-Nov-94 08:49:37 UTC", parsed))
        << "abbreviated weekday on the hyphenated shape";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06-Nov-94 08:49:37", parsed));
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sunday, 06-Nov-94 08:49:37 UTC", parsed))
        << "the control: the full name is what makes it an RFC 850 date";
    // ...and a four-digit year on it, which .NET's four RFC 850 formats all spell yy.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sunday, 06-Nov-1994 08:49:37 GMT", parsed));
    // The space-separated shape takes the ABBREVIATED name, never the full one.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sunday, 06 Nov 1994 08:49:37 GMT", parsed));
    // A year is exactly two or exactly four digits -- .NET's yy and yyyy are both exact widths.
    // Probed through a zone the strict IMF-fixdate arm declines, for the same reason as above.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 199 08:49:37 UTC", parsed))
        << "three-digit year";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("06 Nov 199 08:49:37 GMT", parsed))
        << "three-digit year, no day-of-week";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 9 08:49:37 UTC", parsed))
        << "one-digit year";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 19945 08:49:37 UTC", parsed))
        << "five-digit year";

    // A named zone .NET does not list, an out-of-range offset minute, and trailing text.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 EST", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 +00:60", parsed))
        << "DateTimeParse.cs:3334 rejects a minute field at 60";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 GMT junk", parsed));
    // Inner whitespace IS allowed -- DateTimeStyles.AllowInnerWhite.
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sun,  06   Nov  1994   08:49:37   GMT", parsed));
}

// FLIPPED by #2376 (2026-08-18). #2360's own tests surfaced these: the three strict arms are
// sscanf conversions, and sscanf's %d and %[A-Za-z] did not bound a field's width, so each arm was
// WIDER than the format string it transcribes.
//
// THE TICKET NAMED TWO SITES AND THERE ARE THREE. It listed the abbreviated weekday on the
// hyphenated RFC 850 shape and the three-digit year on IMF-fixdate; asctime carries the same
// unbounded year and the same unvalidated weekday (HttpDateParser.cs:26), and repairing two
// thirds of one rule is not repairing it.
TEST(HttpDateConsumptionTests, Fix2376_TheStrictArmsMatchTheirFormatStrings) {
    RangeConditionHeaderValue parsed{std::string("\"x\"")};

    // .NET's `ddd` is MatchAbbreviatedDayName -- seven names, not any three letters -- and its
    // `dddd` is MatchDayName, full names only.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Xyz, 06 Nov 1994 08:49:37 GMT", parsed))
        << "an invented abbreviation on the IMF-fixdate shape";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06-Nov-94 08:49:37 GMT", parsed))
        << "an ABBREVIATED weekday on the hyphenated shape, which .NET spells dddd";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Xyz Nov  6 08:49:37 1994", parsed))
        << "an invented abbreviation on the asctime shape";

    // `yyyy` and `yy` are ParseDigits with an EXACT width, so nothing between or beyond them
    // parses. "Sun, 06 Nov 199 ..." used to be accepted and read as the year 199 AD.
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 199 08:49:37 GMT", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 9 08:49:37 GMT", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 19945 08:49:37 GMT", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun Nov  6 08:49:37 199", parsed))
        << "the asctime year is yyyy too";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun Nov  6 08:49:37 19945", parsed));

    // THE CONTROLS. Every form RFC 9110 5.6.7 requires still parses to the same instant, so this
    // is a narrowing to the format strings and not a narrowing past them.
    const System::DateTimeOffset expected(1994, 11, 6, 8, 49, 37, System::TimeSpan::Zero);
    for (const char* good : {"Sun, 06 Nov 1994 08:49:37 GMT",     // IMF-fixdate
                             "Sun, 06 Nov 94 08:49:37 GMT",       // the two-digit year (#2130)
                             "Sunday, 06-Nov-94 08:49:37 GMT",    // RFC 850
                             "Sun Nov  6 08:49:37 1994"}) {       // asctime
        SCOPED_TRACE(good);
        ASSERT_TRUE(RangeConditionHeaderValue::TryParse(good, parsed));
        ASSERT_TRUE(parsed.getDateProperty().has_value());
        EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(), expected.getUtcTicksProperty());
    }
    // Every name is recognized, but it must agree with the date. 6..12 November 1994 are
    // Sunday..Saturday, so this also proves the check is not accidentally "Sun only".
    const char* abbreviated[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* full[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                          "Saturday"};
    for (int i = 0; i < 7; ++i) {
        const std::string day = std::to_string(6 + i);
        EXPECT_TRUE(RangeConditionHeaderValue::TryParse(
            std::string(abbreviated[i]) + ", " + day + " Nov 1994 08:49:37 GMT", parsed));
        EXPECT_TRUE(RangeConditionHeaderValue::TryParse(
            std::string(full[i]) + ", " + day + "-Nov-94 08:49:37 GMT", parsed));
    }

    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "Mon, 06 Nov 1994 08:49:37 GMT", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "Monday, 06-Nov-94 08:49:37 GMT", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "Mon Nov  6 08:49:37 1994", parsed));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "Mon, 06 Nov 1994 08:49:37 UTC", parsed))
        << "the lenient arm must not reaccept a strict value with a false weekday";
}

TEST(HttpDateConsumptionTests, InvariantNamesAreAsciiCaseInsensitiveButGmtLiteralIsNot) {
    RangeConditionHeaderValue parsed{std::string("\"x\"")};
    const System::DateTimeOffset expected(1994, 11, 6, 8, 49, 37, System::TimeSpan::Zero);
    for (const char* value : {"sUn, 06 nOv 1994 08:49:37 GMT",
                              "sUnDaY, 06-nOv-94 08:49:37 GMT",
                              "sUn nOv  6 08:49:37 1994"}) {
        ASSERT_TRUE(RangeConditionHeaderValue::TryParse(value, parsed)) << value;
        ASSERT_TRUE(parsed.getDateProperty().has_value());
        EXPECT_EQ(parsed.getDateProperty()->getUtcTicksProperty(), expected.getUtcTicksProperty());
    }
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "Sun, 06 Nov 1994 08:49:37 gmt", parsed));
}

TEST(HttpDateConsumptionTests, NumericFieldsRejectSignsAndOversizedLexemes) {
    RangeConditionHeaderValue parsed{std::string("\"x\"")};
    for (const char* value : {"Sun, +06 Nov 1994 08:49:37 GMT",
                              "Sun, 06 Nov +1994 08:49:37 GMT",
                              "Sun, 06 Nov 1994 +08:49:37 GMT",
                              "Sunday, 06-Nov-99999999999999999999 08:49:37 GMT",
                              "Sun Nov  6 08:49:37 99999999999999999999"}) {
        EXPECT_FALSE(RangeConditionHeaderValue::TryParse(value, parsed)) << value;
    }

    EXPECT_FALSE(RangeConditionHeaderValue::TryParse(
        "06-Nov-2094 08:49:37 GMT", parsed))
        << "the no-weekday hyphenated form belongs to Cookie Expires, not HTTP headers";
}

TEST(HttpDateConsumptionTests, AnEmbeddedNULCannotTruncateTheValueIntoAValidDate) {
    // The original sscanf implementation could truncate here; the current bounded cursor rejects
    // NUL as a non-token too. The regression stays at the public door across both implementations.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse(std::string(kValid) + std::string("\0junk", 5),
                                                     parsed));
}

TEST(HttpDateConsumptionTests, THECONTROLNonDateGarbageStillFailsEverywhere) {
    RetryConditionHeaderValue retry{System::TimeSpan::Zero};
    RangeConditionHeaderValue range{std::string("\"x\"")};
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse(kGarbage, retry));
    EXPECT_TRUE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 UTC", range))
        << "#2360: a UTC zone token is now accepted, at every door, not only RetryCondition";
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 EST", range))
        << "...but only GMT and UTC; a named zone .NET does not list is still rejected";
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sun, 06 Xyz 1994 08:49:37 GMT", retry))
        << "an unknown month was rejected before and still is";
}
