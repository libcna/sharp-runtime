// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2132 — the `modules/net-http-headers` review's gated-behaviour pins
// (docs/SystemNetHttpHeadersNamespaceReviewPlan.md §6.2 and §12).
//
// Nothing here asserts that the behaviour is RIGHT. Each test pins what the review MEASURED, so
// that a future change has to be deliberate:
//
//   * §6.2's measured positives, which exist so they are not re-investigated;
//   * **#2128**'s current behaviour — singleton headers comma-joined, and Transfer-Encoding
//     coexisting with Content-Length. That is a request-smuggling SHAPE and it is `needs_user`:
//     where to enforce singleton semantics (collection, typed accessor, or serialization) changes
//     what a long-standing public API accepts, so it is a decision, not a repair. If one of these
//     tests fails, someone has taken that decision — check that they meant to.
//   * **#2130**'s deferred question is pinned in `HttpDateConsumptionTests` instead, next to the
//     parser it constrains.
#include <gtest/gtest.h>

#include <string>

#include "System/FormatException.hpp"
#include "System/Net/Http/Headers/HttpContentHeaders.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/MediaTypeWithQualityHeaderValue.hpp"
#include "System/Net/Http/Headers/StringWithQualityHeaderValue.hpp"

using namespace System::Net::Http::Headers;

// --- #2128: the singleton / TE+CL shape, PINNED, NOT REPAIRED ---------------------------------

TEST(HeadersGatedBehaviourPins, PIN2128SingletonHeadersAreJoinedWithAComma) {
    // RFC 9110 §8.6 requires a Content-Length field-value to be a single 1*DIGIT. A comma-joined
    // pair is precisely the message a request-smuggling chain relies on two intermediaries
    // disagreeing about.
    HttpContentHeaders content;
    content.Add("Content-Length", "10");
    content.Add("Content-Length", "20");
    EXPECT_EQ(content.ToString(), "Content-Length: 10,20\r\n\r\n");

    // RFC 9110 §7.2 requires exactly one Host.
    HttpRequestHeaders request;
    request.Add("Host", "a.example");
    request.Add("Host", "b.example");
    EXPECT_EQ(request.ToString(), "Host: a.example,b.example\r\n\r\n");

    // Identical repeats are joined too, so "the values agree" is not a special case today.
    HttpContentHeaders same;
    same.Add("Content-Length", "10");
    same.Add("Content-Length", "10");
    EXPECT_EQ(same.ToString(), "Content-Length: 10,10\r\n\r\n");
}

TEST(HeadersGatedBehaviourPins, PIN2128TheTwoTypedAccessorsDISAGREEAboutTheJoinedValue) {
    // A fact #2128's design must weigh and which the review did not record: the two typed
    // accessors over a comma-joined singleton already behave DIFFERENTLY.
    //
    //   Content-Length -> the accessor reports the parameter ABSENT (option (b)'s posture, by
    //                     accident of the numeric parse failing on "10,20")
    //   Host           -> the accessor hands back the joined text "a.example,b.example"
    //
    // So the port is already half-way to enforcing at the typed accessor for one header and not at
    // all for the other. Whichever option #2128 chooses, it has to reconcile these two.
    HttpContentHeaders content;
    content.Add("Content-Length", "10");
    content.Add("Content-Length", "20");
    EXPECT_FALSE(content.getContentLengthProperty().has_value());

    HttpRequestHeaders request;
    request.Add("Host", "a.example");
    request.Add("Host", "b.example");
    EXPECT_EQ(request.getHostProperty().value_or(""), "a.example,b.example");
}

TEST(HeadersGatedBehaviourPins, PIN2128TransferEncodingAndContentLengthCoexist) {
    // RFC 9112 §6.1 requires Content-Length to be ignored, or the message rejected, when
    // Transfer-Encoding is present. Nothing here does either.
    HttpRequestHeaders both;
    both.Add("Transfer-Encoding", "chunked");
    both.Add("Content-Length", "5");
    EXPECT_EQ(both.ToString(), "Transfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n");

    // And across the two collections a real message uses, which is the shape that matters: the
    // request headers carry Transfer-Encoding, the content headers carry Content-Length, and
    // neither collection can see the other.
    HttpRequestHeaders request;
    request.setTransferEncodingChunkedProperty(true);
    HttpContentHeaders content;
    content.setContentLengthProperty(5);
    EXPECT_EQ(request.ToString(), "Transfer-Encoding: chunked\r\n\r\n");
    EXPECT_EQ(content.ToString(), "Content-Length: 5\r\n\r\n");
}

// --- §6.2's measured positives, pinned so they are not re-investigated ------------------------

TEST(HeadersGatedBehaviourPins, PINContentLengthValueParsingIsCorrect) {
    // §6.2: no integer-overflow defect at this door. Recorded because it is the kind of door an
    // audit expects to find one at.
    for (const char* bad : {"not-a-number", "99999999999999999999999", "-1", "", " ", "1 2"}) {
        HttpContentHeaders h;
        h.Add("Content-Length", bad);
        EXPECT_FALSE(h.getContentLengthProperty().has_value()) << "[" << bad << "]";
    }
    HttpContentHeaders ok;
    ok.Add("Content-Length", "12");
    EXPECT_EQ(ok.getContentLengthProperty().value_or(-1), 12);
}

TEST(HeadersGatedBehaviourPins, PINQualityValuesAreValidatedAtBothQualityBearingTypes) {
    for (const char* bad : {"text/plain;q=2.5", "text/plain;q=abc", "text/plain;q=-1"}) {
        MediaTypeWithQualityHeaderValue media("x/x");
        EXPECT_FALSE(MediaTypeWithQualityHeaderValue::TryParse(bad, media)) << bad;
    }
    MediaTypeWithQualityHeaderValue media("x/x");
    EXPECT_TRUE(MediaTypeWithQualityHeaderValue::TryParse("text/plain;q=0.5", media));

    for (const char* bad : {"gzip;q=2.5", "gzip;q=abc", "gzip;q=-1"}) {
        StringWithQualityHeaderValue value("x");
        EXPECT_FALSE(StringWithQualityHeaderValue::TryParse(bad, value)) << bad;
    }
    StringWithQualityHeaderValue value("x");
    EXPECT_TRUE(StringWithQualityHeaderValue::TryParse("gzip;q=0.5", value));
}

TEST(HeadersGatedBehaviourPins, PINNoStdExceptionEscapesAPublicDoor) {
    // §6.2's last positive. `std::` exceptions escaping a System-shaped API is the defect #2111
    // repaired in Text.Json and the one this module was probed for and did not have. Pinned across
    // the degenerate inputs most likely to reach a std::stoi or an index walk off the end.
    static const char* const inputs[] = {
        "", " ", "\"", "\"\"", "\\", "%", "%C", "''", "UTF-8''%", "=", ";", ",", "W/", "*",
        "999999999999999999999999999999", "-999999999999999999999999999999",
        "text/plain;q=999999999999999999999999", "bytes=0-99999999999999999999999",
    };
    for (const char* input : inputs) {
        HttpContentHeaders content;
        HttpRequestHeaders request;
        try {
            content.Add("Content-Length", input);
            (void)content.getContentLengthProperty();
            request.Add("Accept", input);
            (void)request.getAcceptProperty();
            (void)request.getRangeProperty();
        } catch (const System::Exception&) {
            // A System exception is a legitimate answer; a std:: one is not.
        } catch (const std::exception& e) {
            ADD_FAILURE() << "a std:: exception escaped for [" << input << "]: " << e.what();
        }
    }
}

TEST(HeadersGatedBehaviourPins, PINThisModuleIsNOTOnThisRepositoryssOwnWirePath) {
    // Plan §4.6, and the reason CCF-021's guarantee for this module is stated as "no field
    // terminator appears in the SERIALIZED TEXT" rather than "no byte reaches the wire":
    // modules/net-http's HttpRequestMessage keeps a raw std::unordered_map and HttpClientHandler
    // serializes from THAT, so nothing here reaches a socket except through a consumer.
    //
    // There is no runtime assertion that can state a negative about another component without
    // depending on it, so this pin is the ToString() contract itself -- the artefact a consumer
    // would have to serialize -- plus the fact that it is a plain string with no transport behind
    // it. If HttpRequestMessage ever adopts HttpRequestHeaders, §4.6 and CCF-021's boundary
    // sentence both need revisiting, and this comment is where a reader will look.
    HttpRequestHeaders headers;
    headers.Add("X-Test", "value");
    EXPECT_EQ(headers.ToString(), "X-Test: value\r\n\r\n");
}
