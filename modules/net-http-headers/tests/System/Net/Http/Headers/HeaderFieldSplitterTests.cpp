// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2126 / SR-AUD-320 / cause NH-H
// (docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.2).
//
// The defect: seven list splitters, six of which toggled a bool on every `"` with no notion of a
// quoted-pair, so a legal RFC 9110 parameter whose quoted-string contains an ESCAPED quote
// followed by the delimiter was split in the middle of its own value and then rejected.
//
//     text/plain; p="a\";b"   -> rejected
//     text/plain; p="a;b"     -> accepted   <- the control that makes the premise precise
//
// This is a WIDENING: text valid per RFC 9110 starts being accepted. Nothing that parsed before
// stops parsing.
//
// PREMISE REFINEMENT: the seventh splitter,
// `NameValueWithParametersHeaderValue::TryParse`, was not escape-blind — it tracked no quoting at
// all, splitting on a bare `input.find(';')`, so even the CONTROL case was split there. Its own
// test below is what makes that visible.
#include <gtest/gtest.h>

#include <string>

#include "System/Net/Http/Headers/CacheControlHeaderValue.hpp"
#include "System/Net/Http/Headers/ContentDispositionHeaderValue.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/HttpResponseHeaders.hpp"
#include "System/Net/Http/Headers/MediaTypeHeaderValue.hpp"
#include "System/Net/Http/Headers/NameValueWithParametersHeaderValue.hpp"
#include "System/Net/Http/Headers/TransferCodingHeaderValue.hpp"

using namespace System::Net::Http::Headers;

namespace {

// p="a\";b"  -- an escaped quote, then the ';' delimiter, both inside one quoted-string.
const char* const kEscapedParam = "p=\"a\\\";b\"";
// p="a;b"    -- an UNESCAPED ';' inside a quoted-string. Accepted before #2126; still accepted.
const char* const kControlParam = "p=\"a;b\"";

} // namespace

TEST(HeaderFieldSplitterTests, TheSemicolonSplittersAcceptAnEscapedQuoteBeforeTheDelimiter) {
    MediaTypeHeaderValue media("text/plain");
    ASSERT_TRUE(MediaTypeHeaderValue::TryParse(std::string("text/plain; ") + kEscapedParam, media));
    ASSERT_EQ(media.getParametersProperty().size(), 1u);
    EXPECT_EQ(media.getParametersProperty()[0].getValueProperty(), "\"a\\\";b\"")
        << "the whole quoted-string must survive as ONE parameter value";

    TransferCodingHeaderValue coding("chunked");
    ASSERT_TRUE(TransferCodingHeaderValue::TryParse(std::string("gzip; ") + kEscapedParam, coding));
    EXPECT_EQ(coding.getParametersProperty().size(), 1u);

    ContentDispositionHeaderValue disposition("attachment");
    ASSERT_TRUE(ContentDispositionHeaderValue::TryParse(
        std::string("attachment; filename=\"a\\\";b\""), disposition));
    EXPECT_EQ(disposition.getFileNameProperty(), "a\\\";b");

    NameValueWithParametersHeaderValue withParameters("n", "v");
    ASSERT_TRUE(NameValueWithParametersHeaderValue::TryParse(
        std::string("n=v; ") + kEscapedParam, withParameters));
    EXPECT_EQ(withParameters.getParametersProperty().size(), 1u);
}

TEST(HeaderFieldSplitterTests, TheCommaSplittersAcceptAnEscapedQuoteBeforeTheDelimiter) {
    // CacheControl, HttpRequestHeaders and HttpResponseHeaders all split a list on ','.
    CacheControlHeaderValue cacheControl;
    ASSERT_TRUE(CacheControlHeaderValue::TryParse("ext=\"a\\\",b\", no-cache", cacheControl));
    EXPECT_TRUE(cacheControl.getNoCacheProperty());
    ASSERT_EQ(cacheControl.getExtensionsProperty().size(), 1u);
    EXPECT_EQ(cacheControl.getExtensionsProperty()[0].getValueProperty(), "\"a\\\",b\"");

    HttpRequestHeaders request;
    request.Add("Accept", "text/plain;p=\"a\\\",b\", text/html");
    EXPECT_EQ(request.getAcceptProperty().size(), 2u)
        << "the escaped quote must not turn one media type into three";

    HttpResponseHeaders response;
    response.Add("Accept-Ranges", "bytes");
    EXPECT_EQ(response.getAcceptRangesProperty().size(), 1u);
}

TEST(HeaderFieldSplitterTests, THECONTROLAnUnescapedDelimiterInsideQuotesIsStillAccepted) {
    // §4.2's control: this parsed BEFORE #2126 at the six escape-blind doors, and proves the
    // defect was escape-blindness rather than quote-blindness. If this regresses, #2126 has
    // broken the thing that was already working.
    MediaTypeHeaderValue media("text/plain");
    ASSERT_TRUE(MediaTypeHeaderValue::TryParse(std::string("text/plain; ") + kControlParam, media));
    ASSERT_EQ(media.getParametersProperty().size(), 1u);
    EXPECT_EQ(media.getParametersProperty()[0].getValueProperty(), "\"a;b\"");
}

TEST(HeaderFieldSplitterTests, PREMISEREFINEMENTTheSeventhSplitterTrackedNoQuotingAtAll) {
    // NameValueWithParametersHeaderValue split on a bare find(';'), so even the CONTROL case --
    // an unescaped ';' inside a quoted parameter -- was split there and the value rejected. That
    // makes this door strictly worse than the six the review described, and it is the one
    // assertion that distinguishes "escape-blind" from "quote-blind".
    NameValueWithParametersHeaderValue value("n", "v");
    ASSERT_TRUE(NameValueWithParametersHeaderValue::TryParse(
        std::string("n=v; ") + kControlParam, value));
    ASSERT_EQ(value.getParametersProperty().size(), 1u);
    EXPECT_EQ(value.getParametersProperty()[0].getValueProperty(), "\"a;b\"");
}

TEST(HeaderFieldSplitterTests, THECONTROLATrailingBackslashAndAnUnterminatedQuoteStayRejected) {
    // The splitter deliberately does not validate; the caller's grammar check still does. These
    // were rejected before #2126 and must stay rejected, or the widening has gone too far.
    MediaTypeHeaderValue media("text/plain");
    EXPECT_FALSE(MediaTypeHeaderValue::TryParse("text/plain; p=\"a", media)) << "unterminated quote";
    EXPECT_FALSE(MediaTypeHeaderValue::TryParse("text/plain; p=\"a\\\"", media)) << "trailing backslash";
    EXPECT_FALSE(MediaTypeHeaderValue::TryParse("text/plain; p=\"a\\\"; q=\"b", media));
    NameValueWithParametersHeaderValue value("n", "v");
    EXPECT_FALSE(NameValueWithParametersHeaderValue::TryParse("n=v; p=\"a", value));
    EXPECT_FALSE(NameValueWithParametersHeaderValue::TryParse("n=v; p=\"a\\\"", value));
}

TEST(HeaderFieldSplitterTests, ABackslashOUTSIDEAQuotedStringIsStillAnOrdinaryCharacter) {
    // quoted-pair occurs only inside quoted-string and comment, so a backslash outside quotes must
    // NOT swallow the delimiter.
    //
    // Measuring this needs a door that SKIPS an unparsable element rather than failing the whole
    // value, or the assertion cannot discriminate: an over-repair that honours the escape
    // everywhere still produces one unparsable segment, and a TryParse that returns false either
    // way proves nothing. (An earlier version of this test asserted exactly that and passed under
    // the over-repair mutation.) `Accept` is such a door -- parseList drops what will not parse --
    // so the element COUNT is the observable that separates the two behaviours.
    HttpRequestHeaders request;
    request.Add("Accept", "text/plain\\, text/html");
    EXPECT_EQ(request.getAcceptProperty().size(), 1u)
        << "the backslash escapes nothing outside quotes, so this is TWO elements of which one is "
           "invalid -- not one long invalid element";

    MediaTypeHeaderValue media("text/plain");
    EXPECT_FALSE(MediaTypeHeaderValue::TryParse("text/plain; a\\; b=c", media))
        << "and `a\\` remains an invalid token in its own right";
}

TEST(HeaderFieldSplitterTests, EverythingThatParsedBeforeStillParses) {
    // A widening must add, never remove. A representative sample of ordinary values.
    MediaTypeHeaderValue media("text/plain");
    EXPECT_TRUE(MediaTypeHeaderValue::TryParse("text/plain; charset=utf-8", media));
    EXPECT_EQ(media.getCharSetProperty(), "utf-8");
    EXPECT_TRUE(MediaTypeHeaderValue::TryParse("text/plain; charset=\"utf-8\"; q=0.5", media));
    EXPECT_EQ(media.getParametersProperty().size(), 2u);

    CacheControlHeaderValue cacheControl;
    ASSERT_TRUE(CacheControlHeaderValue::TryParse("no-cache, max-age=60, private", cacheControl));
    EXPECT_TRUE(cacheControl.getNoCacheProperty());
    EXPECT_EQ(cacheControl.getMaxAgeProperty().value_or(-1), 60);

    ContentDispositionHeaderValue disposition("attachment");
    ASSERT_TRUE(ContentDispositionHeaderValue::TryParse(
        "attachment; name=\"n\"; filename=\"f.txt\"; size=12", disposition));
    EXPECT_EQ(disposition.getFileNameProperty(), "f.txt");
    EXPECT_EQ(disposition.getSizeProperty().value_or(-1), 12);
}
