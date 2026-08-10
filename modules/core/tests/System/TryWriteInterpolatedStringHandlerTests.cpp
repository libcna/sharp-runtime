// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>
#include "System/ArgumentNullException.hpp"
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

TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_Bool) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(true);
    EXPECT_EQ(h.getString(), "1");
}

TEST(TryWriteInterpolatedStringHandlerTests, AppendFormatted_WithFormat_Ignored) {
    char buf[32];
    TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
    h.AppendFormatted(255, std::string("X2"));
    EXPECT_FALSE(h.getString().empty());
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
