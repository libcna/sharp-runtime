// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// AM4-113: System/Console.hpp imported the float alias SharpRuntime::Single into namespace System,
// which also declares the static-utility class System::Single (System/Single.hpp). A translation
// unit that included both could not name System::Single at all ("reference to 'Single' is
// ambiguous" under Clang; GCC accepted it). AM4-009 removed the same import from Convert.hpp. This
// file is that translation unit: it compiles only while the two headers compose.
#include <gtest/gtest.h>
#include <limits>
#include "System/Console.hpp"
#include "System/Single.hpp"

TEST(ConsoleHeaderComposition, SystemSingleIsNameableAlongsideConsole) {
    EXPECT_TRUE(System::Single::IsNaN(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(System::Single::IsNaN(1.0f));
}
