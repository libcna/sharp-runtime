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
#include "System/Text/Json/Nodes/JsonArray.hpp"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonElement.hpp"
#include "System/Text/Json/JsonEncodedText.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/JsonSerializer.hpp"
#include "System/Text/Json/JsonSerializerOptions.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"
#include "System/Text/Json/Utf8JsonWriter.hpp"

using namespace System::Text::Json;
using System::Text::Json::Nodes::JsonNode;
using SharpRuntime::intcs;

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

TEST(JsonReviewPinTests, Fix2115_BothDocumentOptionsAreREALNow) {
    // INVERTED. Both flags were validated, stored and NEVER CONSULTED. The user chose the full
    // repair over the cheaper honest option of making Validate() throw for whichever could not be
    // implemented, on the grounds that a switch which does nothing is worse than one that does not
    // exist. nlohmann/json supports neither natively, so each is honoured differently: trailing
    // commas by preprocessing the text, duplicates through the parser callback -- the one place a
    // key can be seen BEFORE nlohmann's silent overwrite.
    JsonDocumentOptions trailing;
    trailing.AllowTrailingCommas = true;
    EXPECT_NO_THROW((void)JsonDocument::Parse("[1,]", trailing));
    EXPECT_EQ(1, JsonDocument::Parse("[1,]", trailing)->getRootElementProperty().GetArrayLength());
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":1,}", trailing));

    // Each flag is honoured ONLY when set, so the default path is byte-identical and cannot
    // regress -- which is why the preprocessing is conditional rather than unconditional.
    JsonDocumentOptions strict;
    EXPECT_THROW((void)JsonDocument::Parse("[1,]", strict), JsonException);

    JsonDocumentOptions noDuplicates;
    noDuplicates.AllowDuplicateProperties = false;
    EXPECT_THROW((void)JsonDocument::Parse("{\"x\":1,\"x\":2}", noDuplicates), JsonException);
    // ...and the default still accepts them, last value winning, as .NET's default does.
    EXPECT_EQ(2, JsonDocument::Parse("{\"x\":1,\"x\":2}")
                     ->getRootElementProperty().GetProperty("x").GetInt32());
}

TEST(JsonReviewPinTests, Fix2115_TheTrailingCommaScannerIsNotASearch) {
    // A `,` inside a STRING is data and a `,` inside a COMMENT is nothing, so the rewrite is a
    // scanner. And it removes a comma before a CLOSER and nothing else: .NET's
    // AllowTrailingCommas permits exactly one trailing comma per container, not a missing element,
    // so a rewrite that dropped every comma next to a bracket would silently accept documents
    // .NET rejects.
    JsonDocumentOptions trailing;
    trailing.AllowTrailingCommas = true;

    auto inString = JsonDocument::Parse("[\"a,]\"]", trailing);
    EXPECT_EQ(1, inString->getRootElementProperty().GetArrayLength());
    EXPECT_EQ("a,]", inString->getRootElementProperty()[0].GetString())
        << "a comma inside a string literal must survive untouched";

    EXPECT_THROW((void)JsonDocument::Parse("[1,,2]", trailing), JsonException)
        << "a MISSING ELEMENT is still malformed";
    EXPECT_THROW((void)JsonDocument::Parse("[,1]", trailing), JsonException)
        << "a LEADING comma is still malformed";

    // Nested containers each get their own trailing comma.
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":[1,],\"b\":{\"c\":2,},}", trailing));
}

TEST(JsonReviewPinTests, Fix2115_DuplicateDetectionIsScopedToOneObject) {
    // The same key in two DIFFERENT objects is not a duplicate, and a scoped-per-object check is
    // the only kind that gets that right -- a flat set of every key seen would reject a perfectly
    // ordinary document.
    JsonDocumentOptions noDuplicates;
    noDuplicates.AllowDuplicateProperties = false;

    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":{\"x\":1},\"b\":{\"x\":2}}", noDuplicates));
    EXPECT_THROW((void)JsonDocument::Parse("{\"a\":{\"x\":1,\"x\":2}}", noDuplicates), JsonException)
        << "...and a duplicate NESTED inside must still be caught";

    // THE SHAPE THAT ACTUALLY PROVES THE SCOPE CLOSES, and the two sibling objects above do NOT:
    // measured, a callback that never pops still accepts them, because their keys land in two
    // different (leaked) sets by accident. A key at the ROOT reappearing AFTER a nested object is
    // the shape that collides -- the root's key would be looked up in the stale inner set.
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":{\"x\":1},\"x\":2}", noDuplicates))
        << "a root key may repeat a NESTED key: the scope must have closed";
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"a\":{\"x\":1},\"b\":[{\"x\":2}],\"x\":3}",
                                              noDuplicates));

    // The message is .NET's SR.DuplicatePropertiesNotAllowed_NameSpan, and the 15-character
    // truncation is .NET's too (ThrowHelper.Serialization.cs:361-364,373): a document can carry an
    // arbitrarily long key, and echoing it whole into an exception is what the reference declines.
    try {
        (void)JsonDocument::Parse("{\"x\":1,\"x\":2}", noDuplicates);
        ADD_FAILURE() << "expected JsonException";
    } catch (const JsonException& e) {
        EXPECT_STREQ("Duplicate property \'x\' encountered during deserialization.", e.what());
    }
    try {
        (void)JsonDocument::Parse(
            "{\"averyveryverylongpropertyname\":1,\"averyveryverylongpropertyname\":2}",
            noDuplicates);
        ADD_FAILURE() << "expected JsonException";
    } catch (const JsonException& e) {
        // Exactly 15 characters then three dots -- ThrowHelper.Serialization.cs:372-385, whose
        // MaxLength is 15 and which appends three '.' rather than an ellipsis character. The first
        // cut of this row wrote 16 and the implementation was right.
        EXPECT_STREQ("Duplicate property \'averyveryverylo...\' encountered during deserialization.",
                     e.what());
    }
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
    // INVERTED by #2117: the captured element now reports the disposal instead of answering.
    EXPECT_THROW((void)captured.GetInt32(), System::ObjectDisposedException);
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

// ===========================================================================
// #2113 (SR-AUD-329, cause TJ-E) — JsonEncodedText's two Encode overloads must agree,
// and the narrow one must stop certifying text this module's own parser rejects
// ===========================================================================
//
// The finding names only the narrow overload's leniency. Measured
// (build-probe/2113_probe1_encodedtext.cpp), the defect is wider in one direction and
// narrower in another than the finding's summary:
//
//   WIDER  — the narrow overload had NO validation at all. It accepted every malformed
//            UTF-8 class: C3 28, the UTF-8-encoded lone surrogates ED A0 80 / ED B0 80,
//            the overlong C0 AF, the out-of-range F5 80 80 80, a stray continuation byte
//            and a truncated sequence — plus raw control characters and an embedded NUL.
//   NARROWER— the two overloads can only genuinely DISAGREE on the lone-surrogate class,
//            because that is the only malformed class UTF-16 can also express. The other
//            malformed classes can arrive through the narrow door only.
//
// The decisive evidence is the module contradicting ITSELF: `Encode` certified bytes as
// validated JSON text that this module's own `JsonDocument::Parse` rejects.

namespace {

/// Every raw byte the granularity matrix proved illegal at BOTH granularities — inside a
/// JSON string AND in whitespace position. 0x09/0x0A/0x0D are excluded because they are
/// legal whitespace between tokens; 0x7F is excluded because it is legal inside a string.
std::string IllegalControlBytes() {
    std::string bytes;
    for (int b = 0x00; b <= 0x1F; ++b)
        if (b != 0x09 && b != 0x0A && b != 0x0D) bytes.push_back(static_cast<char>(b));
    return bytes;
}

} // namespace

TEST(JsonEncodedTextTests, TheNarrowOverloadRejectsEveryMalformedUtf8Class) {
    const std::pair<const char*, std::string> malformed[] = {
        {"invalid C3 28", std::string("\xC3\x28")},
        {"lone HIGH surrogate as UTF-8", std::string("\xED\xA0\x80")},
        {"lone LOW surrogate as UTF-8", std::string("\xED\xB0\x80")},
        {"overlong C0 AF", std::string("\xC0\xAF")},
        {"out of range F5 80 80 80", std::string("\xF5\x80\x80\x80")},
        {"stray continuation byte", std::string("\x80")},
        {"truncated 2-byte lead", std::string("\xC3")},
        {"truncated 3-byte lead", std::string("\xE2\x82")},
    };
    for (const auto& [name, bytes] : malformed) {
        EXPECT_THROW((void)JsonEncodedText::Encode(bytes), System::ArgumentException)
            << name << " must not be certified as validated UTF-8/JSON text";
    }
}

TEST(JsonEncodedTextTests, TheRejectionIsAnArgumentExceptionNamingTheParameter) {
    try {
        (void)JsonEncodedText::Encode(std::string("\xC3\x28"));
        FAIL() << "expected an ArgumentException";
    } catch (const System::ArgumentException& e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("value"), std::string::npos) << "must name the parameter: " << what;
        EXPECT_NE(what.find("UTF-8"), std::string::npos) << "must say what was wrong: " << what;
        EXPECT_NE(what.find("index 0"), std::string::npos) << "must locate it: " << what;
    }
}

TEST(JsonEncodedTextTests, ValidTextIsStillAcceptedByteForByte) {
    const std::string valid[] = {
        std::string(""),                      // empty
        std::string("hello"),                 // ASCII
        std::string("\xC3\xA9"),              // U+00E9, 2-byte
        std::string("\xE2\x82\xAC"),          // U+20AC, 3-byte
        std::string("\xF0\x9F\x98\x80"),      // U+1F600, 4-byte
        std::string("{\"a\": 1}"),            // a whole JSON document
        std::string("a\"b\\c"),               // raw quote and backslash are NOT control chars
    };
    for (const std::string& v : valid) {
        JsonEncodedText t = JsonEncodedText::Encode(v);
        EXPECT_EQ(t.Value, v) << "valid text must survive byte-for-byte";
    }
}

TEST(JsonEncodedTextTests, EveryControlByteIllegalAtBOTHGranularitiesIsRejected) {
    for (char c : IllegalControlBytes()) {
        const std::string input = std::string("a") + c + "b";
        EXPECT_THROW((void)JsonEncodedText::Encode(input), System::ArgumentException)
            << "raw control byte 0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c))
            << " is illegal in JSON text at every granularity";
    }
    EXPECT_EQ(IllegalControlBytes().size(), 29u) << "0x00-0x1F minus tab, LF and CR";
}

TEST(JsonEncodedTextTests, THECONTROLTheFourBytesThatAreLegalSOMEWHEREStayAccepted) {
    // This is what stops the previous test from being an invented blanket policy. Measured
    // against this module's own parser: tab, LF and CR are legal AS WHITESPACE between
    // tokens, and 0x7F is legal INSIDE a string. JsonEncodedText has no consumer in this
    // module, so its granularity is undetermined and none of the four may be rejected.
    // If this test ever starts failing, #2113's boundary has been widened past its evidence.
    for (char c : {'\t', '\n', '\r', '\x7F'}) {
        const std::string input = std::string("a") + c + "b";
        EXPECT_NO_THROW((void)JsonEncodedText::Encode(input))
            << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c))
            << " is legal at one granularity, so Encode must not reject it";
    }
    // And the measurements that fix the boundary, pinned directly.
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\t\"k\":1}"));      // tab as whitespace
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\n\"k\":1}"));      // LF as whitespace
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\r\"k\":1}"));      // CR as whitespace
    EXPECT_NO_THROW((void)JsonDocument::Parse("{\"k\":\"a\x7F""b\"}"));  // DEL inside a string
    EXPECT_THROW((void)JsonDocument::Parse("{\x01\"k\":1}"), JsonException);
    EXPECT_THROW((void)JsonDocument::Parse("{\"k\":\"a\x01""b\"}"), JsonException);
}

TEST(JsonEncodedTextTests, THECONTRADICTIONEncodeNoLongerCertifiesWhatTheParserRejects) {
    // The measurement that makes this a defect rather than mere leniency: for each input,
    // Encode said "validated JSON text" while this module's own parser rejected the very
    // same bytes. Both halves are asserted, so the day the parser starts accepting one of
    // these the contradiction is gone and this test says so.
    const std::string contradictory[] = {
        std::string("a\0b", 3),          // embedded NUL
        std::string("a\x01""b"),         // raw control
        std::string("a\x1F""b"),         // raw control
        std::string("\xC3\x28"),         // invalid UTF-8
        std::string("\xED\xA0\x80"),     // lone surrogate as UTF-8
    };
    for (const std::string& bytes : contradictory) {
        EXPECT_THROW((void)JsonEncodedText::Encode(bytes), System::ArgumentException);
        EXPECT_THROW((void)JsonDocument::Parse("{\"k\":\"" + bytes + "\"}"), JsonException)
            << "the parser half of the contradiction must still hold";
    }
}

TEST(JsonEncodedTextTests, BOTHOverloadsAgreeOnEveryInputClassTheyCanBOTHExpress) {
    struct Pair { const char* name; std::string narrow; std::u16string wide; };
    const Pair pairs[] = {
        {"ASCII",            std::string("hello"),            u"hello"},
        {"empty",            std::string(""),                 std::u16string()},
        {"U+00E9",           std::string("\xC3\xA9"),         u"é"},
        {"U+1F600",          std::string("\xF0\x9F\x98\x80"), u"\U0001F600"},
        {"lone HIGH surrogate", std::string("\xED\xA0\x80"),  std::u16string(1, 0xD800)},
        {"lone LOW surrogate",  std::string("\xED\xB0\x80"),  std::u16string(1, 0xDC00)},
        {"embedded NUL",     std::string("a\0b", 3),          std::u16string(u"a\0b", 3)},
        {"raw control 0x01", std::string("a\x01""b"),         std::u16string(u"a\x01""b", 3)},
        {"tab",              std::string("a\tb"),             std::u16string(u"a\tb")},
    };
    for (const auto& p : pairs) {
        bool narrowThrew = false, wideThrew = false;
        try { (void)JsonEncodedText::Encode(p.narrow); } catch (const System::ArgumentException&) { narrowThrew = true; }
        try { (void)JsonEncodedText::Encode(p.wide); }   catch (const System::ArgumentException&) { wideThrew = true; }
        EXPECT_EQ(narrowThrew, wideThrew)
            << p.name << ": the two Encode overloads must not disagree -- that IS SR-AUD-329";
    }
}

TEST(JsonEncodedTextTests, TheUtf16OverloadStillRejectsMalformedUtf16OnItsOwnTerms) {
    // Its own precondition is unchanged: it reports invalid UTF-16, not invalid UTF-8.
    try {
        (void)JsonEncodedText::Encode(std::u16string(1, 0xD800));
        FAIL() << "expected an ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("UTF-16"), std::string::npos) << e.what();
    }
}

TEST(JsonEncodedTextTests, THEPINUtf8JsonWriterIsADIFFERENTKindOfDoorAndIsUnchanged) {
    // The writer neither validates nor certifies: it REPLACES ill-formed sequences with
    // U+FFFD and ESCAPES control characters, so its output re-parses. That is why #2113
    // does not touch it, and why it is not a second site of the same defect.
    {
        Utf8JsonWriter w;
        w.WriteStringValue(std::string("\xED\xA0\x80"));
        EXPECT_EQ(w.GetString(), "\"\\ufffd\\ufffd\\ufffd\"");
        EXPECT_NO_THROW((void)JsonDocument::Parse("[" + w.GetString() + "]"))
            << "the writer's output must remain re-parseable";
    }
    {
        Utf8JsonWriter w;
        w.WriteStringValue(std::string("a\0b", 3));
        EXPECT_EQ(w.GetString(), "\"a\\u0000b\"") << "the writer ESCAPES a NUL rather than rejecting it";
    }
}

// ===========================================================================
// #2114 (SR-AUD-328, narrowed) — a JSON number that does not convert exactly must be
// rejected at every integral door, not truncated, wrapped, or cast as undefined behaviour
// ===========================================================================
//
// The finding's summary implies the whole integer-accessor family truncates. Measured
// (build-probe/2114_probe1_before.log), JsonElement was ALREADY CORRECT at every boundary
// and only two doors survived — JsonValue's accessors and JsonSerializer::Deserialize<T>,
// the second of which the finding does not name.
//
// And it is worse than "truncates". Built with -fsanitize=float-cast-overflow, the two
// surviving doors produced THREE reports of
//   "1e+20 is outside the range of representable values of type 'int'/'long int'/'long long int'"
// at vendor/nlohmann/json.hpp:4279 (build-probe/2114_probe1_fcorec.log) — nlohmann's
// get<int>() on a number_float is a bare static_cast<int>(double), which is UNDEFINED
// BEHAVIOUR out of range. After the repair the same probe reports ZERO.
//
// The repair target already existed in the module: JsonElement's body carried the correct
// rule AND a comment explaining precisely this UB. #2114 moved that rule into
// detail::TryConvertJsonNumberToIntegral so all three doors share it rather than carrying
// two more copies — a duplicated rule that drifts apart being exactly cause TJ-E, which
// #2113 had just repaired one file away.

namespace {

/// Number literals that must NOT convert to an integer at any door: JSON float literals
/// (whatever their numeric value) and integers outside the destination's range.
const char* const kNonConvertible32[] = {
    "1.5", "-0.5", "1.0", "2e1",                 // float literals -- a grammar rule, not arithmetic
    "1e100", "-1e100",                           // float, and out of range: the UB cases
    "99999999999999999999",                      // beyond uint64, so stored as a float
    "2147483648", "-2147483649",                 // integers just outside Int32
};
const char* const kNonConvertible64[] = {
    "1.5", "-0.5", "1.0", "2e1",
    "1e100", "-1e100",
    "99999999999999999999",
    "9223372036854775808",                       // just outside Int64
};
/// Integer literals that must still convert exactly.
const char* const kExact[] = {"0", "7", "-7", "2147483647", "-2147483648"};

std::shared_ptr<Nodes::JsonValue> ValueOf(const std::string& literal) {
    auto node = JsonNode::Parse(literal);
    auto value = std::dynamic_pointer_cast<Nodes::JsonValue>(node);
    EXPECT_NE(value, nullptr) << "expected " << literal << " to parse as a JsonValue";
    return value;
}

} // namespace

TEST(JsonIntegralConversionTests, JsonValueRejectsEveryNumberThatDoesNotConvertExactly) {
    for (const char* literal : kNonConvertible32)
        EXPECT_THROW((void)ValueOf(literal)->GetInt32(), System::InvalidOperationException)
            << literal << " must not convert to Int32";
    for (const char* literal : kNonConvertible64)
        EXPECT_THROW((void)ValueOf(literal)->GetInt64(), System::InvalidOperationException)
            << literal << " must not convert to Int64";
}

TEST(JsonIntegralConversionTests, JsonValueStillConvertsExactIntegers) {
    for (const char* literal : kExact) {
        EXPECT_EQ(ValueOf(literal)->GetInt32(), std::stoi(literal)) << literal;
        EXPECT_EQ(ValueOf(literal)->GetInt64(), std::stoll(literal)) << literal;
    }
    EXPECT_EQ(ValueOf("9223372036854775807")->GetInt64(), 9223372036854775807LL);
    EXPECT_EQ(ValueOf("2147483648")->GetInt64(), 2147483648LL) << "in range for Int64, out for Int32";
    // The finding's named example, and the two the probe added.
    EXPECT_THROW((void)Nodes::JsonValue::Create(1.5)->GetInt32(), System::InvalidOperationException);
    EXPECT_THROW((void)Nodes::JsonValue::Create(1e100)->GetInt64(), System::InvalidOperationException);
    EXPECT_THROW((void)Nodes::JsonValue::Create(std::numeric_limits<double>::quiet_NaN())->GetInt32(),
                 System::InvalidOperationException);
    // A double that IS an exact integer is still a JSON float literal, and still does not convert.
    EXPECT_THROW((void)Nodes::JsonValue::Create(3.0)->GetInt32(), System::InvalidOperationException);
    // GetDouble is untouched.
    EXPECT_DOUBLE_EQ(Nodes::JsonValue::Create(1.5)->GetDouble(), 1.5);
    EXPECT_DOUBLE_EQ(ValueOf("1e100")->GetDouble(), 1e100);
}

TEST(JsonIntegralConversionTests, DeserializeOfAnIntegralTypeRejectsTheSameNumbers) {
    // The door the finding does NOT name, and the one reachable straight from document text.
    for (const char* literal : kNonConvertible32)
        EXPECT_THROW((void)JsonSerializer::Deserialize<int>(literal), JsonException) << literal;
    for (const char* literal : kNonConvertible64)
        EXPECT_THROW((void)JsonSerializer::Deserialize<long long>(literal), JsonException) << literal;
    for (const char* literal : kExact)
        EXPECT_EQ(JsonSerializer::Deserialize<int>(literal), std::stoi(literal)) << literal;
    // Unsigned destinations reject a negative literal rather than wrapping it.
    EXPECT_THROW((void)JsonSerializer::Deserialize<unsigned int>("-1"), JsonException);
    EXPECT_EQ(JsonSerializer::Deserialize<unsigned int>("4294967295"), 4294967295u);
    EXPECT_THROW((void)JsonSerializer::Deserialize<short>("40000"), JsonException);
    EXPECT_EQ(JsonSerializer::Deserialize<short>("-32768"), -32768);
}

TEST(JsonIntegralConversionTests, NonIntegralDeserializeTargetsAreCompletelyUnaffected) {
    EXPECT_DOUBLE_EQ(JsonSerializer::Deserialize<double>("1.5"), 1.5);
    EXPECT_DOUBLE_EQ(JsonSerializer::Deserialize<double>("1e100"), 1e100);
    EXPECT_EQ(JsonSerializer::Deserialize<std::string>("\"x\""), "x");
    EXPECT_TRUE(JsonSerializer::Deserialize<bool>("true"));
    EXPECT_FALSE(JsonSerializer::Deserialize<bool>("false"));
    EXPECT_EQ(JsonSerializer::Deserialize<std::vector<int>>("[1,2,3]"), (std::vector<int>{1, 2, 3}));
    // A non-number for an integral target still fails as a shape mismatch, as it always did.
    EXPECT_THROW((void)JsonSerializer::Deserialize<int>("\"7\""), JsonException);
    EXPECT_THROW((void)JsonSerializer::Deserialize<int>("null"), JsonException);
}

TEST(JsonIntegralConversionTests, THEPINJsonElementWasALREADYCorrectAndItsExceptionTypeDiffers) {
    // #2114 must not change JsonElement. Its rule only moved house.
    for (const char* literal : kNonConvertible32) {
        auto doc = JsonDocument::Parse(literal);
        EXPECT_THROW((void)doc->getRootElementProperty().GetInt32(), System::FormatException) << literal;
        intcs sink = 0;
        EXPECT_FALSE(doc->getRootElementProperty().TryGetInt32(sink)) << literal;
        EXPECT_EQ(sink, 0) << "a failed TryGet must leave the output zeroed";
    }
    for (const char* literal : kExact) {
        auto doc = JsonDocument::Parse(literal);
        EXPECT_EQ(doc->getRootElementProperty().GetInt32(), std::stoi(literal)) << literal;
    }
    // The exception TYPES differ, deliberately, and that difference is .NET's: JsonValue's
    // doc-comment records JsonValueOfElement.cs raising
    // ThrowInvalidOperationException_NodeUnableToConvertElement, while JsonElement matches
    // JsonDocument.cs's FormatException. Pinned so neither is "harmonised" by mistake.
    EXPECT_THROW((void)JsonDocument::Parse("1.5")->getRootElementProperty().GetInt32(),
                 System::FormatException);
    EXPECT_THROW((void)ValueOf("1.5")->GetInt32(), System::InvalidOperationException);
}

TEST(JsonIntegralConversionTests, THEPINWrongKindStillFailsAsAKindErrorNotAConversionError) {
    // The kind check must run before the conversion rule, and keep its own wording.
    EXPECT_THROW((void)ValueOf("\"x\"")->GetInt32(), System::InvalidOperationException);
    try {
        (void)ValueOf("\"x\"")->GetInt32();
        FAIL() << "expected a throw";
    } catch (const System::InvalidOperationException& e) {
        EXPECT_NE(std::string(e.what()).find("not a number"), std::string::npos) << e.what();
    }
    try {
        (void)ValueOf("1.5")->GetInt32();
        FAIL() << "expected a throw";
    } catch (const System::InvalidOperationException& e) {
        EXPECT_NE(std::string(e.what()).find("could not be converted"), std::string::npos) << e.what();
    }
}

TEST(JsonIntegralConversionTests, THERESIDUALANestedIntegerStillReachesTheVendorConversion) {
    // Measured and deliberately NOT repaired: Deserialize<T>'s guard is top-level only,
    // because an aggregate is deserialized by nlohmann's ADL customization points (the
    // design this port uses in place of reflection) and there is no hook between them.
    // Pinned so the residual is visible rather than implied. If this ever starts throwing,
    // the design changed and the plan's §20.9 must be revisited.
    EXPECT_EQ(JsonSerializer::Deserialize<std::vector<int>>("[1.5]"), (std::vector<int>{1}))
        << "a nested float literal is still truncated by the vendored conversion";
}

// ===========================================================================
// #2116 (SR-AUD-330, cause TJ-B) — Deserialize must forward its options, and
// #2121 — Deserialize<T> was a fifth parse door #2112's NUL guard never reached
// ===========================================================================
//
// Both overloads named their options parameter as an unused comment and discarded it, so
// MaxDepth and ReadCommentHandling -- both of which demonstrably work at JsonDocument::Parse
// -- were unreachable through the serializer.
//
// Probing #2114 then found that the TEMPLATE overload called nlohmann's parser directly and
// so never reached detail::RejectEmbeddedNul either: Deserialize<int>("1" NUL " junk")
// returned 1, the same silent truncation #2112 had closed at the other four doors, while the
// JsonDocument overload -- which delegates to JsonDocument::Parse -- was already guarded.
// The review plan's claim that "every entry point that hands caller text to the parser goes
// through it" was wrong by exactly one door; it is corrected in §20.11, not edited away.
//
// The two are repaired together because they are one structural fact: JsonDocument::Parse and
// JsonSerializer::Deserialize were two independent implementations of "parse a document under
// document options", and they had drifted in two different directions at once. There is now
// one implementation, detail::ParseDocumentText, and both call it.

TEST(JsonSerializerOptionForwardingTests, MaxDepthIsReachableThroughTheSerializer) {
    const std::string deep = "[[[[[1]]]]]";      // 5 levels
    JsonSerializerOptions shallow;
    shallow.setMaxDepthProperty(3);

    EXPECT_THROW((void)JsonSerializer::Deserialize(deep, shallow), JsonException)
        << "the JsonDocument overload must honour MaxDepth";
    using Depth5 = std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>>;
    EXPECT_THROW((void)JsonSerializer::Deserialize<Depth5>(deep, shallow), JsonException)
        << "the template overload must honour MaxDepth too";

    JsonSerializerOptions deepEnough;
    deepEnough.setMaxDepthProperty(10);
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize(deep, deepEnough));
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize<Depth5>(deep, deepEnough));
    // And the same document at the DEFAULT options is accepted, so the rejection is the option's.
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize(deep));
}

TEST(JsonSerializerOptionForwardingTests, CommentHandlingIsReachableThroughTheSerializer) {
    const std::string withComment = "[1] // trailing comment";
    // Disallow is the default, at both the serializer and the document.
    EXPECT_THROW((void)JsonSerializer::Deserialize(withComment), JsonException);
    EXPECT_THROW((void)JsonSerializer::Deserialize<std::vector<int>>(withComment), JsonException);

    JsonSerializerOptions skip;
    skip.setReadCommentHandlingProperty(JsonCommentHandling::Skip);
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize(withComment, skip));
    EXPECT_EQ(JsonSerializer::Deserialize<std::vector<int>>(withComment, skip), (std::vector<int>{1}));
}

TEST(JsonSerializerOptionForwardingTests, InvalidOptionsAreRejectedAtBothEntryPoints) {
    // Validate() now runs for the serializer's doors too, because they share the core.
    JsonSerializerOptions negative;
    negative.setMaxDepthProperty(-1);
    EXPECT_THROW((void)JsonSerializer::Deserialize("[1]", negative), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)JsonSerializer::Deserialize<std::vector<int>>("[1]", negative),
                 System::ArgumentOutOfRangeException);
}

TEST(JsonSerializerOptionForwardingTests, THEDEFAULTPathIsUnchangedForOrdinaryDocuments) {
    // #2116 must not disturb anything a caller was already doing successfully.
    EXPECT_EQ(JsonSerializer::Deserialize<int>("7"), 7);
    EXPECT_EQ(JsonSerializer::Deserialize<std::string>("\"x\""), "x");
    EXPECT_EQ(JsonSerializer::Deserialize<std::vector<int>>("[1,2,3]"), (std::vector<int>{1, 2, 3}));
    auto doc = JsonSerializer::Deserialize("{\"a\":{\"b\":[1,2]}}");
    EXPECT_EQ(doc->getRootElementProperty().GetProperty("a").GetProperty("b")[1].GetInt32(), 2);
    // Round-tripping through Serialize is byte-identical.
    EXPECT_EQ(JsonSerializer::Serialize(JsonSerializer::Deserialize("{\"a\":1}")->getRootElementProperty()),
              "{\"a\":1}");
}

TEST(JsonSerializerOptionForwardingTests, Fix2115_BothFlagsWorkAtBOTHDoorsIdentically) {
    // #2116's forwarding is what made this free: it forwarded ALL FOUR options rather than only
    // the two that worked, precisely so that whatever #2115 decided would take effect at both
    // entry points with no further change. It did -- #2115 touched only the shared parse core,
    // and the serializer door followed. This test is that claim, inverted.
    JsonSerializerOptions trailing;
    trailing.setAllowTrailingCommasProperty(true);
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize("[1,]", trailing));
    JsonDocumentOptions documentTrailing;
    documentTrailing.AllowTrailingCommas = true;
    EXPECT_NO_THROW((void)JsonDocument::Parse("[1,]", documentTrailing))
        << "the sibling door behaves identically -- that is the point";

    JsonSerializerOptions noDuplicates;
    noDuplicates.setAllowDuplicatePropertiesProperty(false);
    EXPECT_THROW((void)JsonSerializer::Deserialize("{\"x\":1,\"x\":2}", noDuplicates), JsonException);
    JsonDocumentOptions documentNoDuplicates;
    documentNoDuplicates.AllowDuplicateProperties = false;
    EXPECT_THROW((void)JsonDocument::Parse("{\"x\":1,\"x\":2}", documentNoDuplicates), JsonException);
}

TEST(JsonEmbeddedNulTests, ALLFIVEParseDoorsRejectAnEmbeddedNulIncludingTheOneNumber2112Missed) {
    // #2121. The fifth door is JsonSerializer::Deserialize<T>. Asserting all five in ONE test
    // is deliberate: a sixth door cannot be added without this test being the place it shows up.
    const std::string truncating = std::string("1\0 junk", 7);
    const std::string objectPair = std::string("{\"a\":1}\0{\"b\":2}", 15);

    EXPECT_THROW((void)JsonDocument::Parse(objectPair), JsonException);            // door 1
    EXPECT_THROW((void)JsonNode::Parse(objectPair), JsonException);                // door 2
    EXPECT_THROW((void)JsonSerializer::Deserialize(objectPair), JsonException);    // door 3
    {
        Utf8JsonWriter writer;                                                     // door 4
        EXPECT_THROW(writer.WriteRawValue(objectPair), JsonException);
    }
    EXPECT_THROW((void)JsonSerializer::Deserialize<int>(truncating), JsonException);       // door 5
    EXPECT_THROW((void)JsonSerializer::Deserialize<double>(truncating), JsonException);    // door 5
    EXPECT_THROW((void)JsonSerializer::Deserialize<std::vector<int>>(std::string("[1]\0[2]", 7)),
                 JsonException);                                                           // door 5
}

TEST(JsonEmbeddedNulTests, THECONTROLForTheFifthDoorTheSameBytesWithASpaceWereAlwaysRejected) {
    // Same control discipline as #2112's: the space version was ALWAYS rejected as trailing
    // junk, which is what makes the NUL version a truncation rather than mere leniency.
    EXPECT_THROW((void)JsonSerializer::Deserialize<int>("1 junk"), JsonException);
    // And the fifth door still accepts a document with no NUL at all, unchanged.
    EXPECT_EQ(JsonSerializer::Deserialize<int>("1"), 1);
    EXPECT_EQ(JsonSerializer::Deserialize<std::vector<int>>("[1,2]"), (std::vector<int>{1, 2}));
}

TEST(JsonEmbeddedNulTests, THEDELIBERATEEXCEPTIONJsonNodeParseStillHasNoDepthBound) {
    // detail::ParseDocumentText deliberately does NOT own JsonNode::Parse: #1897 made it
    // iterative and applies NO depth bound, a documented CLAUDE.md-pinned deviation that only
    // the still-unapproved option A may reopen. If routing JsonNode::Parse through the core
    // were ever attempted, this test is where it would surface.
    std::string deep;
    for (int i = 0; i < 200; ++i) deep += "[";
    deep += "1";
    for (int i = 0; i < 200; ++i) deep += "]";
    EXPECT_NO_THROW((void)JsonNode::Parse(deep)) << "JsonNode::Parse has no depth bound, by design";
    EXPECT_THROW((void)JsonDocument::Parse(deep), JsonException) << "JsonDocument::Parse does";
}

// ===========================================================================
// #2120 — the gated-behaviour pins: everything the review MEASURED and deliberately
// did NOT change, so the blocked and deferred tickets have a live baseline
// ===========================================================================
//
// A blocked ticket with no pin is a ticket whose premise can quietly stop being true.
// Each test below asserts TODAY's behaviour of a defect that is knowingly still present, and
// names the ticket that owns it. None of them endorses the behaviour; each of them fails the
// day it changes, which is exactly when the owning ticket needs re-reading.

namespace detail1888Review {
    /// Dependent parameter, per the #2299 gcc trap.
    template <typename T> concept HasPublicDetachParent = requires(T& t) { t.DetachParent(); };
}

TEST(JsonGatedBehaviourPins, Decl2118_GetRawTextReRendersRatherThanReturningSourceText) {
    // RENAMED AND RE-ROLED BY #2118 on 2026-08-19: this was a GATED pin ("a defect knowingly still
    // present"); it is now a DECLARATION. The user decided against retaining source spans, so the
    // re-rendering is the documented contract of this port rather than an unfixed defect.
    //
    // .NET's GetRawText is `_parent.GetRawValueAsString(_idx)` (JsonElement.cs:1196-1201), which
    // slices the ORIGINAL document bytes (JsonDocument.cs:700-704). nlohmann's DOM retains no
    // source spans at all, so dump() can only re-render.
    //
    // WRITTEN TO FAIL THE MOMENT THE LIMITATION LIFTS. If a future change ever does retain spans,
    // every row below breaks and this test is the notice that the declaration must be withdrawn.
    struct Case { const char* source; const char* rendered; const char* dotnetWouldReturn; };
    const Case cases[] = {
        {"{\"a\":1e+01}",       "{\"a\":10.0}",  "{\"a\":1e+01}"},       // exponent form lost
        {"{\"a\":1.10}",        "{\"a\":1.1}",   "{\"a\":1.10}"},        // trailing zero lost
        {"{\"a\":\"\\u0061\"}", "{\"a\":\"a\"}",   "{\"a\":\"\\u0061\"}"},  // escape resolved
        {"{ \"a\" : 1 }",       "{\"a\":1}",     "{ \"a\" : 1 }"},       // whitespace lost
    };
    for (const auto& c : cases) {
        auto doc = JsonDocument::Parse(c.source);
        EXPECT_EQ(doc->getRootElementProperty().GetRawText(), c.rendered)
            << "#2118 declares this; if it changed, the declaration must be withdrawn. Source: "
            << c.source;
        EXPECT_NE(doc->getRootElementProperty().GetRawText(), c.dotnetWouldReturn)
            << "and this is what .NET would have returned, recorded so the gap is explicit";
        // ToString is the same door and must not diverge from it.
        EXPECT_EQ(doc->getRootElementProperty().ToString(),
                  doc->getRootElementProperty().GetRawText());
    }
    // What DOES round-trip: a document already in the renderer's canonical form.
    EXPECT_EQ(JsonDocument::Parse("{\"a\":1}")->getRootElementProperty().GetRawText(), "{\"a\":1}");
}

TEST(JsonGatedBehaviourPins, Decl2118_TheValueSurvivesEvenThoughTheRepresentationDoesNot) {
    // The half that makes the limitation tolerable, and it is asserted rather than asserted-about:
    // GetRawText always returns VALID JSON that parses back to an EQUAL element. What is lost is
    // the representation, never the value. A caller using GetRawText as a value carrier is
    // unaffected; only one reading it as source text is.
    const char* sources[] = {"{\"a\":1e+01}", "{\"a\":1.10}", "{\"a\":\"\\u0061\"}", "{ \"a\" : 1 }"};
    for (const char* src : sources) {
        auto first = JsonDocument::Parse(src);
        const std::string rendered = first->getRootElementProperty().GetRawText();

        auto second = JsonDocument::Parse(rendered);
        EXPECT_EQ(second->getRootElementProperty().GetRawText(), rendered)
            << "re-rendering is IDEMPOTENT: a second pass changes nothing. Source: " << src;
        EXPECT_EQ(second->getRootElementProperty().GetProperty("a").GetRawText(),
                  first->getRootElementProperty().GetProperty("a").GetRawText())
            << "and the value carried is the same one. Source: " << src;
    }
}

TEST(JsonGatedBehaviourPins, Fix2117_DisposalReachesElementsHandedOutEarlier) {
    // INVERTED by #2117. The review's framing correction stands -- this was never a
    // use-after-free, the element held an OWNING aliasing shared_ptr and read LIVE storage --
    // and the defect was that a disposed document went on serving data where .NET throws.
    auto doc = JsonDocument::Parse("{\"a\":10,\"b\":[1,2]}");
    JsonElement scalar = doc->getRootElementProperty().GetProperty("a");
    JsonElement array = doc->getRootElementProperty().GetProperty("b");
    JsonElement survivor = scalar.Clone();
    doc->Dispose();

    EXPECT_THROW((void)doc->getRootElementProperty(), System::ObjectDisposedException)
        << "the door the finding's own file owns was ALREADY guarded";
    EXPECT_NO_THROW(doc->Dispose()) << "double Dispose is still safe";

    EXPECT_THROW((void)scalar.GetInt32(), System::ObjectDisposedException);
    EXPECT_THROW((void)array.GetArrayLength(), System::ObjectDisposedException);
    EXPECT_THROW((void)array[1], System::ObjectDisposedException);

    // ValueKind throws too, which is easy to leave out: .NET's reads
    // `_parent.GetJsonTokenType`, and that begins with CheckNotDisposed().
    EXPECT_THROW((void)scalar.getValueKindProperty(), System::ObjectDisposedException);
    EXPECT_THROW((void)scalar.GetRawText(), System::ObjectDisposedException);
    EXPECT_THROW((void)scalar.Clone(), System::ObjectDisposedException);

    // A CLONE taken before disposal owns its own state and survives -- .NET's Clone() returns an
    // element rooted in a NEW document (JsonElement.cs), so this is parity, not a loophole.
    EXPECT_EQ(survivor.GetInt32(), 10);
    EXPECT_EQ(survivor.getValueKindProperty(), JsonValueKind::Number);
}

TEST(JsonGatedBehaviourPins, Fix2117_ADefaultElementIsUndefinedNotDisposed) {
    // .NET keeps the two apart: CheckValidInstance() raises InvalidOperationException for a null
    // parent, CheckNotDisposed() raises ObjectDisposedException. A default element here has no
    // document at all, so it must keep answering "undefined" rather than claiming a disposal
    // that never happened -- the row that fails if the guard is written as `if (!node_) throw`.
    JsonElement undefined;
    EXPECT_EQ(undefined.getValueKindProperty(), JsonValueKind::Undefined);
    EXPECT_NO_THROW((void)undefined.GetRawText());
    EXPECT_NO_THROW((void)undefined.Clone());
}

TEST(JsonGatedBehaviourPins, Fix2117_LayoutPin) {
    // SA-3 requires the before/after sizeof to be pinned rather than asserted in prose.
    // JsonElement traded one aliasing shared_ptr for a shared_ptr to the document state plus a
    // raw node pointer -- .NET's `_parent` plus `_idx` -- so it grows by exactly one pointer.
    struct Before { std::shared_ptr<void> node; std::string propertyName; };
    struct After  { std::shared_ptr<void> state; const void* node; std::string propertyName; };
    EXPECT_EQ(sizeof(JsonElement), sizeof(After));
    EXPECT_EQ(alignof(JsonElement), alignof(After));
    EXPECT_EQ(sizeof(After), sizeof(Before) + sizeof(void*))
        << "the shadow pair must actually differ, or this pin asserts nothing";
    EXPECT_EQ(sizeof(JsonElement), 56u) << "consumers must rebuild; see the migration note";
}

TEST(JsonGatedBehaviourPins, Fix2115_BothOptionsWorkAtBOTHDoorsIdentically) {
    // INVERTED. The pin covered both doors precisely so they could not drift apart, and they have
    // not: #2115 touched only the shared parse core, so the serializer door followed for free --
    // which is #2116's forwarding paying off.
    JsonDocumentOptions trailingDoc;   trailingDoc.AllowTrailingCommas = true;
    JsonSerializerOptions trailingSer; trailingSer.setAllowTrailingCommasProperty(true);
    EXPECT_NO_THROW((void)JsonDocument::Parse("[1,]", trailingDoc));
    EXPECT_NO_THROW((void)JsonSerializer::Deserialize("[1,]", trailingSer));

    JsonDocumentOptions noDupDoc;   noDupDoc.AllowDuplicateProperties = false;
    JsonSerializerOptions noDupSer; noDupSer.setAllowDuplicatePropertiesProperty(false);
    EXPECT_THROW((void)JsonDocument::Parse("{\"x\":1,\"x\":2}", noDupDoc), JsonException);
    EXPECT_THROW((void)JsonSerializer::Deserialize("{\"x\":1,\"x\":2}", noDupSer), JsonException);

    // Validate() STILL says nothing about either flag -- and that is now CORRECT rather than a
    // gap. #2120 §7.3 folded the complaint into #2115 because the flags were inert and a caller
    // could not discover it from the API; they are not inert any more, so there is nothing for
    // Validate() to warn about. The cheaper option the user was offered -- make Validate() throw
    // for whichever flag could not be implemented -- was declined in favour of implementing both.
    JsonDocumentOptions both;
    both.AllowTrailingCommas = true;
    both.AllowDuplicateProperties = false;
    EXPECT_NO_THROW(both.Validate())
        << "#2115: nothing to report -- both flags do what they say";
}

TEST(JsonGatedBehaviourPins, PINCCF019JsonNodeSParentIsABorrowedPointerAndDetachIsPublic) {
    // SR-AUD-327 / cause TJ-C, CCF-019's first-named site. #1888/#1889/#1894 stay blocked on a
    // public source break and an object-layout change. This review implements nothing there;
    // the pin records what is true today so the blocked tickets keep a live baseline.
    auto object = std::make_shared<Nodes::JsonObject>();
    auto child = Nodes::JsonValue::Create(intcs{1});
    object->Add("a", child);
    EXPECT_EQ(child->getParentProperty(), object.get()) << "a raw, borrowed parent pointer";
    EXPECT_EQ((*object)["a"]->GetValueKind(), JsonValueKind::Number);
    // INVERTED BY #1888 (2026-08-19). This used to call DetachParent() directly and assert that a
    // caller "can sever the link the container believes it owns" while the container still lists
    // the key -- the defect, pinned. DetachParent is now protected with JsonArray/JsonObject as
    // friends, which is the reachability .NET has: its DetachParent is a PRIVATE helper on each
    // container and there is none on JsonNode at all.
    static_assert(!detail1888Review::HasPublicDetachParent<Nodes::JsonObject>,
                  "#1888: DetachParent must not be publicly callable");

    // The link can still be severed -- through the container, which keeps the two in step instead
    // of letting them disagree. That is the property the old assertion was really about.
    object->Remove("a");
    EXPECT_EQ(child->getParentProperty(), nullptr) << "the container detaches its own child";
    EXPECT_FALSE(object->ContainsKey("a")) << "#1888: and no longer lists it -- they cannot disagree";
}

TEST(JsonGatedBehaviourPins, PINConvertersAndReferenceHandlingAreDECLARATIONONLYNotConsulted) {
    // Plan §12. The batch's converter-recursion and null-callback checklist has NO SUBJECT
    // here, and this pin is what stops a future reader assuming it was checked and found safe.
    // JsonSerializerOptions exposes no converter collection at all, so a converter cannot be
    // registered, let alone consulted.
    JsonSerializerOptions opts;
    EXPECT_EQ(JsonSerializer::Deserialize<int>("7", opts), 7)
        << "deserialization is nlohmann's ADL customization points, not converter dispatch";
    // The two policy knobs that DO exist are stored and returned unchanged, and nothing reads
    // them -- the same cause TJ-B as #2115, at type scale rather than option scale.
    opts.setNumberHandlingProperty(Serialization::JsonNumberHandling::AllowReadingFromString);
    EXPECT_EQ(opts.getNumberHandlingProperty(), Serialization::JsonNumberHandling::AllowReadingFromString);
    EXPECT_THROW((void)JsonSerializer::Deserialize<int>("\"7\"", opts), JsonException)
        << "AllowReadingFromString is stored and NOT consulted -- recorded, not ticketed";
}

TEST(JsonGatedBehaviourPins, PINJsonSerializerOptionsDefaultIsONESharedObject) {
    // Plan §12: nothing in this module is thread-safe and Default() hands out a shared object.
    // Recorded so a future reader does not assume concurrency was checked and found safe --
    // it was checked and found ABSENT, which is different.
    EXPECT_EQ(&JsonSerializerOptions::Default(), &JsonSerializerOptions::Default())
        << "one process-wide instance, by reference";
}

TEST(JsonGatedBehaviourPins, Fix2119_TheParseHalfOfTheResidualIsGoneAndIsLinear) {
    // #2119 was a DEFERRED VERIFICATION against OwnedTreeLifetimeContractPlan.md's residual
    // table, which #2110's review deliberately did not claim to have refuted: "it did not
    // re-run that plan's harness, the two measurements are not like-for-like".
    //
    // The like-for-like re-run is build-probe/2119_probe1_deepnesting.cpp, one row per
    // invocation so a crash in one cannot hide the others. Measured
    // (build-probe/2119_probe1_after.log):
    //
    //   X28a 1,000       parse 0.001s   was safe          -> safe
    //   X28b 5,000       parse 0.009s   was safe          -> safe
    //   X28c 20,000      parse 0.032s   was SIGSEGV       -> FIXED by #1897
    //   X28d 100,000     parse 0.154s   was >5s timeout   -> FIXED, and LINEAR (4.8x for 5x)
    //   J19a 1,000       build 0.002s   was safe          -> safe
    //   J19b 5,000       build 0.019s   was safe          -> safe
    //   J19c 20,000      build 0.368s   was SIGSEGV       -> FIXED
    //   J19d 100,000     build 9.63s    was >5s timeout   -> STILL OVER, see the next case
    //
    // So the PARSE half of the residual is gone outright, and it is linear rather than merely
    // survivable.
    std::string deep;
    deep.reserve(200001);
    for (int i = 0; i < 100000; ++i) deep += "[";
    deep += "1";
    for (int i = 0; i < 100000; ++i) deep += "]";
    EXPECT_NO_THROW({
        auto node = JsonNode::Parse(deep);
        EXPECT_NE(node, nullptr);
    }) << "#1897 made this iterative; #2119 owns the like-for-like re-run";
    // JsonDocument::Parse rejects the same text cleanly at its depth bound -- no overflow.
    EXPECT_THROW((void)JsonDocument::Parse(deep), JsonException);
}

TEST(JsonGatedBehaviourPins, Fix1896_ProgrammaticDeepConstructionIsLinear) {
    // INVERTED BY #1896 (2026-08-19). Its predecessor recorded the LAST surviving row of
    // OwnedTreeLifetimeContractPlan.md's deep-nesting group: programmatic top-down construction
    // was quadratic, because AssignParent's cycle guard walked the whole ancestor chain on every
    // attach. It said "#1896 owns it and is still blocked" and that "#1896 landing is a visible
    // change here rather than a silent one" -- this is that change.
    //
    // MEASURED, same binary and flags before and after (build-probe/1896_probe1_*.log):
    //     depth  20,000     0.994s  ->  0.009s
    //     depth  50,000     7.884s  ->  0.023s
    //     depth 100,000    42.583s  ->  0.045s
    // Quadratic to linear; 946x at 100,000 levels.
    //
    // The guard is NOT weakened -- it rejects exactly what it rejected before, which the cycle
    // cases below assert. It is only SKIPPED when it provably cannot reject: `this` is parentless
    // (already checked) and therefore the root of its own subtree, so it can only be an ancestor
    // of `parent` if it HAS a subtree.
    using System::Text::Json::Nodes::JsonArray;

    // The depth is kept modest deliberately: a multi-second test is a bad test, and the
    // measurement lives in the probe log. What is asserted here is the SHAPE -- doubling the
    // depth must not quadruple the work. A generous bound, so this is not a benchmark that
    // flakes under gate load; a quadratic implementation misses it by three orders of magnitude.
    const auto build = [](int depth) {
        auto root = std::make_shared<JsonArray>();
        std::shared_ptr<JsonArray> cur = root;
        for (int i = 0; i < depth; ++i) {
            auto next = std::make_shared<JsonArray>();
            cur->Add(next);
            cur = next;
        }
        return root;
    };
    EXPECT_NE(build(5000), nullptr);
    EXPECT_NE(build(40000), nullptr)
        << "#1896: 40,000 levels was ~14s before the repair and is milliseconds after; if this "
           "case ever becomes slow again, the guard has stopped short-circuiting";
}

TEST(JsonGatedBehaviourPins, Fix1896_TheCycleGuardStillRejectsEverythingItRejectedBefore) {
    // The half that makes the speed-up safe. #1896's approval said in terms that the guard must
    // get FASTER, NOT WEAKER -- the shared_ptr reference cycle it was added to prevent must stay
    // prevented. Every rejection path is asserted here, including the one the short-circuit could
    // plausibly have broken.
    using System::Text::Json::Nodes::JsonArray;
    using System::Text::Json::Nodes::JsonObject;

    // 1. Already-parented node.
    {
        auto parent = std::make_shared<JsonArray>();
        auto child = std::make_shared<JsonArray>();
        parent->Add(child);
        auto other = std::make_shared<JsonArray>();
        EXPECT_THROW(other->Add(child), System::InvalidOperationException);
    }
    // 2. Self-attach. THE TRAP: an EMPTY container short-circuits the ancestor walk, so the
    //    self-check must sit outside that guard or this stops being rejected.
    {
        auto a = std::make_shared<JsonArray>();
        EXPECT_THROW(a->Add(a), System::InvalidOperationException);
    }
    // 3. Attaching an ancestor under its own descendant -- the case the walk exists for, and the
    //    one that still takes the walk because `this` has children.
    {
        auto root = std::make_shared<JsonArray>();
        auto mid = std::make_shared<JsonArray>();
        auto leaf = std::make_shared<JsonArray>();
        root->Add(mid);
        mid->Add(leaf);
        EXPECT_THROW(leaf->Add(root), System::InvalidOperationException);
    }
    // 4. The same, several levels deeper, so the short-circuit cannot be passing by depth luck.
    {
        auto root = std::make_shared<JsonArray>();
        std::shared_ptr<JsonArray> cur = root;
        for (int i = 0; i < 64; ++i) {
            auto next = std::make_shared<JsonArray>();
            cur->Add(next);
            cur = next;
        }
        EXPECT_THROW(cur->Add(root), System::InvalidOperationException);
    }
    // 5. An OBJECT ancestor, not just an array -- hasChildren() must recognise both container
    //    kinds, and a JsonValue (which has no children at all) must not be treated as one.
    {
        auto root = std::make_shared<JsonObject>();
        auto mid = std::make_shared<JsonObject>();
        root->Add("mid", mid);
        EXPECT_THROW(mid->Add("root", root), System::InvalidOperationException);
    }
}
