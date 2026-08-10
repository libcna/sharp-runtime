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
// `src/System/Net/Http/Headers/HttpDateParser.hpp`.
//
// WHAT THIS TICKET DELIBERATELY DOES NOT DO: narrow the grammar. The `sscanf` conversion string
// is kept verbatim with `%n` appended, so every value that parsed before parses to the same
// instant. Rewriting it as a fixed-width scanner would also reject text `sscanf` accepts, and
// with `/rv` absent there is no evidence here for what .NET does with that text.
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
    ASSERT_TRUE(parsed.getDateProperty().has_value());
    const System::DateTimeOffset& d = *parsed.getDateProperty();
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

TEST(HttpDateConsumptionTests, THEPINTheTwoObsoleteFormatsWereNeverAcceptedAndStillAreNot) {
    // #2125's acceptance criteria require the existing RFC 850 / asctime behaviour to be PINNED
    // so the full-consumption repair cannot have narrowed a required form away. Measured: the
    // conversion string demands a comma immediately after a three-letter day name, which BOTH
    // obsolete forms fail, so neither was ever accepted and #2125 removed nothing.
    //
    // RFC 9110 §5.6.7 requires a RECIPIENT to accept all three forms, so this is a real gap --
    // but closing it is a WIDENING and belongs to #2130, which is deferred because /rv is absent
    // and .NET's own behaviour cannot be established here.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sunday, 06-Nov-94 08:49:37 GMT", parsed))
        << "RFC 850 form: not accepted before #2125 either -- see #2130";
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sun Nov  6 08:49:37 1994", parsed))
        << "ANSI C asctime form: not accepted before #2125 either -- see #2130";
    // And the preferred form is accepted, so the pin cannot pass vacuously.
    EXPECT_TRUE(RetryConditionHeaderValue::TryParse(kValid, parsed));
}

TEST(HttpDateConsumptionTests, AnEmbeddedNULCannotTruncateTheValueIntoAValidDate) {
    // sscanf works on a C string, so without this guard "…GMT\0trailing" would consume a clean
    // prefix and report a complete match over text the caller never bounded.
    RetryConditionHeaderValue parsed{System::TimeSpan::Zero};
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse(std::string(kValid) + std::string("\0junk", 5),
                                                     parsed));
}

TEST(HttpDateConsumptionTests, THECONTROLNonDateGarbageStillFailsEverywhere) {
    RetryConditionHeaderValue retry{System::TimeSpan::Zero};
    RangeConditionHeaderValue range{std::string("\"x\"")};
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse(kGarbage, retry));
    EXPECT_FALSE(RangeConditionHeaderValue::TryParse("Sun, 06 Nov 1994 08:49:37 UTC", range))
        << "a non-GMT zone token was rejected before and still is";
    EXPECT_FALSE(RetryConditionHeaderValue::TryParse("Sun, 06 Xyz 1994 08:49:37 GMT", retry))
        << "an unknown month was rejected before and still is";
}
