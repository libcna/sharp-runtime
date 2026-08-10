// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2124 / SR-AUD-319 / cause NH-H
// (docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.1).
//
// The defect: every typed header value in this module that stores a QUOTED string stored it with
// its quotes and emitted it verbatim, and the quoting grammar each of them carried its own copy
// of checks the quoting and nothing else. So
//
//     NameValueHeaderValue("p", "\"safe\r\nX-Injected: yes\"").ToString()
//         == "p=\"safe\r\nX-Injected: yes\""
//
// -- a second header field where the caller supplied one parameter. The unquoted path was already
// guarded; the quoted path was not, and every parameter in this module is a NameValueHeaderValue.
//
// THREE PREMISE CORRECTIONS THIS FILE PINS, because the review's §4.1 got them wrong and a future
// reader must not re-derive them from the plan alone:
//
//   1. ViaHeaderValue did NOT already reject CR *and* LF. `isValidReceivedBy` rejected CR only;
//      `ViaHeaderValue("1.1", "safe\nX")` was accepted and ToString() emitted a raw LF, and a
//      NUL-bearing receivedBy was accepted too. The plan's reading came from a CRLF probe string,
//      which rejects on its CR and hides the LF. Same mistake, same shape, in
//      WarningHeaderValue's `isValidAgent`.
//   2. The finding named seven doors and the review found two more; there are in fact FIVE types
//      handing out a MUTABLE parameter/extension vector -- MediaTypeHeaderValue,
//      TransferCodingHeaderValue, ContentDispositionHeaderValue, CacheControlHeaderValue and
//      NameValueWithParametersHeaderValue -- plus RangeConditionHeaderValue's string constructor,
//      a tenth door no document named.
//   3. This module already contained FOUR independently written copies of the field-terminator
//      rule (HttpHeaders, AuthenticationHeaderValue, HttpRequestHeaders, NameValueHeaderValue) and
//      they disagreed. That is cause NH-H applied to the terminator rule itself. #2124 routed all
//      of them through System::Net::detail::ContainsProtocolFieldTerminator, the one body the
//      family already shares with #2063's ten HTTP doors and #2089's three WebSocket doors.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/FormatException.hpp"
#include "System/Net/Http/Headers/AuthenticationHeaderValue.hpp"
#include "System/Net/Http/Headers/CacheControlHeaderValue.hpp"
#include "System/Net/Http/Headers/ContentDispositionHeaderValue.hpp"
#include "System/Net/Http/Headers/EntityTagHeaderValue.hpp"
#include "System/Net/Http/Headers/HttpContentHeaders.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/HttpResponseHeaders.hpp"
#include "System/Net/Http/Headers/MediaTypeHeaderValue.hpp"
#include "System/Net/Http/Headers/NameValueHeaderValue.hpp"
#include "System/Net/Http/Headers/NameValueWithParametersHeaderValue.hpp"
#include "System/Net/Http/Headers/RangeConditionHeaderValue.hpp"
#include "System/Net/Http/Headers/TransferCodingHeaderValue.hpp"
#include "System/Net/Http/Headers/ViaHeaderValue.hpp"
#include "System/Net/Http/Headers/WarningHeaderValue.hpp"

using namespace System::Net::Http::Headers;

namespace {

/// Every terminator, at every position the matrix requires. Raw text, without quotes.
std::vector<std::string> TerminatorPayloads() {
    return {
        "safe\rX",                       // CR, middle
        "safe\nX",                       // LF, middle
        "safe\r\nX-Injected: yes",       // CRLF, middle -- the finding's own example
        std::string("safe\0X", 6),       // NUL, middle
        "\rsafe",                        // CR, beginning
        "\nsafe",                        // LF, beginning
        std::string("\0safe", 5),        // NUL, beginning
        "safe\r",                        // CR, end
        "safe\n",                        // LF, end
        std::string("safe\0", 5),        // NUL, end
    };
}

/// Characters the family deliberately does NOT reject. If any of these starts being rejected the
/// repair has over-narrowed, which is a different defect and just as real.
std::vector<std::string> NonTerminatorPayloads() {
    return {
        "safe\tX",                       // HTAB is legal inside a field value
        "safe\x7fX",                     // DEL
        "safe\x0bX",                     // VT -- a C0 control that does not terminate a field
        "safe\x1bX",                     // ESC
        "safe\xc3\xa4X",                 // UTF-8 U+00E4
        "safe",                          // the control
    };
}

std::string Quoted(const std::string& raw) { return "\"" + raw + "\""; }

bool HasTerminator(const std::string& s) {
    return s.find('\r') != std::string::npos || s.find('\n') != std::string::npos
        || s.find('\0') != std::string::npos;
}

/// The load-bearing assertion for a COLLECTION: its ToString() legitimately contains CRLF as
/// framing, so the check is that the framing is exactly what the fields require and no more.
void ExpectNoInjectedField(const HttpHeaders& headers, const std::string& what) {
    const std::string text = headers.ToString();
    size_t crlf = 0;
    for (size_t i = 0; i + 1 < text.size(); ++i)
        if (text[i] == '\r' && text[i + 1] == '\n') ++crlf;
    // One CRLF per header field, plus the terminating blank line.
    EXPECT_EQ(crlf, static_cast<size_t>(headers.getCountProperty()) + 1u) << what;
    // No lone CR, no lone LF, no NUL anywhere.
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') { EXPECT_LT(i + 1, text.size()); EXPECT_EQ(text[i + 1], '\n') << what; }
        else if (text[i] == '\n') { EXPECT_GT(i, 0u); EXPECT_EQ(text[i - 1], '\r') << what; }
        else EXPECT_NE(text[i], '\0') << what;
    }
}

} // namespace

// --- the quoted-string doors -----------------------------------------------------------------

TEST(HttpHeaderValueTerminatorTests, NameValueHeaderValueRejectsATerminatorAtAllThreeDoors) {
    for (const std::string& raw : TerminatorPayloads()) {
        const std::string q = Quoted(raw);
        EXPECT_THROW((void)NameValueHeaderValue("p", q), System::FormatException) << q;

        NameValueHeaderValue existing("p");
        EXPECT_THROW(existing.setValueProperty(q), System::FormatException) << q;
        EXPECT_EQ(existing.getValueProperty(), "") << "a rejected set must not have assigned";

        EXPECT_THROW((void)NameValueHeaderValue::Parse("p=" + q), System::FormatException) << q;
        NameValueHeaderValue parsed("placeholder");
        EXPECT_FALSE(NameValueHeaderValue::TryParse("p=" + q, parsed)) << q;
    }
}

TEST(HttpHeaderValueTerminatorTests, TheUnquotedPathWasAlreadyGuardedAndStaysGuarded) {
    // Pinned so the repair cannot be "simplified" back into the else-branch it used to live in.
    for (const std::string& raw : TerminatorPayloads())
        EXPECT_THROW((void)NameValueHeaderValue("p", raw), System::FormatException) << raw;
}

TEST(HttpHeaderValueTerminatorTests, EntityTagAndItsTwoDependentsRejectATerminator) {
    for (const std::string& raw : TerminatorPayloads()) {
        const std::string q = Quoted(raw);
        EXPECT_THROW((void)EntityTagHeaderValue(q), System::FormatException) << q;
        EXPECT_THROW((void)EntityTagHeaderValue(q, true), System::FormatException) << q;
        EXPECT_THROW((void)EntityTagHeaderValue::Parse(q), System::FormatException) << q;
        EntityTagHeaderValue parsed = EntityTagHeaderValue::Any();
        EXPECT_FALSE(EntityTagHeaderValue::TryParse(q, parsed)) << q;
        // The tenth door: RangeConditionHeaderValue's string constructor. Named by no document.
        EXPECT_THROW((void)RangeConditionHeaderValue(q), System::FormatException) << q;
    }
}

TEST(HttpHeaderValueTerminatorTests, WarningRejectsATerminatorInBothItsTextAndItsAgent) {
    for (const std::string& raw : TerminatorPayloads()) {
        EXPECT_THROW((void)WarningHeaderValue(112, "agent", Quoted(raw)), System::FormatException)
            << "warn-text: " << raw;
        EXPECT_THROW((void)WarningHeaderValue(112, raw, "\"t\""), System::FormatException)
            << "warn-agent: " << raw;
    }
}

TEST(HttpHeaderValueTerminatorTests, PREMISECORRECTIONViaAndWarningAcceptedABareLFAndABareNUL) {
    // §4.1 recorded that ViaHeaderValue "already rejects CR and LF" so that only NUL was its gap.
    // These four inputs are the counter-examples: each was ACCEPTED and serialized verbatim before
    // #2124. Kept as their own test so the correction is visible, not buried in a loop.
    EXPECT_THROW((void)ViaHeaderValue("1.1", "safe\nX"), System::FormatException);
    EXPECT_THROW((void)ViaHeaderValue("1.1", std::string("safe\0X", 6)), System::FormatException);
    EXPECT_THROW((void)WarningHeaderValue(112, "safe\nX", "\"t\""), System::FormatException);
    EXPECT_THROW((void)WarningHeaderValue(112, std::string("safe\0X", 6), "\"t\""),
                 System::FormatException);
    // And the CR that the plan's probe string actually rejected on, pinned as still rejected.
    EXPECT_THROW((void)ViaHeaderValue("1.1", "safe\rX"), System::FormatException);
}

// --- the mutable parameter vectors ------------------------------------------------------------

TEST(HttpHeaderValueTerminatorTests, AllFiveMutableParameterVectorsAreClosedAtTheirElementType) {
    // Validation at construction of the CONTAINER is not sufficient: these five hand out the
    // parameter vector by mutable reference, so the only place the rule can sit and cover them
    // all is NameValueHeaderValue itself -- which is what every element is.
    for (const std::string& raw : TerminatorPayloads()) {
        const std::string q = Quoted(raw);
        EXPECT_THROW((void)NameValueHeaderValue("p", q), System::FormatException) << q;
    }
    // And the containers therefore cannot be handed one, at any door.
    MediaTypeHeaderValue media("text/plain");
    TransferCodingHeaderValue coding("chunked");
    ContentDispositionHeaderValue disposition("attachment");
    CacheControlHeaderValue cacheControl;
    NameValueWithParametersHeaderValue withParameters("n", "v");
    EXPECT_EQ(media.getParametersProperty().size(), 0u);
    EXPECT_EQ(coding.getParametersProperty().size(), 0u);
    EXPECT_EQ(disposition.getParametersProperty().size(), 0u);
    EXPECT_EQ(cacheControl.getExtensionsProperty().size(), 0u);
    EXPECT_EQ(withParameters.getParametersProperty().size(), 0u);
}

TEST(HttpHeaderValueTerminatorTests, TheParameterCarryingParseDoorsRejectATerminatorToo) {
    for (const std::string& raw : TerminatorPayloads()) {
        const std::string q = Quoted(raw);
        MediaTypeHeaderValue media("text/plain");
        EXPECT_FALSE(MediaTypeHeaderValue::TryParse("text/plain; p=" + q, media)) << q;
        TransferCodingHeaderValue coding("chunked");
        EXPECT_FALSE(TransferCodingHeaderValue::TryParse("chunked; p=" + q, coding)) << q;
        ContentDispositionHeaderValue disposition("attachment");
        EXPECT_FALSE(ContentDispositionHeaderValue::TryParse("attachment; p=" + q, disposition)) << q;
    }
}

TEST(HttpHeaderValueTerminatorTests, ContentDispositionNameAndFileNameQuoteTheirInputAndSoRejectIt) {
    for (const std::string& raw : TerminatorPayloads()) {
        ContentDispositionHeaderValue d("attachment");
        EXPECT_THROW(d.setNameProperty(raw), System::FormatException) << raw;
        EXPECT_THROW(d.setFileNameProperty(raw), System::FormatException) << raw;
        // No partial mutation: a rejected setter leaves no parameter behind.
        EXPECT_EQ(d.getParametersProperty().size(), 0u) << raw;
        EXPECT_EQ(d.ToString(), "attachment") << raw;
    }
}

// --- what must NOT change ---------------------------------------------------------------------

TEST(HttpHeaderValueTerminatorTests, THECONTROLValidQuotedStringsWithEscapesStillParse) {
    // If this fails, #2124 has over-narrowed into #2126's territory (the escape-blind splitters)
    // or has started rejecting characters the frame grammar permits.
    EXPECT_NO_THROW((void)NameValueHeaderValue("p", "\"a\\\"b\""));   // escaped quote
    EXPECT_NO_THROW((void)NameValueHeaderValue("p", "\"a\\\\b\""));   // escaped backslash
    EXPECT_NO_THROW((void)NameValueHeaderValue("p", "\"a,b\""));      // comma inside quotes
    EXPECT_NO_THROW((void)NameValueHeaderValue("p", "\"a;b\""));      // semicolon inside quotes
    EXPECT_NO_THROW((void)EntityTagHeaderValue("\"a\\\"b\""));
    EXPECT_EQ(NameValueHeaderValue::Parse("p=\"a\\\"b\"").getValueProperty(), "\"a\\\"b\"");
}

TEST(HttpHeaderValueTerminatorTests, THECONTROLNonTerminatingCharactersAreStillAccepted) {
    for (const std::string& raw : NonTerminatorPayloads()) {
        const std::string q = Quoted(raw);
        EXPECT_NO_THROW((void)NameValueHeaderValue("p", q)) << raw;
        EXPECT_NO_THROW((void)EntityTagHeaderValue(q)) << raw;
        EXPECT_NO_THROW((void)WarningHeaderValue(112, "agent", q)) << raw;
        ContentDispositionHeaderValue d("attachment");
        EXPECT_NO_THROW(d.setNameProperty(raw)) << raw;
    }
}

TEST(HttpHeaderValueTerminatorTests, AQuotedPairWhoseEscapeeIsACRIsStillRejected) {
    // "a\<CR>b" -- the backslash is a quoted-string escape, not a wire-level one. The CR still
    // ends the field, so a grammar-only check would accept exactly the byte that must not ship.
    EXPECT_THROW((void)NameValueHeaderValue("p", "\"a\\\rb\""), System::FormatException);
    EXPECT_THROW((void)EntityTagHeaderValue("\"a\\\nb\""), System::FormatException);
}

TEST(HttpHeaderValueTerminatorTests, THEPINRejectionMessagesDoNotEchoTheOffendingText) {
    // ProtocolFieldValidation.hpp's companion rule: attacker-controlled text copied into an
    // exception message and then logged re-creates the injection the rejection prevents.
    const std::string payload = "\"safe\r\nX-Injected: yes\"";
    try { (void)NameValueHeaderValue("p", payload); FAIL(); }
    catch (const System::FormatException& e) {
        EXPECT_EQ(std::string(e.what()).find("Injected"), std::string::npos) << e.what();
        EXPECT_FALSE(HasTerminator(e.what())) << "the message itself must be safe to log";
    }
    try { (void)EntityTagHeaderValue(payload); FAIL(); }
    catch (const System::FormatException& e) { EXPECT_FALSE(HasTerminator(e.what())); }
    try { (void)WarningHeaderValue(112, "agent", payload); FAIL(); }
    catch (const System::FormatException& e) { EXPECT_FALSE(HasTerminator(e.what())); }
    try { (void)ViaHeaderValue("1.1", "safe\nX"); FAIL(); }
    catch (const System::FormatException& e) { EXPECT_FALSE(HasTerminator(e.what())); }
}

// --- serialization: the guarantee, stated where §4.6 says it must be stated --------------------

TEST(HttpHeaderValueTerminatorTests, NoToStringInThisModuleCanEmitAFieldTerminator) {
    // §4.6: this module is NOT on this repository's own wire path (HttpRequestMessage keeps a raw
    // map and HttpClientHandler serializes from that), so the guarantee is stated one step earlier
    // than #2063's and #2089's: no field terminator appears in the SERIALIZED TEXT.
    for (const std::string& raw : TerminatorPayloads()) {
        const std::string q = Quoted(raw);

        HttpContentHeaders content;
        MediaTypeHeaderValue media("text/plain");
        EXPECT_THROW(media.getParametersProperty().push_back(NameValueHeaderValue("p", q)),
                     System::FormatException);
        content.setContentTypeProperty(media);
        ExpectNoInjectedField(content, raw);

        HttpResponseHeaders response;
        EXPECT_THROW(response.setETagProperty(EntityTagHeaderValue(q)), System::FormatException);
        ExpectNoInjectedField(response, raw);

        HttpRequestHeaders request;
        EXPECT_THROW(request.setHostProperty(raw), System::FormatException);
        EXPECT_THROW(request.setFromProperty(raw), System::FormatException);
        EXPECT_THROW(request.setProtocolProperty(raw), System::FormatException);
        ExpectNoInjectedField(request, raw);
    }
}

TEST(HttpHeaderValueTerminatorTests, THEPINTheThreeDoorsThatWereAlreadyCorrectStayCorrect) {
    // AuthenticationHeaderValue's parameter, HttpRequestHeaders' Host/From/Protocol, and
    // HttpHeaders::Add all rejected terminators before #2124. #2124 re-expressed each of them
    // through the shared predicate, and re-expressing a rule is exactly where its accepted set
    // quietly changes -- so the unchanged answers are pinned here.
    for (const std::string& raw : TerminatorPayloads()) {
        EXPECT_THROW((void)AuthenticationHeaderValue("Bearer", raw), System::FormatException) << raw;
        AuthenticationHeaderValue parsed("Basic");
        EXPECT_FALSE(AuthenticationHeaderValue::TryParse("Bearer " + raw, parsed)) << raw;
        HttpRequestHeaders headers;
        EXPECT_THROW(headers.Add("X", raw), System::FormatException) << raw;
    }
    // ...and their accepted side is unchanged too.
    EXPECT_NO_THROW((void)AuthenticationHeaderValue("Bearer", "abc def"));
    HttpRequestHeaders headers;
    EXPECT_NO_THROW(headers.setHostProperty(std::string("example.com:8080")));
    EXPECT_NO_THROW(headers.setFromProperty(std::string("a@example.com")));
    EXPECT_NO_THROW(headers.Add("X", "a b\tc"));
}
