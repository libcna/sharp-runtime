// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include "System/ArgumentNullException.hpp"
#include "System/Boolean.hpp"
#include "System/Byte.hpp"
#include "System/Double.hpp"
#include "System/FormatException.hpp"
#include "System/IFormattable.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/Object.hpp"
#include "System/Single.hpp"
#include "System/TryWriteInterpolatedStringHandler.hpp"

using System::TryWriteInterpolatedStringHandler;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, Ctor_SufficientBuffer_ShouldAppendTrue) {
    char buf[32];
    bool shouldAppend = false;
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf), 5, shouldAppend);
    EXPECT_TRUE(shouldAppend);
    EXPECT_TRUE(h.getSuccessProperty());
}

TEST(TryWriteInterpolatedStringHandlerTests, Ctor_InsufficientBuffer_ShouldAppendFalse) {
    char buf[3];
    bool shouldAppend = true;
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf), 10, shouldAppend);
    EXPECT_FALSE(shouldAppend);
    EXPECT_FALSE(h.getSuccessProperty());
}

// ---------------------------------------------------------------------------
// AppendLiteral
// ---------------------------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, AppendLiteral_String_Succeeds) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral(std::string("hello")));
    EXPECT_EQ(h.getCharsWrittenProperty(), 5u);
    EXPECT_EQ(h.getString(), "hello");
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendLiteral_Cstring_Succeeds) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral("world"));
    EXPECT_EQ(h.getString(), "world");
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendLiteral_BufferFull_Fails) {
    char buf[3];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_FALSE(h.AppendLiteral("toolong"));
    EXPECT_FALSE(h.getSuccessProperty());
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendLiteral_Multiple_Concatenates) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendLiteral("foo");
    h.AppendLiteral("bar");
    EXPECT_EQ(h.getString(), "foobar");
}

// ---------------------------------------------------------------------------
// AppendFormatted
// ---------------------------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_Int) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendLiteral("x=");
    h.AppendFormatted(42);
    EXPECT_EQ(h.getString(), "x=42");
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_Double) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(3.14);
    EXPECT_FALSE(h.getString().empty());
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_String) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendLiteral("name=");
    h.AppendFormatted(std::string("Alice"));
    EXPECT_EQ(h.getString(), "name=Alice");
}

// Was AppendFormatted_Bool, which asserted "1". That was this port's std::to_string
// spelling of a bool, not this port's own Boolean::ToString, and SR-AUD-133 named it
// as one of the three headline divergences. Ticket #2304 routes the arm to
// Boolean::ToString, so the assertion is now the text that sibling API answers.
TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_Bool_UsesBooleanToString) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(true);
    EXPECT_EQ(h.getString(), "True");
    EXPECT_EQ(h.getString(), System::Boolean::ToString(true));

    char buf2[32];
    TryWriteInterpolatedStringHandler h2(buf2, sizeof(buf2));
    h2.AppendFormatted(false);
    EXPECT_EQ(h2.getString(), "False");
}

// Was AppendFormatted_WithFormat_Ignored, whose name stated the defect and whose only
// assertion -- that the result is non-empty -- held just as well when the format was
// honoured, so it could never have detected the repair. It is now the pin on the
// headline case of SR-AUD-133: 255 under "X2" is "FF", not "255".
TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_WithFormat_IsHonoured) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(255, std::string("X2"));
    EXPECT_EQ(h.getString(), "FF");
    EXPECT_EQ(h.getString(), System::Int32::ToString(255, "X2"));
}

// ---------------------------------------------------------------------------
// Failure propagation
// ---------------------------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, AfterFailure_SubsequentAppends_False) {
    char buf[5];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendLiteral("hello");   // exactly fits
    bool ok = h.AppendLiteral("!"); // one too many
    EXPECT_FALSE(ok);
    EXPECT_FALSE(h.getSuccessProperty());
}

TEST(TryWriteInterpolatedStringHandlerTests, GetString_OnFailure_Empty) {
    char buf[3];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendLiteral("toolong");
    EXPECT_EQ(h.getString(), "");
}

// ---------------------------------------------------------------------------
// Raw-pointer boundaries — ticket #1810 / SR-AUD-132
//
// Before that ticket both raw-pointer boundaries were unvalidated. A null
// destination with a positive claimed capacity passed the capacity check in
// appendRaw() and reached std::memcpy(dest_ + pos_, ...) -- an ASan-confirmed
// WRITE to address zero, plus a UBSan "null pointer passed as argument 1, which
// is declared to never be null". The C-string literal overload independently
// forwarded a null to std::strlen, which crashed the same way. The .NET
// counterpart takes a Span<char>, which cannot represent a nonempty null
// destination at all, so these checks restore by validation what the .NET type
// gets from its parameter type.
// ---------------------------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, NullDestinationWithCapacity_Throws) {
    // The exact audited input.
    EXPECT_THROW(TryWriteInterpolatedStringHandler(nullptr, 1),
                 System::ArgumentNullException);
}

TEST(TryWriteInterpolatedStringHandlerTests, NullDestinationWithCapacity_FourArgCtor_Throws) {
    bool shouldAppend = false;
    EXPECT_THROW(TryWriteInterpolatedStringHandler(nullptr, 1, 0, shouldAppend),
                 System::ArgumentNullException);
    // The out-parameter is deliberately NOT written: an exception reports a
    // destination that does not exist, where shouldAppend=false would report a
    // destination that exists and is too small. Those are different failures.
    EXPECT_FALSE(shouldAppend);
}

TEST(TryWriteInterpolatedStringHandlerTests, NullDestination_NamesTheParameter) {
    try {
        TryWriteInterpolatedStringHandler h(nullptr, 64);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("destination"), std::string::npos)
            << "message was: " << e.what();
    }
}

TEST(TryWriteInterpolatedStringHandlerTests, NullDestinationWithZeroCapacity_IsValid) {
    // Deliberately accepted: (nullptr, 0) is this port's spelling of an empty
    // destination, it already behaved correctly before #1810, and it is the rule
    // tickets #1774 and #1805 settled for the same pointer/length shape.
    TryWriteInterpolatedStringHandler h(nullptr, 0);
    EXPECT_FALSE(h.AppendLiteral("x"));
    EXPECT_FALSE(h.getSuccessProperty());
    EXPECT_EQ(h.getCharsWrittenProperty(), 0u);
}

TEST(TryWriteInterpolatedStringHandlerTests, NullDestinationWithZeroCapacity_GetStringIsEmpty) {
    // getString() formed std::string(dest_, pos_) unconditionally; [nullptr,
    // nullptr) is not a valid range even though the count is zero.
    TryWriteInterpolatedStringHandler h(nullptr, 0);
    EXPECT_EQ(h.getString(), "");
}

TEST(TryWriteInterpolatedStringHandlerTests, NullLiteral_Throws) {
    char buf[8] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_THROW(h.AppendLiteral(static_cast<const char*>(nullptr)),
                 System::ArgumentNullException);
}

TEST(TryWriteInterpolatedStringHandlerTests, NullLiteral_NamesTheParameter) {
    char buf[8] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    try {
        h.AppendLiteral(static_cast<const char*>(nullptr));
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("value"), std::string::npos)
            << "message was: " << e.what();
    }
}

TEST(TryWriteInterpolatedStringHandlerTests, EmptyLiteralIsNotNull) {
    // The decided policy in one assertion: "" appends nothing and succeeds, a null
    // pointer throws. Treating null as empty would have collapsed these.
    char buf[8] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral(""));
    EXPECT_EQ(h.getCharsWrittenProperty(), 0u);
    EXPECT_TRUE(h.getSuccessProperty());
    EXPECT_THROW(h.AppendLiteral(static_cast<const char*>(nullptr)),
                 System::ArgumentNullException);
}

TEST(TryWriteInterpolatedStringHandlerTests, EmptyStdStringLiteralSucceeds) {
    // std::string("").data() may be non-null, but memcpy is undefined for a null
    // pointer even with a zero length, so appendRaw() returns early at len == 0.
    char buf[8] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral(std::string{}));
    EXPECT_EQ(h.getCharsWrittenProperty(), 0u);
}

TEST(TryWriteInterpolatedStringHandlerTests, ExactCapacityBoundaryStillFits) {
    // appendRaw()'s capacity test became `len > destLen_ - pos_` to avoid a size_t
    // wrap in `pos_ + len`. These pin that the boundary itself did not move.
    char buf[4] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral("ab"));
    EXPECT_TRUE(h.AppendLiteral("cd"));
    EXPECT_EQ(h.getCharsWrittenProperty(), 4u);
    EXPECT_TRUE(h.getSuccessProperty());
    EXPECT_FALSE(h.AppendLiteral("e"));
    EXPECT_FALSE(h.getSuccessProperty());
}

TEST(TryWriteInterpolatedStringHandlerTests, ZeroCapacityNonNullDestinationRefusesEveryAppend) {
    char buf[1] = {};
    TryWriteInterpolatedStringHandler h(buf, 0);
    EXPECT_TRUE(h.AppendLiteral(""));      // nothing to write, so nothing to refuse
    EXPECT_FALSE(h.AppendLiteral("x"));
    EXPECT_FALSE(h.getSuccessProperty());
}

TEST(TryWriteInterpolatedStringHandlerTests, ValidUsageUnchangedAfterValidationAdded) {
    char buf[16] = {};
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendLiteral("x="));
    EXPECT_TRUE(h.AppendFormatted(42));
    EXPECT_EQ(h.getCharsWrittenProperty(), 4u);
    EXPECT_TRUE(h.getSuccessProperty());
    EXPECT_EQ(h.getString(), "x=42");
}

// ---------------------------------------------------------------------------
// Formatting semantics — ticket #2304 / SR-AUD-133, and #2305
//
// Before #2304 AppendFormatted was a hand-written type map that discarded the
// format argument outright and answered with C++ default spellings. Measured on
// the unchanged tree (build-probe/2303_probe2_BEFORE.log): `true` was "1",
// `3.14` was "3.140000", 255 under "X2" was "255", every unrecognised specifier
// was silently accepted, and every type that was not arithmetic, std::string,
// const char* or char -- INCLUDING A STRING LITERAL, whose deduced type is
// char[N] and never matched the pointer arm -- collapsed to the placeholder
// "[?]", losing the value entirely. A real System::IFormattable implementer
// produced "[?]" as well, although the class doc-comment promised otherwise.
//
// The repair writes no format grammar. Each arm delegates to the sibling
// formatter this repository already ships for that category, so every assertion
// below is a text some existing, tested API already answers. The literal
// expectations are the primary statement; the cross-checks against the sibling
// are secondary, and deliberately not the only assertion, so a shared defect in
// both cannot pass unseen.
// ---------------------------------------------------------------------------

namespace {

// Implements the port's IFormattable, so it owns a format grammar of its own.
class Money : public System::IFormattable {
public:
    explicit Money(double amount) : amount_(amount) {}
    [[nodiscard]] std::string ToString(const std::string& format) const override {
        if (format.empty()) return "Money(" + System::Double::ToString(amount_) + ")";
        return "Money[" + System::Double::ToString(amount_, format) + "]";
    }

private:
    double amount_;
};

// A System::Object descendant: has ToString(), has no format overload.
class Tag : public System::Object {
public:
    [[nodiscard]] const std::string& GetTypeName() const override { return name_; }
    [[nodiscard]] std::string ToString() const override { return "Tag!"; }

private:
    inline static const std::string name_{"Tag"};
};

// Neither of the above: nothing to ask, so the placeholder is still correct.
struct Opaque {
    int x;
};

}  // namespace

// --- bool ------------------------------------------------------------------

// Boolean has no format overload in this repository, so a specifier supplied
// with a bool has nowhere to go and is ignored. That is the pre-existing
// behaviour of this category and is stated here rather than left implicit.
TEST(TryWriteInterpolatedStringHandlerTests, Bool_FormatIsIgnoredNotHonoured) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(true, std::string("X2"));
    EXPECT_EQ(h.getString(), "True");
}

// --- floating point --------------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, Double_UsesDoubleToString) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(3.14);
    EXPECT_EQ(h.getString(), "3.14");
    EXPECT_EQ(h.getString(), System::Double::ToString(3.14));

    char whole[32];
    TryWriteInterpolatedStringHandler h2(whole, sizeof(whole));
    h2.AppendFormatted(1.0);
    EXPECT_EQ(h2.getString(), "1");
}

TEST(TryWriteInterpolatedStringHandlerTests, Float_UsesSingleToString) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(2.5f);
    EXPECT_EQ(h.getString(), "2.5");
    EXPECT_EQ(h.getString(), System::Single::ToString(2.5f));

    // 0.1f, not 2.5f, is what distinguishes Single from Double: 2.5 is exact in
    // both precisions and reads the same either way, so a float routed to
    // Double::ToString by mistake would be invisible on it. Widening 0.1f to
    // double instead exposes the representation error.
    char narrow[64];
    TryWriteInterpolatedStringHandler h2(narrow, sizeof(narrow));
    h2.AppendFormatted(0.1f);
    EXPECT_EQ(h2.getString(), "0.1");
    EXPECT_EQ(h2.getString(), System::Single::ToString(0.1f));
    EXPECT_NE(h2.getString(), System::Double::ToString(static_cast<double>(0.1f)));
}

TEST(TryWriteInterpolatedStringHandlerTests, Double_HonoursFormat) {
    const struct {
        const char* spec;
        const char* expected;
    } cases[] = {
        {"F2", "3.14"},
        {"G4", "3.142"},
        {"E2", "3.14E+000"},
    };
    for (const auto& c : cases) {
        char buf[64];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(3.14159, std::string(c.spec));
        EXPECT_EQ(h.getString(), c.expected) << "specifier " << c.spec;
        EXPECT_EQ(h.getString(), System::Double::ToString(3.14159, c.spec)) << c.spec;
    }
}

TEST(TryWriteInterpolatedStringHandlerTests, Double_HonoursGroupSeparatorFormat) {
    char buf[64];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(1234.5, std::string("N2"));
    EXPECT_EQ(h.getString(), "1,234.50");
}

TEST(TryWriteInterpolatedStringHandlerTests, Float_HonoursFormat) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(2.5f, std::string("F1"));
    EXPECT_EQ(h.getString(), "2.5");
}

// --- integers, and the width each one is routed by -------------------------

TEST(TryWriteInterpolatedStringHandlerTests, Int_HonoursEveryFormatItsWrapperAccepts) {
    const struct {
        const char* spec;
        const char* expected;
    } cases[] = {
        {"X", "FF"}, {"X2", "FF"}, {"x2", "ff"}, {"D5", "00255"}, {"G", "255"},
    };
    for (const auto& c : cases) {
        char buf[64];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(255, std::string(c.spec));
        EXPECT_EQ(h.getString(), c.expected) << "specifier " << c.spec;
    }
}

TEST(TryWriteInterpolatedStringHandlerTests, Int_HonoursBinaryFormat) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(42, std::string("B8"));
    EXPECT_EQ(h.getString(), "00101010");
}

// One assertion per integral width, because the arm chooses its wrapper by
// sizeof and signedness: a mis-routed width would still produce plausible
// decimal text unformatted, and only the format overload exposes it.
TEST(TryWriteInterpolatedStringHandlerTests, EveryIntegralWidthReachesItsOwnWrapper) {
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(static_cast<unsigned char>(255), std::string("X2"));
        EXPECT_EQ(h.getString(), "FF");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(static_cast<signed char>(-5));
        EXPECT_EQ(h.getString(), "-5");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(static_cast<short>(4660), std::string("X4"));
        EXPECT_EQ(h.getString(), "1234");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(static_cast<unsigned short>(60000));
        EXPECT_EQ(h.getString(), "60000");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(4000000000u, std::string("X8"));
        EXPECT_EQ(h.getString(), "EE6B2800");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(5000000000LL, std::string("D12"));
        EXPECT_EQ(h.getString(), "005000000000");
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(18446744073709551615ULL);
        EXPECT_EQ(h.getString(), "18446744073709551615");
    }
    // A negative value in hexadecimal is what actually pins the WIDTH of the
    // wrapper the arm chose: each wrapper reinterprets the value at its own
    // width, so -1 is eight nibbles through Int32 and sixteen through Int64.
    // Every other case in this test would survive a four-versus-eight byte
    // mis-route unnoticed.
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(-1, std::string("X"));
        EXPECT_EQ(h.getString(), "FFFFFFFF");
        EXPECT_EQ(h.getString(), System::Int32::ToString(-1, "X"));
    }
    {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(static_cast<long long>(-1), std::string("X"));
        EXPECT_EQ(h.getString(), "FFFFFFFFFFFFFFFF");
    }
}

// The unformatted integer text is exactly what it was before #2304. This is the
// guard that the delegation did not quietly restyle the common case.
TEST(TryWriteInterpolatedStringHandlerTests, UnformattedIntegerTextIsUnchanged) {
    const int values[] = {0, 42, -7, 2147483647, -2147483647 - 1};
    for (int v : values) {
        char buf[32];
        TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
        h.AppendFormatted(v);
        EXPECT_EQ(h.getString(), std::to_string(v));
        EXPECT_EQ(h.getString(), System::Int32::ToString(v));
    }
}

// --- text ------------------------------------------------------------------

// A string literal deduces to char[N], never matched the const char* arm, and so
// produced the placeholder "[?]" -- a silent, total loss of the value. This is
// the door SR-AUD-133 does not mention and the most severe of the set.
TEST(TryWriteInterpolatedStringHandlerTests, StringLiteralIsAppendedNotPlaceholdered) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted("lit");
    EXPECT_EQ(h.getString(), "lit");
    EXPECT_NE(h.getString(), "[?]");
}

TEST(TryWriteInterpolatedStringHandlerTests, EmptyStringLiteralAppendsNothing) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendFormatted(""));
    EXPECT_EQ(h.getString(), "");
    EXPECT_EQ(h.getCharsWrittenProperty(), 0u);
}

// The char-array arm reads within the array bounds and stops at the first NUL,
// so an array that is not NUL-terminated cannot run off the end of it. Writing
// this arm as std::string(v) would have read past the last element.
TEST(TryWriteInterpolatedStringHandlerTests, CharArrayWithoutTerminatorStaysInBounds) {
    const char exact[3] = {'a', 'b', 'c'};
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(exact);
    EXPECT_EQ(h.getString(), "abc");
}

TEST(TryWriteInterpolatedStringHandlerTests, CharArrayStopsAtTheFirstNul) {
    const char padded[6] = {'a', 'b', 0, 'z', 'z', 0};
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(padded);
    EXPECT_EQ(h.getString(), "ab");
}

TEST(TryWriteInterpolatedStringHandlerTests, StringViewIsAppended) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(std::string_view("sv"));
    EXPECT_EQ(h.getString(), "sv");
}

TEST(TryWriteInterpolatedStringHandlerTests, TextIgnoresAFormatSpecifier) {
    // .NET's String is not IFormattable, so a specifier on a string is discarded
    // there too. Same for this port's char and char-pointer text.
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(std::string("hi"), std::string("X2"));
    EXPECT_EQ(h.getString(), "hi");

    char buf2[32];
    TryWriteInterpolatedStringHandler h2(buf2, sizeof(buf2));
    h2.AppendFormatted('a', std::string("X2"));
    EXPECT_EQ(h2.getString(), "a");
}

// --- #2305: the null char-pointer door -------------------------------------
//
// AppendLiteral(const char*) has rejected null since #1810. AppendFormatted(const
// char*) reached `std::string(v)` instead, whose NUL-terminated-array
// precondition a null violates: measured on the unchanged tree, libstdc++ threw
// std::logic_error out of this System-shaped API with nothing to catch it and the
// process aborted with exit 134 (build-probe/2303_probe3_nullptr_BEFORE.log).
// Same shape, same policy, same parameter name as #1810.

TEST(TryWriteInterpolatedStringHandlerTests, NullCharPointerThroughAppendFormatted_Throws) {
    char buf[32];
    const char* value = nullptr;
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_THROW(h.AppendFormatted(value), System::ArgumentNullException);
}

TEST(TryWriteInterpolatedStringHandlerTests, NullCharPointerThroughAppendFormatted_NamesTheParameter) {
    char buf[32];
    const char* value = nullptr;
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    try {
        h.AppendFormatted(value);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("value"), std::string::npos) << e.what();
    }
}

// A non-null pointer must still work: the guard rejects null, not pointers.
TEST(TryWriteInterpolatedStringHandlerTests, NonNullCharPointerStillAppends) {
    char buf[32];
    const char* value = "ptr";
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendFormatted(value));
    EXPECT_EQ(h.getString(), "ptr");
}

// --- the value's own contract ----------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, IFormattableReceivesTheFormat) {
    char buf[64];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(Money(12.5), std::string("F2"));
    EXPECT_EQ(h.getString(), "Money[12.50]");
}

TEST(TryWriteInterpolatedStringHandlerTests, IFormattableUnformattedGetsTheEmptyFormat) {
    char buf[64];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(Money(12.5));
    EXPECT_EQ(h.getString(), "Money(12.5)");
}

TEST(TryWriteInterpolatedStringHandlerTests, ObjectDescendantUsesItsOwnToString) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(Tag{});
    EXPECT_EQ(h.getString(), "Tag!");
}

// A type whose only ToString takes no argument has nowhere to put a specifier,
// so one supplied with it is ignored rather than rejected.
TEST(TryWriteInterpolatedStringHandlerTests, ToStringWithoutFormatIgnoresTheSpecifier) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(Tag{}, std::string("X2"));
    EXPECT_EQ(h.getString(), "Tag!");
}

// The placeholder still exists, and is still what a type with nothing to ask
// produces. #2304 narrowed which types reach it; it did not remove it.
TEST(TryWriteInterpolatedStringHandlerTests, TypeWithNoToStringIsStillThePlaceholder) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(Opaque{7});
    EXPECT_EQ(h.getString(), "[?]");
}

// --- the empty format string ------------------------------------------------

TEST(TryWriteInterpolatedStringHandlerTests, EmptyFormatEqualsTheUnformattedOverload) {
    char a[32];
    TryWriteInterpolatedStringHandler ha(a, sizeof(a));
    ha.AppendFormatted(3.14, std::string(""));

    char b[32];
    TryWriteInterpolatedStringHandler hb(b, sizeof(b));
    hb.AppendFormatted(3.14);

    EXPECT_EQ(ha.getString(), hb.getString());
    EXPECT_EQ(ha.getString(), "3.14");
}

// --- an unrecognised specifier ----------------------------------------------
//
// Delegating to the sibling formatters delegates their validation policy too:
// CCF-006 (tickets #1847/#1849) settled that an unrecognised specifier raises
// FormatException at the public boundary of all twelve numeric wrappers. Before
// #2304 this handler silently accepted every one of them.

TEST(TryWriteInterpolatedStringHandlerTests, UnrecognisedIntegerSpecifierThrows) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_THROW(h.AppendFormatted(42, std::string("Q")), System::FormatException);
}

TEST(TryWriteInterpolatedStringHandlerTests, MalformedIntegerWidthThrows) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_THROW(h.AppendFormatted(42, std::string("Dz")), System::FormatException);
}

TEST(TryWriteInterpolatedStringHandlerTests, UnrecognisedFloatSpecifierThrows) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_THROW(h.AppendFormatted(3.14, std::string("Q")), System::FormatException);

    // SAMPLE-028 correction: "Fz" is not an unrecognised specifier, it is a CUSTOM numeric
    // format string whose two characters are both literals. Measured against the reference
    // implementation: string.Format("{0:Fz}", 2.5f) is "Fz", while "{0:Q}" above does throw.
    char buf2[32];
    TryWriteInterpolatedStringHandler h2(buf2, sizeof(buf2));
    EXPECT_TRUE(h2.AppendFormatted(2.5f, std::string("Fz")));
    EXPECT_EQ(std::string(buf2, 2), "Fz");
}

// The specifier is validated while producing the text, before a single byte is
// handed to appendRaw, so a rejected specifier leaves the handler exactly as it
// was rather than half-written. That is what makes the throw safe to add to a
// type whose bool result already means "did it fit".
TEST(TryWriteInterpolatedStringHandlerTests, RejectedSpecifierLeavesTheHandlerUntouched) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    ASSERT_TRUE(h.AppendLiteral("x="));
    EXPECT_THROW(h.AppendFormatted(42, std::string("Q")), System::FormatException);
    EXPECT_EQ(h.getCharsWrittenProperty(), 2u);
    EXPECT_TRUE(h.getSuccessProperty());
    EXPECT_EQ(h.getString(), "x=");
    // and the handler is still usable afterwards
    EXPECT_TRUE(h.AppendFormatted(42));
    EXPECT_EQ(h.getString(), "x=42");
}

// --- capacity behaviour is unchanged by the new text ------------------------

TEST(TryWriteInterpolatedStringHandlerTests, FormattedTextStillRespectsCapacity) {
    // "False" is five characters where the old "0" was one, so the capacity
    // decision is made against the text actually produced, not against the old
    // spelling.
    char tight[4];
    TryWriteInterpolatedStringHandler h(tight, sizeof(tight));
    EXPECT_FALSE(h.AppendFormatted(false));
    EXPECT_FALSE(h.getSuccessProperty());
    EXPECT_EQ(h.getCharsWrittenProperty(), 0u);

    char roomy[8];
    TryWriteInterpolatedStringHandler h2(roomy, sizeof(roomy));
    EXPECT_TRUE(h2.AppendFormatted(false));
    EXPECT_EQ(h2.getString(), "False");
}

TEST(TryWriteInterpolatedStringHandlerTests, FormattedTextExactlyFillingTheBufferSucceeds) {
    char buf[2];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    EXPECT_TRUE(h.AppendFormatted(255, std::string("X2")));
    EXPECT_EQ(h.getString(), "FF");
    EXPECT_EQ(h.getCharsWrittenProperty(), 2u);
}
