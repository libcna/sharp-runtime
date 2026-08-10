// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the RFC 5987 decoder of `ContentDispositionHeaderValue`:
//
//   #2127 / SR-AUD-323 / cause NH-J — the charset label was parsed only far enough to find the
//          delimiters and then discarded, so `iso-8859-1''foo-%E4.html` produced a raw 0xE4 byte
//          in a std::string the rest of this runtime reads as UTF-8, and `bogus''x` was accepted
//          as though the label meant nothing.
//
//   #2129 (post-audit, no SR-AUD identifier) / cause NH-K — a percent-encoded CR/LF/NUL decoded to
//          a raw one and was handed to the caller. ToString() re-encodes it, so it does NOT inject
//          on serialization; it travels INWARD. That is why #2129 is deliberately NOT a CCF-021
//          member (plan §8.3) even though it is the family's closest call.
#include <gtest/gtest.h>

#include <string>

#include "System/Net/Http/Headers/ContentDispositionHeaderValue.hpp"

using namespace System::Net::Http::Headers;

namespace {

/// Parses `attachment; filename*=<raw>` and returns the decoded filename*, or "" if the parameter
/// is reported absent (which is how this decoder has always signalled a malformed value).
std::string DecodeFileNameStar(const std::string& raw) {
    ContentDispositionHeaderValue value("attachment");
    if (!ContentDispositionHeaderValue::TryParse("attachment; filename*=" + raw, value)) return "";
    return value.getFileNameStarProperty();
}

} // namespace

TEST(Rfc5987DecoderTests, AnIso88591ValueIsTranscodedToUtf8) {
    // The finding's own example. 0xE4 is LATIN SMALL LETTER A WITH DIAERESIS in ISO-8859-1, so the
    // decoded text is 11 UTF-8 bytes (C3 A4 for the ä), not 10 with a stray 0xE4.
    const std::string decoded = DecodeFileNameStar("iso-8859-1''foo-%E4.html");
    EXPECT_EQ(decoded.size(), 11u) << "measured 10 before #2127";
    EXPECT_EQ(decoded, "foo-\xc3\xa4.html");
    // Case-insensitive, as a charset label is.
    EXPECT_EQ(DecodeFileNameStar("ISO-8859-1''foo-%E4.html"), "foo-\xc3\xa4.html");
}

TEST(Rfc5987DecoderTests, THECONTROLTheUtf8PathIsByteIdenticalToBefore) {
    EXPECT_EQ(DecodeFileNameStar("UTF-8''foo-%C3%A4.html"), "foo-\xc3\xa4.html");
    EXPECT_EQ(DecodeFileNameStar("utf-8''plain.txt"), "plain.txt");
    EXPECT_EQ(DecodeFileNameStar("UTF-8'en'foo.txt"), "foo.txt") << "the language tag is ignored";
}

TEST(Rfc5987DecoderTests, AnUnsupportedOrEmptyCharsetLabelIsRejected) {
    // RFC 5987 §3.2.1 names exactly two charsets a recipient must handle and requires it to reject
    // a value whose charset it does not support. Recorded as THIS PORT'S CHOICE: /rv is absent, so
    // whether .NET rejects the parameter or drops it silently could not be established here.
    EXPECT_EQ(DecodeFileNameStar("bogus''x"), "") << "measured as ACCEPTED before #2127";
    EXPECT_EQ(DecodeFileNameStar("''x"), "") << "an empty label is not a charset";
    EXPECT_EQ(DecodeFileNameStar("utf-16''x"), "");
    EXPECT_EQ(DecodeFileNameStar("windows-1252''x"), "");
}

TEST(Rfc5987DecoderTests, ATruncatedPercentEscapeIsRejected) {
    // A defect the finding does not name: the bound was `i + 2 < size`, so a truncated escape at
    // the very end fell through and was kept as literal text, turning malformed input into a
    // plausible-looking file name. This decoder already rejected a NON-HEX escape, so a truncated
    // one is rejected for the same reason.
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%"), "");
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%C"), "");
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%ZZ"), "") << "non-hex was already rejected";
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%41b"), "aAb") << "a complete escape still decodes";
}

TEST(Rfc5987DecoderTests, THEPINTheMalformedShapesThatWereAlreadyRejectedStayRejected) {
    EXPECT_EQ(DecodeFileNameStar("noquotes"), "") << "no delimiters at all";
    EXPECT_EQ(DecodeFileNameStar("UTF-8'only-one-quote"), "");
    EXPECT_EQ(DecodeFileNameStar("UTF-8'en'a'b"), "") << "a third quote is malformed";
}

// --- #2129: the decoded value must not carry protocol structure to the caller ------------------

TEST(Rfc5987DecoderTests, ADecodedCrLfOrNulIsRejectedRatherThanHandedToTheCaller) {
    // Before #2129 `filename*=UTF-8''a%0D%0Ab` was accepted and getFileNameStarProperty() returned
    // a string containing a raw CR/LF. ToString() percent-encodes it, so it does not inject on
    // serialization -- but a consumer that puts the decoded value into another header, a log line,
    // a filename or a Content-Disposition it builds itself does.
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%0D%0Ab"), "") << "measured as ACCEPTED before #2129";
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%0Db"), "");
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%0Ab"), "");
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%00b"), "");
    EXPECT_EQ(DecodeFileNameStar("iso-8859-1''a%0D%0Ab"), "") << "and on the Latin-1 path too";
}

TEST(Rfc5987DecoderTests, THECONTROLOtherControlCharactersAreNotTheSubjectOfNumber2129) {
    // #2129 is scoped to the three characters that terminate a protocol field, exactly like
    // CCF-021's predicate -- not to "control characters" in general. A decoded TAB or a decoded
    // ESC is not a framing hazard and stays accepted, so the narrowing cannot creep.
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%09b"), "a\tb");
    EXPECT_EQ(DecodeFileNameStar("UTF-8''a%1Bb"), "a\x1b" "b");
}

TEST(Rfc5987DecoderTests, AValidValueStillRoundTripsThroughToString) {
    ContentDispositionHeaderValue value("attachment");
    value.setFileNameStarProperty("foo-\xc3\xa4.html");
    EXPECT_EQ(value.ToString(), "attachment; filename*=UTF-8''foo-%C3%A4.html");
    EXPECT_EQ(value.getFileNameStarProperty(), "foo-\xc3\xa4.html");

    ContentDispositionHeaderValue reparsed("attachment");
    ASSERT_TRUE(ContentDispositionHeaderValue::TryParse(value.ToString(), reparsed));
    EXPECT_EQ(reparsed.getFileNameStarProperty(), "foo-\xc3\xa4.html");
}
