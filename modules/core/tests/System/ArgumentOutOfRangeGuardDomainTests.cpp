// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2254 (finding SR-AUD-091). The nine public
// System::ArgumentOutOfRangeException::ThrowIf* guard templates used to format every operand
// with a bare std::to_string(value), which imposed an UNDECLARED formatting requirement on top
// of each guard's declared comparison contract. Measured over 22 candidate types x 9 guards,
// exactly half of the advertised surface -- 99 of 198 (type, guard) pairs -- failed to compile,
// and it failed inside <bits/basic_string.h> naming nine standard-library candidates rather
// than at any stated sharp-runtime constraint.
//
// #2254 replaced the bare call with one private ordered if-constexpr formatter whose FIRST
// branch is the same std::to_string call, so every type that compiled before takes the identical
// path and emits byte-identical text, and added a static_assert per guard for the comparison
// operator that guard actually evaluates.
//
// This fixture pins the RUNTIME half: for every newly supported rendering branch, the exception
// type, the parameter name, the actual value, the message text and the HResult; and for the
// pre-existing arithmetic branch, that the emitted text did not move. The COMPILE-TIME half --
// that the deliberately rejected shapes really are rejected, per site -- is
// test/consumer/core_argument_out_of_range_guard_domain_negative.cpp, because a static_assert in
// a body that is never instantiated silently enforces nothing (the #2213 precedent). The
// measured before/after matrix lives in docs/CoreArgumentOutOfRangeGuardDomainPlan.md.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "System/ArgumentOutOfRangeException.hpp"

using System::ArgumentOutOfRangeException;

namespace {

// Branch 1 of the formatter: nothing below it is reachable for an arithmetic type.
// Branch 3: a scoped enumeration, which did not compile at all before #2254.
enum class Severity : int { Low = 1, High = 7 };

// Branch 3 with a negative underlying value, so the sign survives the cast.
enum class Offset : short { Behind = -3, Ahead = 4 };

// Branch 2: this repository's own rendering convention -- System::Object::ToString(), which
// Decimal, TimeSpan, DateTime and Guid all provide. .NET renders the actual value the same way.
struct Money {
    int cents;
    auto operator<=>(const Money&) const = default;
    bool operator==(const Money&) const = default;
    [[nodiscard]] std::string ToString() const { return "$" + std::to_string(cents); }
};

// Branch 5: a user type convertible to std::string but with no ToString() and no to_string.
struct Tag {
    std::string text;
    auto operator<=>(const Tag&) const = default;
    bool operator==(const Tag&) const = default;
    operator std::string() const { return text; }  // NOLINT(google-explicit-constructor)
};

// Branch 6: the documented ADL extension point -- the route a comparison-only type takes to
// become usable without this header paying for <sstream>.
namespace ext {
struct Slot {
    int index;
    auto operator<=>(const Slot&) const = default;
    bool operator==(const Slot&) const = default;
};
inline std::string to_string(Slot s) { return "slot#" + std::to_string(s.index); }
}  // namespace ext

// Captures the four observable properties of a thrown guard exception.
struct Thrown {
    bool threw = false;
    std::string paramName;
    std::string actualValue;
    std::string message;
    SharpRuntime::intcs hresult = 0;
};

template<typename Fn>
Thrown Capture(Fn&& fn) {
    Thrown t;
    try {
        fn();
    } catch (const ArgumentOutOfRangeException& ex) {
        t.threw = true;
        t.paramName = ex.getParamNameProperty();
        t.actualValue = ex.getActualValueProperty();
        t.message = ex.what();
        t.hresult = ex.getHResultProperty();
    }
    return t;
}

}  // namespace

// ---------------------------------------------------------------------------
// Branch 1 -- the pre-existing arithmetic path, pinned byte-identically.
//
// These are the regression pins for the whole ticket: if the formatter's branch order ever
// changed, an arithmetic type could fall through to a different branch and silently emit
// different text. All 121 first-party call sites in this repository instantiate an arithmetic T.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch1_Int_ActualValueAndMessageUnchanged) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfGreaterThan(10, 5, "val");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "val");
    EXPECT_EQ(t.actualValue, "10");
    EXPECT_NE(t.message.find("'val' must be less than or equal to 5."), std::string::npos);
    EXPECT_NE(t.message.find("Actual value was 10."), std::string::npos);
    EXPECT_EQ(t.hresult, static_cast<SharpRuntime::intcs>(0x80131502));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch1_NegativeInt_KeepsItsSign) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfNegative(-42, "count");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "-42");
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch1_Double_UsesStdToStringExactly) {
    // std::to_string(double) is fixed-format with six fractional digits. That is the text this
    // port has always emitted; the formatter must not have silently changed it.
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfZero(0.0, "ratio");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, std::to_string(0.0));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch1_UnsignedMaximum) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfGreaterThan(4294967295u, 1u, "big");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "4294967295");
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch1_UnscopedEnum_StillRendersItsInteger) {
    // An unscoped enumeration compiles only because integral promotion picks
    // std::to_string(int). That accident is pre-existing behaviour and is deliberately
    // preserved -- it is why the formatter's enumeration branch sits BELOW the to_string branch
    // and why a scoped enumeration now renders the same way.
    enum Level { Quiet = 0, Loud = 9 };
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(Loud, Loud, "level");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "9");
    EXPECT_NE(t.message.find("'level' must not equal 9."), std::string::npos);
}

// ---------------------------------------------------------------------------
// Branch 2 -- a ToString() member.  Did not compile before #2254.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch2_ToStringMember_RendersThroughToString) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfGreaterThan(Money{500}, Money{100}, "price");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "price");
    EXPECT_EQ(t.actualValue, "$500");
    EXPECT_NE(t.message.find("'price' must be less than or equal to $100."), std::string::npos);
    EXPECT_NE(t.message.find("Actual value was $500."), std::string::npos);
    EXPECT_EQ(t.hresult, static_cast<SharpRuntime::intcs>(0x80131502));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch2_ToStringMember_InRangeDoesNotThrow) {
    EXPECT_NO_THROW(
        ArgumentOutOfRangeException::ThrowIfGreaterThan(Money{100}, Money{500}, "price"));
}

// ---------------------------------------------------------------------------
// Branch 3 -- scoped enumerations.  Did not compile at all before #2254.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch3_ScopedEnum_RendersUnderlyingInteger) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(Severity::High, Severity::High, "severity");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "severity");
    EXPECT_EQ(t.actualValue, "7");
    EXPECT_NE(t.message.find("'severity' must not equal 7."), std::string::npos);
    EXPECT_EQ(t.hresult, static_cast<SharpRuntime::intcs>(0x80131502));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch3_ScopedEnum_NegativeUnderlyingKeepsItsSign) {
    // A short underlying type must not be rendered through an unsigned overload.
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfLessThan(Offset::Behind, Offset::Ahead, "offset");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "-3");
    EXPECT_NE(t.message.find("'offset' must be greater than or equal to 4."), std::string::npos);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch3_ScopedEnum_OrderingHonoursTheComparison) {
    EXPECT_NO_THROW(
        ArgumentOutOfRangeException::ThrowIfGreaterThan(Severity::Low, Severity::High, "sev"));
    EXPECT_THROW(
        ArgumentOutOfRangeException::ThrowIfGreaterThan(Severity::High, Severity::Low, "sev"),
        ArgumentOutOfRangeException);
}

namespace enumext {
enum class Colour : int { Red = 2, Blue = 5 };
inline std::string to_string(Colour c) { return c == Colour::Red ? "Red" : "Blue"; }
}  // namespace enumext

TEST(ArgumentOutOfRangeGuardDomainTests, Branch3_BeatsTheAdlBranchForAnEnumeration) {
    // The deliberate consequence of putting the enumeration branch ABOVE the ADL branch: an
    // enumeration cannot override its own rendering through to_string. The alternative order
    // would make a SCOPED enumeration print its name while an UNSCOPED one with the identical
    // to_string still printed its integer, because an unscoped enumeration is consumed by the
    // std::to_string branch above both. Consistency with the shipped behaviour wins; this test
    // is what makes that decision observable rather than merely asserted.
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(enumext::Colour::Blue, enumext::Colour::Blue,
                                                  "colour");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "5");
    EXPECT_EQ(t.actualValue.find("Blue"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Branch 4 -- std::string and std::string_view.
//
// ThrowIfEqual(std::string("a"), std::string("a"), "p") is the call NEXT.md section 2.7 named as
// the finding's widened reproducer: an obvious call that did not compile.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_String_RendersTheTextItself) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(std::string("a"), std::string("a"), "p");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "p");
    EXPECT_EQ(t.actualValue, "a");
    EXPECT_NE(t.message.find("'p' must not equal a."), std::string::npos);
    EXPECT_NE(t.message.find("Actual value was a."), std::string::npos);
    EXPECT_EQ(t.hresult, static_cast<SharpRuntime::intcs>(0x80131502));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_String_DistinctValuesDoNotThrow) {
    EXPECT_NO_THROW(
        ArgumentOutOfRangeException::ThrowIfEqual(std::string("a"), std::string("b"), "p"));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_String_OrderingUsesLexicographicCompare) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfGreaterThan(std::string("zulu"),
                                                        std::string("alpha"), "name");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "zulu");
    EXPECT_NE(t.message.find("'name' must be less than or equal to alpha."), std::string::npos);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_String_EmptyIsTheZeroValue) {
    // ThrowIfZero compares against T{}, which for std::string is the empty string. The guard's
    // static_assert names exactly that requirement.
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfZero(std::string(), "name");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "");
    EXPECT_NE(t.message.find("'name' must be a non-zero value."), std::string::npos);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_String_EmbeddedNulSurvivesIntoActualValue) {
    const std::string withNul("a\0b", 3);
    const Thrown t = Capture([&] {
        ArgumentOutOfRangeException::ThrowIfEqual(withNul, withNul, "p");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue.size(), 3u);
    EXPECT_EQ(t.actualValue, withNul);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_StringView_RendersTheTextItself) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfNotEqual(std::string_view("left"),
                                                     std::string_view("right"), "side");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "left");
    EXPECT_NE(t.message.find("'side' must equal right."), std::string::npos);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch4_StringView_NoDanglingAfterTheThrow) {
    // The formatter copies out of the view; nothing in the exception may alias the caller's
    // storage, which for a string_view built from a temporary would already be gone here.
    Thrown t;
    {
        const std::string owner = "ephemeral";
        t = Capture([&] {
            ArgumentOutOfRangeException::ThrowIfEqual(std::string_view(owner),
                                                      std::string_view(owner), "p");
        });
    }
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.actualValue, "ephemeral");
}

// ---------------------------------------------------------------------------
// Branch 5 -- a user type convertible to std::string.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch5_StringConvertible_RendersThroughTheConversion) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(Tag{"red"}, Tag{"red"}, "tag");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "tag");
    EXPECT_EQ(t.actualValue, "red");
    EXPECT_NE(t.message.find("'tag' must not equal red."), std::string::npos);
}

// ---------------------------------------------------------------------------
// Branch 6 -- the ADL extension point.
//
// This is the branch that replaces the rejected <sstream>/operator<< route: any comparison-only
// type becomes usable by declaring one free to_string beside it, and the header pays nothing.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, Branch6_AdlToString_RendersThroughTheUserFunction) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfGreaterThan(ext::Slot{9}, ext::Slot{2}, "slot");
    });
    ASSERT_TRUE(t.threw);
    EXPECT_EQ(t.paramName, "slot");
    EXPECT_EQ(t.actualValue, "slot#9");
    EXPECT_NE(t.message.find("'slot' must be less than or equal to slot#2."), std::string::npos);
    EXPECT_EQ(t.hresult, static_cast<SharpRuntime::intcs>(0x80131502));
}

TEST(ArgumentOutOfRangeGuardDomainTests, Branch6_AdlToString_InRangeDoesNotThrow) {
    EXPECT_NO_THROW(
        ArgumentOutOfRangeException::ThrowIfGreaterThan(ext::Slot{2}, ext::Slot{9}, "slot"));
}

// ---------------------------------------------------------------------------
// The formatter runs only on the throwing path.
//
// A guard that does not fire must not render its operand at all -- rendering can allocate, and
// for the ADL branch it runs arbitrary user code.
// ---------------------------------------------------------------------------

namespace {
int gRenderCount = 0;

namespace counted {
struct Counted {
    int v;
    auto operator<=>(const Counted&) const = default;
    bool operator==(const Counted&) const = default;
};
inline std::string to_string(Counted c) {
    ++gRenderCount;
    return std::to_string(c.v);
}
}  // namespace counted
}  // namespace

TEST(ArgumentOutOfRangeGuardDomainTests, Formatter_NotInvokedWhenTheGuardDoesNotFire) {
    gRenderCount = 0;
    ArgumentOutOfRangeException::ThrowIfGreaterThan(counted::Counted{1}, counted::Counted{9}, "c");
    EXPECT_EQ(gRenderCount, 0);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Formatter_InvokedTwiceForABinaryGuardThatFires) {
    // One render for the actual value, one for the bound named in the message. This pins the
    // count so that a future refactor cannot quietly render the same operand three times.
    gRenderCount = 0;
    EXPECT_THROW(
        ArgumentOutOfRangeException::ThrowIfGreaterThan(counted::Counted{9}, counted::Counted{1},
                                                        "c"),
        ArgumentOutOfRangeException);
    EXPECT_EQ(gRenderCount, 2);
}

TEST(ArgumentOutOfRangeGuardDomainTests, Formatter_InvokedOnceForAUnaryGuardThatFires) {
    gRenderCount = 0;
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfZero(counted::Counted{0}, "c"),
                 ArgumentOutOfRangeException);
    EXPECT_EQ(gRenderCount, 1);
}

// ---------------------------------------------------------------------------
// Every guard reaches the widened formatter, not only the ones used above.
// ---------------------------------------------------------------------------

TEST(ArgumentOutOfRangeGuardDomainTests, AllNineGuards_AcceptAWidenedType) {
    using S = std::string;
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfZero(S(), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_NO_THROW(ArgumentOutOfRangeException::ThrowIfNegative(S("a"), "p"));
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfNegativeOrZero(S(), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfGreaterThan(S("b"), S("a"), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfGreaterThanOrEqual(S("a"), S("a"), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfLessThan(S("a"), S("b"), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfLessThanOrEqual(S("a"), S("a"), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfEqual(S("a"), S("a"), "p"),
                 ArgumentOutOfRangeException);
    EXPECT_THROW(ArgumentOutOfRangeException::ThrowIfNotEqual(S("a"), S("b"), "p"),
                 ArgumentOutOfRangeException);
}

TEST(ArgumentOutOfRangeGuardDomainTests, DefaultParamName_StillWorksOnAWidenedType) {
    const Thrown t = Capture([] {
        ArgumentOutOfRangeException::ThrowIfEqual(Severity::Low, Severity::Low);
    });
    ASSERT_TRUE(t.threw);
    EXPECT_TRUE(t.paramName.empty());
    EXPECT_EQ(t.actualValue, "1");
}
