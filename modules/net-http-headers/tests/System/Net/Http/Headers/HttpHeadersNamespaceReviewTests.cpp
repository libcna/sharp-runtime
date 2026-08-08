// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the compatible tickets of the `modules/net-http-headers` namespace
// review (docs/SystemNetHttpHeadersNamespaceReviewPlan.md, owning ticket #2122).
//
//   #2123 / SR-AUD-322 / cause NH-I — TryAddWithoutValidation rejected only an EMPTY name, so a
//          CR/LF-bearing name returned true and ToString() emitted two header fields where the
//          caller supplied one. "Without validation" governs the VALUE, never the NAME.
//
// The review's decisive correction: HttpHeaders::Add was ALREADY correct for all four character
// classes, in both name and value, and for padded names and obs-fold. The repair was to route
// the broken door through the predicate the sibling already used — the same shape as #2103,
// #2101, #2111 and #2114 — while deliberately preserving the unvalidated VALUE behaviour that
// separates the two doors.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/FormatException.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"

using namespace System::Net::Http::Headers;

namespace {

/// Names that must be rejected: every class the probe measured as accepted, plus the RFC 9110
/// separators, plus the empty name that was already rejected.
std::vector<std::string> InvalidNames() {
    std::vector<std::string> names = {
        "",                              // already rejected before #2123
        "X-Bad\r\nInjected: yes",        // the finding's own example
        "X\rY", "X\nY", std::string("X\0Y", 3),
        "X Y", "X\tY",
    };
    for (char sep : {'(', ')', '<', '>', '@', ',', ';', ':', '\\', '"', '/', '[', ']', '?', '=', '{', '}'})
        names.push_back(std::string("X") + sep + "Y");
    return names;
}

} // namespace

TEST(HttpHeadersNameValidationTests, TryAddWithoutValidationRejectsEveryInvalidName) {
    for (const std::string& name : InvalidNames()) {
        HttpRequestHeaders headers;
        EXPECT_FALSE(headers.TryAddWithoutValidation(name, "v"))
            << "an invalid header name must not report success";
        EXPECT_EQ(headers.ToString(), "\r\n")
            << "and the collection must be left completely unchanged";
    }
}

TEST(HttpHeadersNameValidationTests, THEFINDINGSOwnExampleNoLongerSerializesTwoFields) {
    HttpRequestHeaders headers;
    EXPECT_FALSE(headers.TryAddWithoutValidation("X-Bad\r\nInjected: yes", "v"));
    const std::string serialized = headers.ToString();
    EXPECT_EQ(serialized.find("Injected"), std::string::npos) << serialized;
    // The load-bearing assertion: no byte that terminates a header field survives into the text.
    EXPECT_EQ(serialized, "\r\n") << "nothing was added at all";
}

TEST(HttpHeadersNameValidationTests, THECONTRACTTheVALUEIsStillDeliberatelyUnvalidated) {
    // This is what separates TryAddWithoutValidation from Add, and #2123 must not change it.
    // If this test ever starts failing, the ticket has silently repaired a second contract.
    HttpRequestHeaders headers;
    EXPECT_TRUE(headers.TryAddWithoutValidation("X-Fine", "a\r\nY: b"))
        << "the VALUE is bypassed by design -- that IS the door's purpose";
    EXPECT_TRUE(headers.TryAddWithoutValidation("X-Also-Fine", std::string("a\0b", 3)));
    // Whereas Add rejects exactly those values.
    HttpRequestHeaders validating;
    EXPECT_THROW(validating.Add("X-Fine", "a\r\nY: b"), System::FormatException);
    EXPECT_THROW(validating.Add("X-Fine", std::string("a\0b", 3)), System::FormatException);
}

TEST(HttpHeadersNameValidationTests, ValidNamesAreStillAcceptedByBothDoors) {
    const char* valid[] = {"X", "Content-Type", "X-Custom_Header", "a1", "!#$%&'*+-.^_`|~"};
    for (const char* name : valid) {
        HttpRequestHeaders headers;
        EXPECT_TRUE(headers.TryAddWithoutValidation(name, "v")) << name;
        EXPECT_TRUE(headers.Contains(name)) << name;
        HttpRequestHeaders validating;
        EXPECT_NO_THROW(validating.Add(name, "v")) << name;
    }
}

TEST(HttpHeadersNameValidationTests, TheVectorOverloadCannotLeaveTheCollectionPartiallyPopulated) {
    HttpRequestHeaders headers;
    EXPECT_FALSE(headers.TryAddWithoutValidation("X\r\nY", std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(headers.ToString(), "\r\n") << "the name is checked once, before anything is stored";
    EXPECT_TRUE(headers.TryAddWithoutValidation("X-Ok", std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(headers.GetValues("X-Ok").size(), 2u);
}

TEST(HttpHeadersNameValidationTests, THEPINAddWasALREADYCorrectAndStaysThatWay) {
    // The repair target. Measured before #2123 and pinned here so it cannot regress into the
    // permissive state that would make the two doors identical again.
    HttpRequestHeaders headers;
    EXPECT_THROW(headers.Add("X-Bad\r\nInjected: yes", "v"), System::FormatException);
    EXPECT_THROW(headers.Add("X Y", "v"), System::FormatException);
    EXPECT_THROW(headers.Add(" X ", " v "), System::FormatException) << "padded names rejected";
    EXPECT_THROW(headers.Add("X", "a\r\n b"), System::FormatException) << "obs-fold rejected";
    EXPECT_EQ(headers.ToString(), "\r\n") << "a rejected Add stores nothing";
}

TEST(HttpHeadersNameValidationTests, THEPINHeaderLookupIsCaseINSENSITIVE) {
    // Plan §6.2. Recorded because `net-http`'s SR-AUD-315 case-SENSITIVITY finding is about that
    // module's raw map -- a different type in a different component. The two must never be merged.
    HttpRequestHeaders headers;
    headers.Add("Content-Type", "text/plain");
    EXPECT_TRUE(headers.Contains("content-type"));
    EXPECT_TRUE(headers.Contains("CONTENT-TYPE"));
}
