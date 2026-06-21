// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include "System/Console.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

using System::Console;
using System::ConsoleColor;
using System::ConsoleCancelEventArgs;
using System::ConsoleSpecialKey;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;

// ===========================================================================
// Write overloads — uintcs / ulongcs / vector<char>
// ===========================================================================

TEST(ConsoleWriteExtTests, Write_Uintcs_DoesNotThrow) {
    EXPECT_NO_THROW(Console::Write(static_cast<uintcs>(42u)));
}

TEST(ConsoleWriteExtTests, Write_Ulongcs_DoesNotThrow) {
    EXPECT_NO_THROW(Console::Write(static_cast<ulongcs>(99u)));
}

TEST(ConsoleWriteExtTests, Write_VectorChar_DoesNotThrow) {
    std::vector<char> buf = {'H', 'i'};
    EXPECT_NO_THROW(Console::Write(buf));
}

TEST(ConsoleWriteExtTests, Write_VectorChar_EmptyDoesNotThrow) {
    std::vector<char> buf;
    EXPECT_NO_THROW(Console::Write(buf));
}

TEST(ConsoleWriteExtTests, Write_VectorChar_WithIndexCount_DoesNotThrow) {
    std::vector<char> buf = {'A', 'B', 'C', 'D'};
    EXPECT_NO_THROW(Console::Write(buf, 1, 2));
}

TEST(ConsoleWriteExtTests, Write_VectorChar_ZeroCount_DoesNotThrow) {
    std::vector<char> buf = {'X'};
    EXPECT_NO_THROW(Console::Write(buf, 0, 0));
}

// ===========================================================================
// WriteLine overloads — uintcs / ulongcs / vector<char>
// ===========================================================================

TEST(ConsoleWriteLineExtTests, WriteLine_Uintcs_DoesNotThrow) {
    EXPECT_NO_THROW(Console::WriteLine(static_cast<uintcs>(7u)));
}

TEST(ConsoleWriteLineExtTests, WriteLine_Ulongcs_DoesNotThrow) {
    EXPECT_NO_THROW(Console::WriteLine(static_cast<ulongcs>(1234567890u)));
}

TEST(ConsoleWriteLineExtTests, WriteLine_VectorChar_DoesNotThrow) {
    std::vector<char> buf = {'O', 'K'};
    EXPECT_NO_THROW(Console::WriteLine(buf));
}

// ===========================================================================
// Cursor position
// ===========================================================================

TEST(ConsoleCursorTests, SetCursorPosition_StoresValues) {
    Console::SetCursorPosition(5, 3);
    EXPECT_EQ(Console::getCursorLeftProperty(), 5);
    EXPECT_EQ(Console::getCursorTopProperty(), 3);
    Console::SetCursorPosition(0, 0);
}

TEST(ConsoleCursorTests, GetCursorPosition_ReturnsStoredPair) {
    Console::SetCursorPosition(10, 4);
    auto [left, top] = Console::GetCursorPosition();
    EXPECT_EQ(left, 10);
    EXPECT_EQ(top, 4);
    Console::SetCursorPosition(0, 0);
}

TEST(ConsoleCursorTests, SetCursorLeftProperty_UpdatesColumn) {
    Console::SetCursorPosition(0, 2);
    Console::setCursorLeftProperty(7);
    EXPECT_EQ(Console::getCursorLeftProperty(), 7);
    EXPECT_EQ(Console::getCursorTopProperty(), 2);
    Console::SetCursorPosition(0, 0);
}

TEST(ConsoleCursorTests, SetCursorTopProperty_UpdatesRow) {
    Console::SetCursorPosition(3, 0);
    Console::setCursorTopProperty(9);
    EXPECT_EQ(Console::getCursorTopProperty(), 9);
    EXPECT_EQ(Console::getCursorLeftProperty(), 3);
    Console::SetCursorPosition(0, 0);
}

TEST(ConsoleCursorTests, Clear_ResetsCursorPosition) {
    Console::SetCursorPosition(5, 5);
    Console::Clear();
    EXPECT_EQ(Console::getCursorLeftProperty(), 0);
    EXPECT_EQ(Console::getCursorTopProperty(), 0);
}

// ===========================================================================
// Cursor size
// ===========================================================================

TEST(ConsoleCursorSizeTests, DefaultCursorSize_Is25) {
    // Reset to known default
    Console::setCursorSizeProperty(25);
    EXPECT_EQ(Console::getCursorSizeProperty(), 25);
}

TEST(ConsoleCursorSizeTests, CursorSize_RoundTrip) {
    Console::setCursorSizeProperty(50);
    EXPECT_EQ(Console::getCursorSizeProperty(), 50);
    Console::setCursorSizeProperty(25);
}

// ===========================================================================
// Window properties
// ===========================================================================

TEST(ConsoleWindowTests, WindowWidth_IsPositive) {
    EXPECT_GT(Console::getWindowWidthProperty(), 0);
}

TEST(ConsoleWindowTests, WindowHeight_IsPositive) {
    EXPECT_GT(Console::getWindowHeightProperty(), 0);
}

TEST(ConsoleWindowTests, WindowLeft_IsZero) {
    EXPECT_EQ(Console::getWindowLeftProperty(), 0);
}

TEST(ConsoleWindowTests, WindowTop_IsZero) {
    EXPECT_EQ(Console::getWindowTopProperty(), 0);
}

TEST(ConsoleWindowTests, LargestWindowWidth_GteWindowWidth) {
    EXPECT_GE(Console::getLargestWindowWidthProperty(), Console::getWindowWidthProperty());
}

TEST(ConsoleWindowTests, LargestWindowHeight_GteWindowHeight) {
    EXPECT_GE(Console::getLargestWindowHeightProperty(), Console::getWindowHeightProperty());
}

TEST(ConsoleWindowTests, SetWindowSize_DoesNotThrow) {
    EXPECT_NO_THROW(Console::SetWindowSize(80, 24));
}

TEST(ConsoleWindowTests, SetWindowPosition_DoesNotThrow) {
    EXPECT_NO_THROW(Console::SetWindowPosition(0, 0));
}

// ===========================================================================
// Buffer properties
// ===========================================================================

TEST(ConsoleBufferTests, BufferWidth_IsPositive) {
    EXPECT_GT(Console::getBufferWidthProperty(), 0);
}

TEST(ConsoleBufferTests, BufferHeight_IsPositive) {
    EXPECT_GT(Console::getBufferHeightProperty(), 0);
}

TEST(ConsoleBufferTests, SetBufferWidth_DoesNotThrow) {
    EXPECT_NO_THROW(Console::setBufferWidthProperty(80));
}

TEST(ConsoleBufferTests, SetBufferHeight_DoesNotThrow) {
    EXPECT_NO_THROW(Console::setBufferHeightProperty(24));
}

TEST(ConsoleBufferTests, SetBufferSize_DoesNotThrow) {
    EXPECT_NO_THROW(Console::SetBufferSize(80, 24));
}

// ===========================================================================
// MoveBufferArea
// ===========================================================================

TEST(ConsoleMoveBufferTests, MoveBufferArea_6Arg_DoesNotThrow) {
    EXPECT_NO_THROW(Console::MoveBufferArea(0, 0, 10, 5, 0, 5));
}

TEST(ConsoleMoveBufferTests, MoveBufferArea_9Arg_DoesNotThrow) {
    EXPECT_NO_THROW(Console::MoveBufferArea(0, 0, 10, 5, 0, 5,
        ' ', ConsoleColor::White, ConsoleColor::Black));
}

// ===========================================================================
// Keyboard state
// ===========================================================================

TEST(ConsoleKeyboardStateTests, CapsLock_ReturnsFalse) {
    EXPECT_FALSE(Console::getCapsLockProperty());
}

TEST(ConsoleKeyboardStateTests, NumberLock_ReturnsFalse) {
    EXPECT_FALSE(Console::getNumberLockProperty());
}

// ===========================================================================
// CancelKeyPress handler removal
// ===========================================================================

TEST(ConsoleCancelKeyTests, RemoveCancelKeyPressHandler_DoesNotThrow) {
    Console::addCancelKeyPressHandler([](void*, ConsoleCancelEventArgs&) {});
    EXPECT_NO_THROW(Console::removeCancelKeyPressHandler());
}

TEST(ConsoleCancelKeyTests, RemoveCancelKeyPressHandler_CanCallTwice) {
    EXPECT_NO_THROW(Console::removeCancelKeyPressHandler());
    EXPECT_NO_THROW(Console::removeCancelKeyPressHandler());
}

// ===========================================================================
// Beep
// ===========================================================================

TEST(ConsoleBeepTests, Beep_DoesNotThrow) {
    EXPECT_NO_THROW(Console::Beep());
}

TEST(ConsoleBeepTests, Beep_WithFreqDuration_DoesNotThrow) {
    EXPECT_NO_THROW(Console::Beep(800, 200));
}
