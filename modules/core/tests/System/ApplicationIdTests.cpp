// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2291 (SR-AUD-117) took all four of the review's decisions toward .NET, so every
// construction below moved: the token is bytes rather than text, and the two optional components
// are std::optional rather than always-present strings.
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "System/ApplicationId.hpp"
#include "System/ArgumentException.hpp"
#include "System/Version.hpp"

using System::ApplicationId;
using System::Version;
using Token = std::vector<SharpRuntime::bytecs>;

namespace {

const Token kToken{0xDE, 0xAD, 0xBE, 0xEF};

ApplicationId makeId() {
    return ApplicationId(kToken, "MyApp", Version(1, 2, 3, 4), "amd64", "neutral");
}

}  // namespace

TEST(ApplicationIdTests, GetName_CorrectValue) {
    EXPECT_EQ(makeId().getNameProperty(), "MyApp");
}
TEST(ApplicationIdTests, GetVersion_CorrectValue) {
    EXPECT_EQ(makeId().getVersionProperty().ToString(), "1.2.3.4");
}
TEST(ApplicationIdTests, GetProcessorArchitecture) {
    EXPECT_EQ(makeId().getProcessorArchitectureProperty(), std::optional<std::string>("amd64"));
}
TEST(ApplicationIdTests, GetCulture) {
    EXPECT_EQ(makeId().getCultureProperty(), std::optional<std::string>("neutral"));
}
TEST(ApplicationIdTests, GetPublicKeyToken) {
    EXPECT_EQ(makeId().getPublicKeyTokenProperty(), kToken);
}
TEST(ApplicationIdTests, Copy_IsEqual) {
    auto orig = makeId();
    auto copy = orig.Copy();
    EXPECT_TRUE(copy.Equals(orig));
}
TEST(ApplicationIdTests, Equals_SameFields_True) {
    EXPECT_TRUE(makeId().Equals(makeId()));
}
TEST(ApplicationIdTests, Equals_DiffName_False) {
    ApplicationId b(kToken, "Other", Version(1, 2, 3, 4), "amd64", "neutral");
    EXPECT_FALSE(makeId().Equals(b));
}
TEST(ApplicationIdTests, GetHashCode_Consistent) {
    EXPECT_EQ(makeId().GetHashCode(), makeId().GetHashCode());
}
TEST(ApplicationIdTests, OperatorEq_SameFields_True) {
    EXPECT_TRUE(makeId() == makeId());
}
TEST(ApplicationIdTests, OperatorNe_DiffName_True) {
    ApplicationId b(kToken, "Other", Version(1, 2, 3, 4), "amd64", "neutral");
    EXPECT_TRUE(makeId() != b);
}

// ---------------------------------------------------------------------------
// #2291 — the four decisions
// ---------------------------------------------------------------------------

TEST(ApplicationIdTests, Fix2291_1_AnEmptyNameIsRejected) {
    // .NET raises for a null or empty name (`ApplicationId.cs:17-23`); this port accepted "".
    EXPECT_THROW((ApplicationId(kToken, "", Version(1, 0), "amd64", "neutral")),
                 System::ArgumentException);
    EXPECT_NO_THROW((ApplicationId(kToken, "x", Version(1, 0), std::nullopt, std::nullopt)));
}

TEST(ApplicationIdTests, Fix2291_2_TheTokenIsBytesAndIsClonedBothWays) {
    // It was a std::string stored verbatim, so binary key material was unrepresentable and no
    // clone was made. .NET takes byte[] and clones on the way IN and on the way OUT
    // (`ApplicationId.cs:19` and `:34`), so a caller cannot mutate a stored token through either
    // array. A byte a std::string cannot hold at all is used deliberately.
    Token mutableSource{0x00, 0xFF, 0x7F};
    ApplicationId id(mutableSource, "MyApp", Version(1, 0), std::nullopt, std::nullopt);

    mutableSource[0] = 0x99;                       // mutate the caller's array after construction
    EXPECT_EQ(Token({0x00, 0xFF, 0x7F}), id.getPublicKeyTokenProperty())
        << "the constructor must have copied, as .NET's Clone() does";

    Token received = id.getPublicKeyTokenProperty();
    received[1] = 0x11;                            // mutate the array we were handed
    EXPECT_EQ(Token({0x00, 0xFF, 0x7F}), id.getPublicKeyTokenProperty())
        << "the getter must return a COPY -- a const& would defeat the constructor's own copy";

    // An embedded NUL is exactly what the old std::string representation could not carry.
    Token withNul{0x41, 0x00, 0x42};
    ApplicationId nulId(withNul, "MyApp", Version(1, 0), std::nullopt, std::nullopt);
    EXPECT_EQ(3u, nulId.getPublicKeyTokenProperty().size());
}

TEST(ApplicationIdTests, Fix2291_3_CultureAndArchitectureDistinguishAbsentFromEmpty) {
    // Both are `string?` in .NET and were non-nullable here, so absent and empty were one state.
    ApplicationId absent(kToken, "MyApp", Version(1, 0), std::nullopt, std::nullopt);
    ApplicationId empty(kToken, "MyApp", Version(1, 0), std::string{}, std::string{});

    EXPECT_EQ(std::nullopt, absent.getCultureProperty());
    EXPECT_EQ(std::optional<std::string>(""), empty.getCultureProperty());
    EXPECT_NE(absent, empty) << "these were indistinguishable before #2291";
}

TEST(ApplicationIdTests, Fix2291_4_ToStringIsDotNetsGrammar) {
    // `ApplicationId.cs:38-69`, transcribed. Lowercase quoted keys, absent components omitted,
    // token as UPPERCASE hex.
    EXPECT_EQ("MyApp, culture=\"neutral\", version=\"1.2.3.4\", publicKeyToken=\"DEADBEEF\""
              ", processorArchitecture =\"amd64\"",
              makeId().ToString());

    // Absent components are omitted entirely -- they used to be printed unconditionally.
    ApplicationId bare(Token{}, "Bare", Version(2, 0), std::nullopt, std::nullopt);
    EXPECT_EQ("Bare, version=\"2.0\", publicKeyToken=\"\"", bare.ToString());

    // TWO REFERENCE QUIRKS, TRANSCRIBED RATHER THAN TIDIED, because a caller may match on them.
    // (a) `processorArchitecture` carries a SPACE BEFORE ITS '=' where the other three keys do
    //     not -- `ApplicationId.cs:63`.
    EXPECT_NE(makeId().ToString().find(", processorArchitecture =\""), std::string::npos);
    EXPECT_EQ(std::string::npos, makeId().ToString().find(", processorArchitecture=\""));
    // (b) the token is emitted even when EMPTY, because .NET's guard is a null test on the array
    //     and a zero-length array is not null.
    EXPECT_NE(bare.ToString().find("publicKeyToken=\"\""), std::string::npos);

    // THE CONSEQUENCE THAT MOTIVATED IT: two identities differing only by token used to produce
    // IDENTICAL text, because the token was never included at all.
    ApplicationId other(Token{0x01}, "MyApp", Version(1, 2, 3, 4), "amd64", "neutral");
    EXPECT_NE(makeId().ToString(), other.ToString());
}

TEST(ApplicationIdTests, Fix2291_EqualsComparesTheTokenElementwiseAndTheHashDoesNot) {
    // `ApplicationId.cs:71-77` compares all five components, the token by sequence;
    // `:79-82` hashes NAME AND VERSION ONLY, and says why in its own comment.
    ApplicationId a(Token{0x01, 0x02}, "MyApp", Version(1, 0), "amd64", "neutral");
    ApplicationId b(Token{0x01, 0x03}, "MyApp", Version(1, 0), "amd64", "neutral");
    EXPECT_NE(a, b) << "Equals compares the token";
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode())
        << "...and the hash deliberately does not, which is permitted and is .NET's choice";
}
