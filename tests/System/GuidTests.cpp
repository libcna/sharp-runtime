// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/Guid.hpp"

using System::Guid;

// ---------------------------------------------------------------------------
// Empty
// ---------------------------------------------------------------------------

TEST(GuidTests, EmptyIsAllZeros) {
    EXPECT_EQ(Guid::Empty.ToString(), "00000000-0000-0000-0000-000000000000");
}

TEST(GuidTests, DefaultConstructorEqualsEmpty) {
    Guid g;
    EXPECT_EQ(g, Guid::Empty);
}

TEST(GuidTests, EmptyByteArrayIsAllZeros) {
    for (uint8_t b : Guid::Empty.ToByteArray())
        EXPECT_EQ(b, 0u);
}

// ---------------------------------------------------------------------------
// String construction and ToString roundtrip
// ---------------------------------------------------------------------------

TEST(GuidTests, ParseRoundtrip) {
    // Official .NET test vector from GuidTests.cs: s_testGuid
    const std::string s = "a8a110d5-fc49-43c5-bf46-802db8f843ff";
    Guid g(s);
    EXPECT_EQ(g.ToString(), s);
}

TEST(GuidTests, ParseAllFFs) {
    // Guid with all bits set
    const std::string s = "ffffffff-ffff-ffff-ffff-ffffffffffff";
    Guid g(s);
    EXPECT_EQ(g.ToString(), s);
}

TEST(GuidTests, ParseZeroGuid) {
    Guid g("00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(g, Guid::Empty);
}

TEST(GuidTests, ToStringIsLowercase) {
    Guid g("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
    std::string s = g.ToString();
    for (char c : s)
        EXPECT_TRUE(c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "Character '" << c << "' is not lowercase hex or dash";
}

TEST(GuidTests, ToStringHas36Chars) {
    Guid g("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    EXPECT_EQ(g.ToString().size(), 36u);
}

TEST(GuidTests, ToStringDashPositions) {
    std::string s = Guid("a8a110d5-fc49-43c5-bf46-802db8f843ff").ToString();
    EXPECT_EQ(s[8],  '-');
    EXPECT_EQ(s[13], '-');
    EXPECT_EQ(s[18], '-');
    EXPECT_EQ(s[23], '-');
}

TEST(GuidTests, ParseBracedFormat) {
    // {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    Guid g("{a8a110d5-fc49-43c5-bf46-802db8f843ff}");
    EXPECT_EQ(g.ToString(), "a8a110d5-fc49-43c5-bf46-802db8f843ff");
}

TEST(GuidTests, ParseParenthesisFormat) {
    // (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
    Guid g("(a8a110d5-fc49-43c5-bf46-802db8f843ff)");
    EXPECT_EQ(g.ToString(), "a8a110d5-fc49-43c5-bf46-802db8f843ff");
}

TEST(GuidTests, ParseInvalidThrows) {
    EXPECT_THROW(Guid("not-a-guid"), std::exception);
    EXPECT_THROW(Guid("a8a110d5-fc49-43c5-bf46"), std::exception);
    EXPECT_THROW(Guid(""), std::exception);
}

// ---------------------------------------------------------------------------
// Byte-array constructor
// ---------------------------------------------------------------------------

TEST(GuidTests, ByteArrayConstructorRoundtrip) {
    Guid original("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    Guid copy(original.ToByteArray());
    EXPECT_EQ(copy, original);
}

TEST(GuidTests, ByteArrayAllZerosEqualsEmpty) {
    std::array<uint8_t, 16> zeros{};
    Guid g(zeros);
    EXPECT_EQ(g, Guid::Empty);
}

// ---------------------------------------------------------------------------
// Equality and ordering
// ---------------------------------------------------------------------------

TEST(GuidTests, EqualitySameString) {
    Guid a("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    Guid b("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a != b);
}

TEST(GuidTests, InequalityDifferentString) {
    Guid a("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    Guid b("00000000-0000-0000-0000-000000000001");
    EXPECT_NE(a, b);
}

TEST(GuidTests, LessThanOrdering) {
    Guid lo("00000000-0000-0000-0000-000000000001");
    Guid hi("00000000-0000-0000-0000-000000000002");
    EXPECT_LT(lo, hi);
    EXPECT_FALSE(hi < lo);
}

TEST(GuidTests, NotLessThanEqual) {
    Guid a("a8a110d5-fc49-43c5-bf46-802db8f843ff");
    EXPECT_FALSE(a < a);
}

// ---------------------------------------------------------------------------
// NewGuid — RFC 4122 version 4
// ---------------------------------------------------------------------------

TEST(GuidTests, NewGuidNotEqualEmpty) {
    // From .NET GuidTests.cs: Assert.NotEqual(Guid.Empty, guid1)
    Guid g = Guid::NewGuid();
    EXPECT_NE(g, Guid::Empty);
}

TEST(GuidTests, NewGuidTwoCallsDiffer) {
    // From .NET GuidTests.cs: Assert.NotEqual(guid1, guid2)
    Guid g1 = Guid::NewGuid();
    Guid g2 = Guid::NewGuid();
    EXPECT_NE(g1, g2);
}

TEST(GuidTests, NewGuidVersion4Bits) {
    // RFC 4122: byte[6] upper nibble must be 0x4 (version 4)
    Guid g = Guid::NewGuid();
    const auto& b = g.ToByteArray();
    EXPECT_EQ(b[6] & 0xF0u, 0x40u);
}

TEST(GuidTests, NewGuidVariantBits) {
    // RFC 4122: byte[8] top two bits must be 0b10xxxxxx
    Guid g = Guid::NewGuid();
    const auto& b = g.ToByteArray();
    EXPECT_EQ(b[8] & 0xC0u, 0x80u);
}

TEST(GuidTests, NewGuidToStringFormat) {
    // NewGuid().ToString() must be 36 chars in canonical form
    std::string s = Guid::NewGuid().ToString();
    ASSERT_EQ(s.size(), 36u);
    EXPECT_EQ(s[8],  '-');
    EXPECT_EQ(s[13], '-');
    EXPECT_EQ(s[18], '-');
    EXPECT_EQ(s[23], '-');
}

TEST(GuidTests, NewGuidMany_AllDiffer) {
    // Statistical check: 20 consecutive GUIDs should all be unique
    std::vector<Guid> guids;
    for (int i = 0; i < 20; ++i) guids.push_back(Guid::NewGuid());
    for (int i = 0; i < 20; ++i)
        for (int j = i + 1; j < 20; ++j)
            EXPECT_NE(guids[i], guids[j]) << "Duplicate at " << i << " and " << j;
}
