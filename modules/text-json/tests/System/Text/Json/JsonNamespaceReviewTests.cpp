// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the compatible tickets of the `modules/text-json` namespace review
// (docs/SystemTextJsonNamespaceReviewPlan.md, owning ticket #2110).
//
//   #2111 / cause TJ-A — a std:: exception ESCAPED four public parse doors on an eight-character
//          number literal. JsonDocument::Parse caught only nlohmann's parse_error, and a number
//          overflowing a double raises out_of_range, which is not one. A caller writing
//          catch (const System::Exception&) never saw it and the process terminated.
//   #2112 / cause TJ-G — an embedded NUL made the vendored parser stop early, so a document was
//          SILENTLY TRUNCATED and everything after the NUL was discarded with no diagnostic.
//
// Neither carries an SR-AUD identifier: no audit finding names either, and audit numbering stays
// frozen at 364. Both exception choices are recorded in the plan as THIS PORT'S choice — the
// reference tree is absent.
#include <gtest/gtest.h>

#include <string>

#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/JsonSerializer.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Utf8JsonWriter.hpp"

using namespace System::Text::Json;
using System::Text::Json::Nodes::JsonNode;

namespace {

/// The load-bearing assertion for #2111. It is NOT enough that *some* exception is thrown — it
/// must not be a std:: one. Catching System::Exception first and std::exception second is what
/// distinguishes them, and a bare EXPECT_THROW(..., JsonException) would not: before #2111 the
/// escaping type was nlohmann::detail::out_of_range, which is a std::exception and NOT a
/// System::Exception, so the process died at the call site.
template <typename F>
void ExpectNoStdExceptionEscapes(F&& call, const char* what) {
    bool systemException = false, stdException = false;
    try {
        call();
    } catch (const System::Exception&) {
        systemException = true;
    } catch (const std::exception&) {
        stdException = true;
    }
    EXPECT_TRUE(systemException) << what << ": expected a System exception";
    EXPECT_FALSE(stdException) << what << ": a std:: exception must not cross this API";
}

} // namespace

// ===========================================================================
// #2111 — no std:: exception may escape any public parse door
// ===========================================================================

TEST(JsonParseExceptionIdentityTests, ANumberOverflowingADoubleThrowsAJsonExceptionAtEveryDoor) {
    const std::string overflow = "{\"a\":1e999999}";
    EXPECT_THROW((void)JsonDocument::Parse(overflow), JsonException);
    EXPECT_THROW((void)JsonDocument::ParseValue(overflow), JsonException);
    EXPECT_THROW((void)JsonNode::Parse(overflow), JsonException);
    EXPECT_THROW((void)JsonSerializer::Deserialize(overflow), JsonException);
}

TEST(JsonParseExceptionIdentityTests, NoStdExceptionEscapesAnyPublicParseDoor) {
    for (const std::string& input : {std::string("{\"a\":1e999999}"),
                                     std::string("{\"a\":-1e999999}"),
                                     std::string("{\"a\":1e400}"),
                                     std::string("[1e999999]")}) {
        ExpectNoStdExceptionEscapes([&] { (void)JsonDocument::Parse(input); },
                                    ("JsonDocument::Parse " + input).c_str());
        ExpectNoStdExceptionEscapes([&] { (void)JsonNode::Parse(input); },
                                    ("JsonNode::Parse " + input).c_str());
        ExpectNoStdExceptionEscapes([&] { (void)JsonSerializer::Deserialize(input); },
                                    ("JsonSerializer::Deserialize " + input).c_str());
    }
}

TEST(JsonParseExceptionIdentityTests, TheWriterSRawValueDoorIsGuardedToo) {
    // A FOURTH door the review found by reading rather than by probing: WriteRawValue validated
    // through the same parser with the same parse_error-only catch.
    Utf8JsonWriter writer;
    ExpectNoStdExceptionEscapes([&] { writer.WriteRawValue("1e999999"); },
                                "Utf8JsonWriter::WriteRawValue");
}

TEST(JsonParseExceptionIdentityTests, TheNativeParserReasonSurvivesIntoTheMessage) {
    try {
        (void)JsonDocument::Parse("{\"a\":1e999999}");
        FAIL() << "expected a throw";
    } catch (const JsonException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("number overflow"), std::string::npos)
            << "the vendored parser's own reason must not be discarded: " << message;
    }
}

TEST(JsonParseExceptionIdentityTests, NumbersWITHINDoubleRangeAreCompletelyUnaffected) {
    // The other half of the contract: #2111 changes an exception's type and nothing else.
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":1e308}"));
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":-1e308}"));
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":1.7976931348623157e308}"));
    auto doc = JsonDocument::Parse("{\"a\":1e308}");
    EXPECT_EQ(doc->getRootElementProperty().GetProperty("a").GetDouble(), 1e308);
    // A genuine syntax error still reports as one, so the widened catch did not swallow anything.
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":}"), JsonException);
    EXPECT_THROW((void)JsonDocument::Parse(""), JsonException);
}

// ===========================================================================
// #2112 — an embedded NUL must not silently truncate a document
// ===========================================================================

TEST(JsonEmbeddedNulTests, ADocumentWithAnEmbeddedNulIsRejectedRatherThanTruncated) {
    // Before #2112 this was ACCEPTED and parsed as {"a":1} — the second object silently gone.
    const std::string truncating("{\"a\":1}\0{\"b\":2}", 15);
    EXPECT_THROW((void)JsonDocument::Parse(truncating), JsonException);
    EXPECT_THROW((void)JsonNode::Parse(truncating), JsonException);

    const std::string arrays("[1,2]\0[9]", 9);
    EXPECT_THROW((void)JsonDocument::Parse(arrays), JsonException);
}

TEST(JsonEmbeddedNulTests, THECONTROLTheSameDocumentWithASpaceWasAlreadyRejected) {
    // This is what makes #2112 a defect rather than general leniency: the identical document
    // with a SPACE instead of the NUL was ALWAYS rejected as trailing junk. The NUL was being
    // treated as end-of-input. If this control ever starts passing, the finding's premise is
    // gone and #2112's guard is measuring nothing.
    const std::string spaced("{\"a\":1} {\"b\":2}", 15);
    EXPECT_THROW((void)JsonDocument::Parse(spaced), JsonException);
}

TEST(JsonEmbeddedNulTests, TheRejectionNamesThePositionAndTheEscapeThatWouldWork) {
    const std::string truncating("{\"a\":1}\0{\"b\":2}", 15);
    try {
        (void)JsonDocument::Parse(truncating);
        FAIL() << "expected a throw";
    } catch (const JsonException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("NUL"), std::string::npos) << message;
        EXPECT_NE(message.find("position 7"), std::string::npos) << message;
        EXPECT_NE(message.find("\\u0000"), std::string::npos)
            << "the message must say what WOULD work: " << message;
    }
}

TEST(JsonEmbeddedNulTests, EveryNulPositionIsRejected) {
    EXPECT_THROW((void)JsonDocument::Parse(std::string("\0", 1)), JsonException);
    EXPECT_THROW((void)JsonDocument::Parse(std::string("{\"a\":1}\0", 8)), JsonException);
    EXPECT_THROW((void)JsonDocument::Parse(std::string("\0{\"a\":1}", 8)), JsonException);
    // A NUL inside a token was ALREADY a syntax error; it must stay rejected, and it does not
    // matter which of the two guards rejects it.
    EXPECT_THROW((void)JsonDocument::Parse(std::string("{\"a\":12\0 34}", 12)), JsonException);
}

TEST(JsonEmbeddedNulTests, TheWriterSRawValueDoorRejectsItToo) {
    // Sharper here than at the parse doors: validation stopped at the NUL and PASSED, while the
    // FULL text — everything after the NUL included — was appended to the buffer. The written
    // document would have contained text that was never validated.
    Utf8JsonWriter writer;
    EXPECT_THROW(writer.WriteRawValue(std::string("{\"a\":1}\0{\"b\":2}", 15)), JsonException);
}

TEST(JsonEmbeddedNulTests, AnESCAPEDNulIsStillAcceptedAndStillProducesTheCharacter) {
    // The deliberate non-narrowing: \u0000 inside a string value is legal JSON and stays legal.
    // #2112 rejects the raw byte in the DOCUMENT TEXT, never the escaped character in a VALUE.
    auto doc = JsonDocument::Parse("{\"a\":\"x\\u0000y\"}");
    const std::string value = doc->getRootElementProperty().GetProperty("a").GetString();
    EXPECT_EQ(value.size(), 3u);
    EXPECT_EQ(value[1], '\0');
}

TEST(JsonEmbeddedNulTests, ADocumentWithNoNulIsCompletelyUnaffected) {
    auto doc = JsonDocument::Parse("{\"a\":1,\"b\":[2,3],\"c\":\"text\"}");
    JsonElement root = doc->getRootElementProperty();
    EXPECT_EQ(root.GetProperty("a").GetInt32(), 1);
    EXPECT_EQ(root.GetProperty("c").GetString(), "text");
    EXPECT_NO_THROW((void)JsonNode::Parse("{\"a\":1}"));
    Utf8JsonWriter writer;
    EXPECT_NO_THROW(writer.WriteRawValue("{\"a\":1}"));
}

// ===========================================================================
// PINS — behaviour the review MEASURED and deliberately did NOT change (plan §7.4, §6.2, §6.3)
// ===========================================================================

TEST(JsonReviewPinTests, JsonDocumentParseHasNoStackOverflowExposureAt100000Levels) {
    // This refutes the batch's leading structural suspicion for a JSON parser. It rejects at the
    // configured depth and returns cleanly rather than overflowing the stack.
    std::string deep;
    deep.reserve(200004);
    for (int i = 0; i < 100000; ++i) deep += '[';
    for (int i = 0; i < 100000; ++i) deep += ']';
    EXPECT_THROW((void)JsonDocument::Parse(deep), JsonException);
}

TEST(JsonReviewPinTests, TwoOfTheFourDocumentOptionsDOWork) {
    // Plan §6.2: SR-AUD-326's summary says "parsing flags are exposed but not applied". Two of
    // the four ARE applied, and #2115 is scoped to the other two. Pinned so that scoping cannot
    // silently drift.
    JsonDocumentOptions skip;
    skip.CommentHandling = JsonCommentHandling::Skip;
    EXPECT_NO_THROW((void)JsonDocument::Parse("[1] // c", skip));
    EXPECT_THROW((void)JsonDocument::Parse("[1] // c"), JsonException);

    JsonDocumentOptions shallow;
    shallow.MaxDepth = 3;
    EXPECT_THROW((void)JsonDocument::Parse("[[[[[1]]]]]", shallow), JsonException);
    EXPECT_NO_THROW((void)JsonDocument::Parse("[[1]]", shallow));

    JsonDocumentOptions negative;
    negative.MaxDepth = -1;
    EXPECT_THROW((void)JsonDocument::Parse("[]", negative), System::ArgumentOutOfRangeException);
}

TEST(JsonReviewPinTests, TheTwoINERTDocumentOptionsAreStillInertAndThatIsTicketed) {
    // PIN of a known defect, NOT an endorsement: #2115 is the open design ticket. Recorded as a
    // test so that whichever way #2115 is decided, the change is deliberate and visible.
    JsonDocumentOptions trailing;
    trailing.AllowTrailingCommas = true;
    EXPECT_THROW((void)JsonDocument::Parse("[1,]", trailing), JsonException)
        << "AllowTrailingCommas is inert -- #2115";

    JsonDocumentOptions noDuplicates;
    noDuplicates.AllowDuplicateProperties = false;
    auto doc = JsonDocument::Parse("{\"x\":1,\"x\":2}", noDuplicates);
    EXPECT_EQ(doc->getRootElementProperty().GetProperty("x").GetInt32(), 2)
        << "AllowDuplicateProperties is inert and the last value wins -- #2115";
}

TEST(JsonReviewPinTests, JsonElementSIntegerAccessorsAreALREADYCorrect) {
    // Plan §6.3: SR-AUD-328's premise is largely REFUTED. JsonElement already rejects every
    // boundary tested; only the Nodes half survives, and that is #2114. Pinned so the
    // already-correct neighbour cannot regress while #2114 is open.
    auto doc = JsonDocument::Parse(
        "{\"f\":1.5,\"big\":1e100,\"huge\":99999999999999999999,\"i\":7}");
    JsonElement root = doc->getRootElementProperty();
    EXPECT_THROW((void)root.GetProperty("f").GetInt32(), System::Exception);
    EXPECT_THROW((void)root.GetProperty("big").GetInt32(), System::Exception);
    EXPECT_THROW((void)root.GetProperty("big").GetInt64(), System::Exception);
    EXPECT_THROW((void)root.GetProperty("huge").GetInt64(), System::Exception);
    EXPECT_EQ(root.GetProperty("i").GetInt32(), 7);
}

TEST(JsonReviewPinTests, DisposalGuardsThatALREADYWorkAndTheOneThatDoesNot) {
    // Plan §6.1: SR-AUD-324 is NOT a use-after-free. JsonElement holds an OWNING aliasing
    // shared_ptr, so a captured element reads LIVE storage. Two halves already work; the third
    // is the blocked #2117.
    auto doc = JsonDocument::Parse("{\"a\":10}");
    JsonElement captured = doc->getRootElementProperty().GetProperty("a");
    doc->Dispose();
    EXPECT_THROW((void)doc->getRootElementProperty(), System::ObjectDisposedException);
    EXPECT_NO_THROW(doc->Dispose());
    // PIN of the known defect: the captured element still answers. #2117, blocked on layout.
    EXPECT_EQ(captured.GetInt32(), 10)
        << "a captured element still reads live storage -- #2117, and it is NOT a dangling read";
}

TEST(JsonReviewPinTests, MalformedTextThatTheParserALREADYRejects) {
    // Plan §7.4's measured positives, pinned so a future parser change cannot quietly relax them.
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":\"\xC3\x28\"}"), JsonException);   // invalid UTF-8
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":\"\\ud800\"}"), JsonException);    // lone high surrogate
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":\"\\udc00\"}"), JsonException);    // lone low surrogate
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":\"\\x\"}"), JsonException);        // invalid escape
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":\"\\ud83d\\ude00\"}"));         // a valid pair
    // Property lookup is case-sensitive, and a missing key is a System exception.
    auto doc = JsonDocument::Parse("{\"Name\":1}");
    EXPECT_THROW((void)doc->getRootElementProperty().GetProperty("name"), System::Exception);
}
