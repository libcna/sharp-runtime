// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2037 -- SR-AUD-302, cause N-C of docs/SystemNetNamespaceReviewPlan.md.
//
// The bracketed branch found ']', then searched for the next ':' ANYWHERE after it, so every
// character in between evaporated. Measured before the repair
// (build-probe/2037_probe1_before_after.log):
//
//   "[::1]ignored:80"   -> [::1]:80        (the finding's own case)
//   "[::1]ignored"      -> [::1]:0         (not named by the finding: no colon at all)
//   "[::1]x"            -> [::1]:0
//   "[::1] :80"         -> [::1]:80        (a SPACE, silently dropped)
//   "[fe80::1%7]bad:80" -> [fe80::1%7]:80
//
// while the unbracketed branch of the SAME function correctly rejected "1.2.3.4 :80". The
// repair is transcribed from the branch that was already right; no external reference was
// needed, which matters because /rv/tmp/runtime/src/libraries/ is absent from this container.
#include <gtest/gtest.h>
#include <string>
#include "System/FormatException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"

using System::FormatException;
using System::Net::IPAddress;
using System::Net::IPEndPoint;

namespace {

    // Parse and TryParse must agree on every input: one throws exactly where the other is false.
    void expectRejected(const std::string& text) {
        IPEndPoint parsed;
        EXPECT_FALSE(IPEndPoint::TryParse(text, parsed)) << "TryParse(\"" << text << "\")";
        EXPECT_THROW((void)IPEndPoint::Parse(text), FormatException) << "Parse(\"" << text << "\")";
    }

    void expectParsesAs(const std::string& text, const std::string& expected) {
        IPEndPoint parsed;
        ASSERT_TRUE(IPEndPoint::TryParse(text, parsed)) << "TryParse(\"" << text << "\")";
        EXPECT_EQ(parsed.ToString(), expected) << "TryParse(\"" << text << "\")";
        EXPECT_EQ(IPEndPoint::Parse(text).ToString(), expected) << "Parse(\"" << text << "\")";
    }

} // namespace

// ---------------------------------------------------------------------------
// The finding's own case, and every other shape of discarded trailing text.
// ---------------------------------------------------------------------------

TEST(IPEndPointParseTests, TextBetweenBracketAndColon_Rejected) {
    expectRejected("[::1]ignored:80");
    expectRejected("[fe80::1%7]bad:80");
    expectRejected("[::1]-:80");
}

TEST(IPEndPointParseTests, TrailingTextWithNoColonAtAll_Rejected) {
    // Not named by the finding: with no ':' after the bracket the old code cleared the port and
    // reported success, so the trailing run vanished just as silently.
    expectRejected("[::1]ignored");
    expectRejected("[::1]x");
    expectRejected("[::1]]");
}

TEST(IPEndPointParseTests, WhitespaceAfterBracket_Rejected_LikeTheUnbracketedBranch) {
    // "1.2.3.4 :80" was already rejected; "[::1] :80" was not. One function, two answers.
    expectRejected("[::1] :80");
    expectRejected("[::1]\t:80");
    IPEndPoint parsed;
    EXPECT_FALSE(IPEndPoint::TryParse("1.2.3.4 :80", parsed));
}

// ---------------------------------------------------------------------------
// Every currently-valid form must parse identically. These expectations are
// transcribed from the BEFORE half of the probe log.
// ---------------------------------------------------------------------------

TEST(IPEndPointParseTests, BracketedForms_Unchanged) {
    expectParsesAs("[::1]:80", "[::1]:80");
    expectParsesAs("[::1]:0", "[::1]:0");
    expectParsesAs("[::1]:65535", "[::1]:65535");
    expectParsesAs("[::1]", "[::1]:0");
    expectParsesAs("[fe80::1%7]:80", "[fe80::1%7]:80");
    expectParsesAs("[::]:0", "[::]:0");
}

TEST(IPEndPointParseTests, Fix2045_ATrailingColonWithNoPortIsRejected) {
    // INVERTED by #2045, whose gate was "the .NET reference tree is absent here so the intended
    // behaviour cannot be established" -- and the reference is present, so it is established:
    // IPEndPoint.cs:120-148 finds the port field structurally (`addressLength == s.Length`
    // means there is none) and otherwise runs `uint.TryParse(s.Slice(addressLength + 1), ...)`
    // with NumberStyles.None, which REJECTS an empty span. The ticket's inference was right.
    expectRejected("[::1]:");
    expectRejected("1.2.3.4:");

    // THE DISTINCTION THAT MAKES THIS A REPAIR AND NOT A NARROWING: no colon at all is a bare
    // address and still means port 0. Both shapes leave the port text empty, which is exactly
    // why the guard had to become "was a port field present" rather than "is the port text
    // non-empty" -- a mutation that reverts it is caught by these two rows together.
    expectParsesAs("1.2.3.4", "1.2.3.4:0");
    expectParsesAs("[::1]", "[::1]:0");
    expectParsesAs("::1", "[::1]:0");
    expectParsesAs("fe80::1%7", "[fe80::1%7]:0");

    // NumberStyles.None also means no sign and no whitespace, which this parser's digit loop
    // already enforced; asserted here so the port field's grammar is stated in one place.
    expectRejected("1.2.3.4:+80");
    expectRejected("1.2.3.4: 80");
    expectRejected("1.2.3.4:80 ");
    expectRejected("[::1]:+80");

    // A colon at position 0 is not a port separator: .NET requires `lastColonPos > 0`, and here
    // the empty address fails to parse, so both reject for their own reason and agree.
    expectRejected(":80");
    expectRejected(":");
}

TEST(IPEndPointParseTests, UnbracketedForms_Unchanged) {
    expectParsesAs("1.2.3.4:80", "1.2.3.4:80");
    expectParsesAs("1.2.3.4:65535", "1.2.3.4:65535");
    expectParsesAs("0.0.0.0:0", "0.0.0.0:0");
    expectParsesAs("255.255.255.255:65535", "255.255.255.255:65535");
    expectParsesAs("::1", "[::1]:0");
    expectParsesAs("fe80::1%7", "[fe80::1%7]:0");
}

// ---------------------------------------------------------------------------
// Malformed forms that were already rejected must stay rejected.
// ---------------------------------------------------------------------------

TEST(IPEndPointParseTests, AlreadyRejectedForms_StayRejected) {
    expectRejected("[::1]:80junk");
    expectRejected("[::1]:99999");
    expectRejected("[::1]::80");
    expectRejected("[]:80");
    expectRejected("[::1");
    expectRejected("[");
    expectRejected("[]");
    expectRejected("1.2.3.4:80junk");
    expectRejected("1.2.3.4:65536");
    expectRejected(" 1.2.3.4:80");
    expectRejected("");
}

// ---------------------------------------------------------------------------
// Round trips.
// ---------------------------------------------------------------------------

TEST(IPEndPointParseTests, ToStringRoundTripsThroughParse) {
    for (const char* text : {"[::1]:80", "1.2.3.4:80", "0.0.0.0:0",
                             "255.255.255.255:65535", "[fe80::1%7]:9", "[::]:0"}) {
        IPEndPoint parsed;
        ASSERT_TRUE(IPEndPoint::TryParse(text, parsed)) << text;
        EXPECT_EQ(parsed.ToString(), text);
        IPEndPoint again;
        ASSERT_TRUE(IPEndPoint::TryParse(parsed.ToString(), again)) << text;
        EXPECT_EQ(again, parsed) << text;
    }
}

TEST(IPEndPointParseTests, ParsedComponentsAreExact) {
    IPEndPoint parsed;
    ASSERT_TRUE(IPEndPoint::TryParse("[fe80::1%7]:9", parsed));
    EXPECT_EQ(parsed.getPortProperty(), 9);
    EXPECT_TRUE(parsed.getAddressProperty().getIsIPv6Property());
    EXPECT_EQ(parsed.getAddressProperty().getScopeIdProperty(), 7);

    ASSERT_TRUE(IPEndPoint::TryParse("1.2.3.4:80", parsed));
    EXPECT_EQ(parsed.getPortProperty(), 80);
    EXPECT_EQ(parsed.getAddressProperty(), IPAddress::Parse("1.2.3.4"));
}
