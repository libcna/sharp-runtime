// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2009 (SR-AUD-295, cause T-C of
// docs/SystemTextNamespaceReviewPlan.md, CCF-004's fourth module).
//
// `StringBuilder::CopyTo` evaluated `destinationIndex > destinationLength - count` on
// `intcs` without ever validating `destinationLength` -- the one parameter of the five that
// had no guard. The consequence is sharper than the finding records: for
// destinationLength == INTCS_MIN and count 1 the subtraction wraps to INTCS_MAX, so
// `0 > INTCS_MAX` is FALSE, the guard PASSES, and std::copy runs. The undefined arithmetic
// did not accompany the bounds check, it DEFEATED it.
//
// Measured before (build-probe/2007_asan_before.log):
//   StringBuilder.cpp:146:50: runtime error: signed integer overflow: -2147483648 - 4
//   AddressSanitizer: heap-buffer-overflow -- four bytes written into a two-byte buffer
// Measured after (build-probe/2007_asan_after.log):
//   System exception (Non-negative number required. (Parameter 'destinationLength'))
//   and the valid copy still returns "ABCD" with no diagnostic.
//
// The suite lives in its own file because StringBuilderTests.cpp has no CopyTo coverage at
// all -- the audit's own note that the finding "has no regression".

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/StringBuilder.hpp"

using SharpRuntime::intcs;
using System::Text::StringBuilder;

namespace {
constexpr intcs kIntcsMin = -2147483647 - 1;
constexpr intcs kIntcsMax = 2147483647;
} // namespace

TEST(StringBuilderCopyToTests, NegativeDestinationLengthThrowsBeforeAnyArithmetic) {
    StringBuilder sb("ABCD");
    char dest[8] = {};
    for (intcs capacity : {kIntcsMin, static_cast<intcs>(-1), static_cast<intcs>(-5)}) {
        SCOPED_TRACE(capacity);
        try {
            sb.CopyTo(0, dest, capacity, 0, 1);
            FAIL() << "expected ArgumentOutOfRangeException";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_EQ("destinationLength", e.getParamNameProperty());
        }
    }
}

TEST(StringBuilderCopyToTests, NegativeDestinationLengthWithZeroCountAlsoThrows) {
    // Before #2009 this returned normally: `0 > -5 - 0` is false, so a negative capacity was
    // simply accepted whenever count was zero.
    StringBuilder sb("ABCD");
    char dest[8] = {};
    EXPECT_THROW(sb.CopyTo(0, dest, -5, 0, 0), System::ArgumentOutOfRangeException);
}

TEST(StringBuilderCopyToTests, TheOverflowNoLongerDefeatsTheBoundsCheck) {
    // The headline case: INTCS_MIN capacity with a count large enough to overrun. Before
    // #2009 this wrote four bytes past a two-byte buffer and returned normally.
    StringBuilder sb("ABCD");
    char dest[2] = {'\0', '\0'};
    EXPECT_THROW(sb.CopyTo(0, dest, kIntcsMin, 0, 4), System::ArgumentOutOfRangeException);
    EXPECT_EQ('\0', dest[0]);
    EXPECT_EQ('\0', dest[1]);
}

TEST(StringBuilderCopyToTests, MaximalCapacityDoesNotOverflowEither) {
    // The other end of the same subtraction: destinationLength == INTCS_MAX with a negative
    // count is rejected by the count guard, and with a valid count the widened comparison
    // must still admit the copy.
    StringBuilder sb("ABCD");
    char dest[8] = {};
    EXPECT_THROW(sb.CopyTo(0, dest, kIntcsMax, 0, -1), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(sb.CopyTo(0, dest, 8, 0, 4));
    EXPECT_EQ(0, std::memcmp(dest, "ABCD", 4));
}

TEST(StringBuilderCopyToTests, DestinationExactlyLargeEnoughSucceeds) {
    StringBuilder sb("ABCD");
    char dest[4] = {};
    EXPECT_NO_THROW(sb.CopyTo(0, dest, 4, 0, 4));
    EXPECT_EQ(0, std::memcmp(dest, "ABCD", 4));
}

TEST(StringBuilderCopyToTests, DestinationOneUnitTooSmallStillThrowsArgumentException) {
    // Unchanged by #2009: the pre-existing message and exception type are preserved.
    StringBuilder sb("ABCD");
    char dest[4] = {};
    try {
        sb.CopyTo(0, dest, 3, 0, 4);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentOutOfRangeException&) {
        FAIL() << "the capacity is valid here; this must stay ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string::npos,
                  e.getMessageProperty().find("out of bounds for the destination array"));
    }
}

TEST(StringBuilderCopyToTests, TheFourPreExistingGuardsAreUnchanged) {
    StringBuilder sb("ABCD");
    char dest[8] = {};
    EXPECT_THROW(sb.CopyTo(0, nullptr, 8, 0, 1), System::ArgumentNullException);
    EXPECT_THROW(sb.CopyTo(0, dest, 8, 0, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(sb.CopyTo(0, dest, 8, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(sb.CopyTo(-1, dest, 8, 0, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(sb.CopyTo(5, dest, 8, 0, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(sb.CopyTo(3, dest, 8, 0, 2), System::ArgumentException);
}

TEST(StringBuilderCopyToTests, EveryValidCopyIsUnchanged) {
    StringBuilder sb("ABCDEF");
    char dest[16];
    std::memset(dest, '.', sizeof(dest));
    sb.CopyTo(2, dest, 16, 4, 3);
    EXPECT_EQ(0, std::memcmp(dest, "....CDE", 7));
    EXPECT_EQ('.', dest[7]);

    // A zero-length copy at the very end of both buffers.
    std::memset(dest, '.', sizeof(dest));
    EXPECT_NO_THROW(sb.CopyTo(6, dest, 16, 16, 0));
    EXPECT_EQ('.', dest[0]);

    // An empty builder.
    StringBuilder empty;
    EXPECT_NO_THROW(empty.CopyTo(0, dest, 16, 0, 0));
}
