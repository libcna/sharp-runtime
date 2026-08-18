// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tickets #2163 (SR-AUD-243, cause CN-A), #2164 (SR-AUD-244, cause CN-B) and #2165.
//
// Two kinds of test live here and they must not be confused. The first kind asserts a REPAIR: a
// value that used to be accepted is now rejected, because the audit's own managed probe recorded
// current .NET rejecting it. The second kind is a PIN: a value that is still accepted, recorded
// deliberately, because .NET's answer for it is recollection rather than measurement with the /rv
// reference tree absent. Each pin names ticket #2166, which owns the question.
//
// Note on what can be observed: SetCursorPosition, SetWindowSize and SetWindowPosition write
// through std::printf while every other member uses std::cout, so redirecting std::cout's stream
// buffer -- the technique the other suites in this component use -- does not capture their escape
// sequences. The cursor assertions therefore check the rejection and the cached state, which is
// what the finding is about. Recorded in
// docs/SystemConsoleNamespaceReviewPlan.md section 4.2.
#include <gtest/gtest.h>

#include <climits>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Console.hpp"

using System::Console;
using System::ConsoleColor;
using SharpRuntime::intcs;

namespace {

/** Captures whatever the colour setters write to std::cout, and restores it on scope exit. */
class CoutCapture {
public:
    CoutCapture() : saved_(std::cout.rdbuf(sink_.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(saved_); }
    [[nodiscard]] std::string text() const { return sink_.str(); }

private:
    std::ostringstream sink_;
    std::streambuf* saved_;
};

ConsoleColor cast(int v) { return static_cast<ConsoleColor>(v); }

/** Restores the process-global console state so these tests cannot leak into their neighbours. */
class ConsoleDomainTest : public ::testing::Test {
protected:
    void SetUp() override {
        CoutCapture quiet;
        Console::ResetColor();
        Console::SetCursorPosition(0, 0);
    }
    void TearDown() override {
        CoutCapture quiet;
        Console::ResetColor();
        Console::SetCursorPosition(0, 0);
    }
};

} // namespace

// =================================================================================================
// SR-AUD-243 — the colour domain
// =================================================================================================

TEST_F(ConsoleDomainTest, EveryDefinedColourIsStillAcceptedAndEmitsItsUnchangedSequence) {
    // The invariance half of the repair: not one emitted byte may move for a valid value.
    static const char* const kForeground[16] = {
        "\033[30m", "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m", "\033[37m",
        "\033[90m", "\033[91m", "\033[92m", "\033[93m", "\033[94m", "\033[95m", "\033[96m", "\033[97m"};
    static const char* const kBackground[16] = {
        "\033[40m", "\033[41m", "\033[42m", "\033[43m", "\033[44m", "\033[45m", "\033[46m", "\033[47m",
        "\033[100m", "\033[101m", "\033[102m", "\033[103m", "\033[104m", "\033[105m", "\033[106m", "\033[107m"};

    for (int i = 0; i <= 15; ++i) {
        {
            CoutCapture capture;
            Console::setForegroundColorProperty(cast(i));
            EXPECT_EQ(capture.text(), kForeground[i]) << "foreground " << i;
        }
        EXPECT_EQ(static_cast<int>(Console::getForegroundColorProperty()), i);
        {
            CoutCapture capture;
            Console::setBackgroundColorProperty(cast(i));
            EXPECT_EQ(capture.text(), kBackground[i]) << "background " << i;
        }
        EXPECT_EQ(static_cast<int>(Console::getBackgroundColorProperty()), i);
    }
}

TEST_F(ConsoleDomainTest, AnUndefinedColourIsRejectedOnBothSetters) {
    // The audit's managed probe recorded current .NET as color=exception:System.ArgumentException
    // for a cast 99, because Unix ConsolePal rejects anything outside 0-15.
    for (int bad : {16, 99, 255, -1, -2, INT_MAX, INT_MIN}) {
        CoutCapture capture;
        EXPECT_THROW(Console::setForegroundColorProperty(cast(bad)), System::ArgumentException)
            << "foreground " << bad;
        EXPECT_THROW(Console::setBackgroundColorProperty(cast(bad)), System::ArgumentException)
            << "background " << bad;
        EXPECT_EQ(capture.text(), "") << "a rejected colour must emit nothing, value " << bad;
    }
}

TEST_F(ConsoleDomainTest, ARejectedColourLeavesTheGetterUntouched) {
    {
        CoutCapture quiet;
        Console::setForegroundColorProperty(ConsoleColor::Green);
        Console::setBackgroundColorProperty(ConsoleColor::DarkRed);
    }
    ASSERT_EQ(Console::getForegroundColorProperty(), ConsoleColor::Green);
    ASSERT_EQ(Console::getBackgroundColorProperty(), ConsoleColor::DarkRed);

    EXPECT_THROW(Console::setForegroundColorProperty(cast(99)), System::ArgumentException);
    EXPECT_THROW(Console::setBackgroundColorProperty(cast(99)), System::ArgumentException);

    // Before #2163 the invalid value was observable here; that is the second half of SR-AUD-243.
    EXPECT_EQ(Console::getForegroundColorProperty(), ConsoleColor::Green);
    EXPECT_EQ(Console::getBackgroundColorProperty(), ConsoleColor::DarkRed);
}

TEST_F(ConsoleDomainTest, TheColourRejectionNamesItsParameter) {
    try {
        Console::setForegroundColorProperty(cast(99));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "value");
    }
}

TEST_F(ConsoleDomainTest, IntMinNoLongerProducesATruncatedEscapeSequence) {
    // The measured extension to SR-AUD-243: ansiColor formats into char buf[12], sized for the
    // 0-15 domain, and INT_MIN used to emit "ESC[-21474836" -- truncated mid-number with no
    // terminating 'm'. A terminal that receives an unterminated escape consumes whatever output
    // follows it, so the consequence was not merely a wrong colour.
    CoutCapture capture;
    EXPECT_THROW(Console::setForegroundColorProperty(cast(INT_MIN)), System::ArgumentException);
    EXPECT_EQ(capture.text(), "");
}

TEST_F(ConsoleDomainTest, ResetColorStillWorksAfterARejectedSet) {
    EXPECT_THROW(Console::setForegroundColorProperty(cast(99)), System::ArgumentException);
    CoutCapture capture;
    Console::ResetColor();
    EXPECT_EQ(capture.text(), "\033[0m");
    EXPECT_EQ(Console::getForegroundColorProperty(), ConsoleColor::White);
    EXPECT_EQ(Console::getBackgroundColorProperty(), ConsoleColor::Black);
}

// =================================================================================================
// SR-AUD-244 — the cursor domain
// =================================================================================================

TEST_F(ConsoleDomainTest, ValidCursorPositionsAreStillAcceptedAndCached) {
    Console::SetCursorPosition(0, 0);
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{0}, intcs{0}));

    Console::SetCursorPosition(12, 34);
    EXPECT_EQ(Console::getCursorLeftProperty(), 12);
    EXPECT_EQ(Console::getCursorTopProperty(), 34);
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{12}, intcs{34}));

    Console::setCursorLeftProperty(5);
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{5}, intcs{34}));
    Console::setCursorTopProperty(6);
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{5}, intcs{6}));
}

TEST_F(ConsoleDomainTest, ANegativeCursorCoordinateIsRejected) {
    // The audit's managed probe recorded current .NET as
    // cursor=exception:System.ArgumentOutOfRangeException for (-1, 0).
    EXPECT_THROW(Console::SetCursorPosition(-1, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(0, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(-1, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(INT_MIN, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(0, INT_MIN), System::ArgumentOutOfRangeException);
}

TEST_F(ConsoleDomainTest, BothPropertyAliasesRejectANegativeCoordinateToo) {
    EXPECT_THROW(Console::setCursorLeftProperty(-5), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorTopProperty(-5), System::ArgumentOutOfRangeException);
}

TEST_F(ConsoleDomainTest, ARejectedCursorCoordinateLeavesTheCacheUntouched) {
    Console::SetCursorPosition(7, 9);
    ASSERT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{7}, intcs{9}));

    EXPECT_THROW(Console::SetCursorPosition(-1, 3), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(3, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorLeftProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorTopProperty(-1), System::ArgumentOutOfRangeException);

    // Before #2164 the cache held the negative value; the cache is a documented local reduction,
    // so an invalid coordinate survived in the process even where the terminal ignored it.
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{7}, intcs{9}));
}

TEST_F(ConsoleDomainTest, TheCursorRejectionNamesTheOffendingCoordinate) {
    try {
        Console::SetCursorPosition(-1, 0);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "left");
    }
    try {
        Console::SetCursorPosition(0, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "top");
    }
}

TEST_F(ConsoleDomainTest, ClearStillResetsTheCachedPosition) {
    Console::SetCursorPosition(4, 5);
    CoutCapture capture;
    Console::Clear();
    EXPECT_EQ(capture.text(), "\033[2J\033[1;1H");
    EXPECT_EQ(Console::GetCursorPosition(), std::make_pair(intcs{0}, intcs{0}));
}

// =================================================================================================
// #2166 — ANSWERED. Everything below was pinned by #2165 as behaviour it deliberately did not
// change, because ".NET's answer for it is recollection rather than measurement". /rv supplies the
// measurement, and it corrects the framing in one important way: the cursor bound is NOT a belief
// about a platform layer. .NET validates a cursor coordinate in Console.cs ITSELF, before dispatch
// (Console.cs:550-558), so it applies on every platform.
//
// The other doors are different, and the difference is worth stating. .NET's UNIX pal throws
// PlatformNotSupportedException for every one of them -- CursorSize's setter
// (ConsolePal.Unix.cs:193-197), SetWindowSize (:387-394), SetWindowPosition (:744-747),
// SetBufferSize (:727-730), the buffer setters (:305-315) and MoveBufferArea (:717-725) -- so on
// this port's runtime platform .NET states no range for them at all. The ranges below are its
// WINDOWS pal's, borrowed because they are the only ones .NET defines. Refusing outright would
// remove a feature this port offers rather than repair one, which is the wrong direction for a
// validation ticket.
// =================================================================================================

TEST_F(ConsoleDomainTest, Fix2166_ACursorCoordinateIsBoundedByShortMaxValueExclusive) {
    // The bound is EXCLUSIVE -- .NET's test is `left >= short.MaxValue` -- so 32766 is the last
    // accepted column and 32767 is the first rejected one. Getting that boundary off by one is the
    // easy mistake, so both sides of it are asserted.
    EXPECT_NO_THROW(Console::SetCursorPosition(32766, 32766));
    EXPECT_THROW(Console::SetCursorPosition(32767, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(0, 32767), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(INT_MAX, 0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetCursorPosition(-1, 0), System::ArgumentOutOfRangeException);

    // The identity, which is what makes this a transcription rather than a guess.
    try {
        Console::SetCursorPosition(32767, 0);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "left");
        EXPECT_EQ(e.getActualValueProperty(), "32767");
        const std::string message = e.what();
        EXPECT_NE(message.find("The value must be greater than or equal to zero and less than the "
                               "console's buffer size in that dimension."),
                  std::string::npos) << message;
    }
    // The SECOND parameter is named too, not just the first.
    try {
        Console::SetCursorPosition(0, -5);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "top");
    }

    // ...and the property aliases inherit it, because .NET's CursorLeft/CursorTop setters
    // delegate to SetCursorPosition.
    EXPECT_THROW(Console::setCursorLeftProperty(32767), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorTopProperty(32767), System::ArgumentOutOfRangeException);
}

TEST_F(ConsoleDomainTest, Fix2166_TheCursorSizeDomainIsEnforced) {
    const intcs original = Console::getCursorSizeProperty();
    EXPECT_THROW(Console::setCursorSizeProperty(0), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorSizeProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setCursorSizeProperty(101), System::ArgumentOutOfRangeException);
    EXPECT_EQ(Console::getCursorSizeProperty(), original) << "a rejected value must not be stored";

    // Both ends of the domain are accepted, so this is a domain check rather than a sign check.
    EXPECT_NO_THROW(Console::setCursorSizeProperty(1));
    EXPECT_NO_THROW(Console::setCursorSizeProperty(100));
    EXPECT_EQ(Console::getCursorSizeProperty(), 100);

    try {
        Console::setCursorSizeProperty(101);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "value");
        EXPECT_NE(std::string(e.what()).find(
                      "The cursor size is invalid. It must be a percentage between 1 and 100."),
                  std::string::npos) << e.what();
    }
    Console::setCursorSizeProperty(original);
}

TEST_F(ConsoleDomainTest, Fix2166_TheWindowAndBufferDoorsRejectInvalidArguments) {
    EXPECT_THROW(Console::SetWindowSize(-1, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetWindowSize(0, 0), System::ArgumentOutOfRangeException)
        << "a window of zero columns is not a window; .NET's check is ThrowIfNegativeOrZero";
    EXPECT_THROW(Console::SetWindowPosition(-1, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetBufferSize(-1, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetBufferSize(0, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::SetBufferSize(32767, 1), System::ArgumentOutOfRangeException)
        << "short.MaxValue is excluded here too";
    EXPECT_THROW(Console::setBufferWidthProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::setBufferHeightProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Console::MoveBufferArea(-1, -1, -1, -1, -1, -1), System::ArgumentOutOfRangeException);

    // Every one of MoveBufferArea's six coordinates is checked, and each names itself. A single
    // check on the first argument would satisfy the row above and nothing else.
    const char* names[] = {"sourceLeft", "sourceTop", "sourceWidth",
                           "sourceHeight", "targetLeft", "targetTop"};
    for (int bad = 0; bad < 6; ++bad) {
        intcs a[6] = {0, 0, 0, 0, 0, 0};
        a[bad] = -1;
        try {
            Console::MoveBufferArea(a[0], a[1], a[2], a[3], a[4], a[5]);
            ADD_FAILURE() << "argument " << bad << " was not checked";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_EQ(e.getParamNameProperty(), names[bad]);
        }
    }
    // The nine-argument overload delegates, so it inherits all six checks.
    EXPECT_THROW(Console::MoveBufferArea(-1, 0, 0, 0, 0, 0, ' ',
                                         System::ConsoleColor::Black, System::ConsoleColor::Black),
                 System::ArgumentOutOfRangeException);

    // The invariance rows: valid arguments still work, so this is validation and not a blanket
    // refusal of the doors.
    EXPECT_NO_THROW(Console::SetWindowSize(80, 25));
    EXPECT_NO_THROW(Console::SetWindowPosition(0, 0));
    EXPECT_NO_THROW(Console::SetBufferSize(80, 25));
    EXPECT_NO_THROW(Console::setBufferWidthProperty(80));
    EXPECT_NO_THROW(Console::setBufferHeightProperty(25));
    EXPECT_NO_THROW(Console::MoveBufferArea(0, 0, 1, 1, 0, 0));
}
