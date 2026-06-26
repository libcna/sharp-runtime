// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>
#include "System/Func.hpp"

TEST(FuncTests, Func_NoArg_ReturnsInt) {
    System::Func<int> f = [] { return 42; };
    EXPECT_EQ(f(), 42);
}

TEST(FuncTests, FuncT_OneArg_ReturnsDouble) {
    System::FuncT<int, double> f = [](int x) { return x * 2.5; };
    EXPECT_DOUBLE_EQ(f(4), 10.0);
}

TEST(FuncTests, FuncT2_TwoArgs_ReturnsSum) {
    System::FuncT2<int, int, int> f = [](int a, int b) { return a + b; };
    EXPECT_EQ(f(3, 4), 7);
}

TEST(FuncTests, FuncT3_ThreeArgs_ReturnsConcatenated) {
    System::FuncT3<std::string, std::string, std::string, std::string> f =
        [](std::string a, std::string b, std::string c) { return a + b + c; };
    EXPECT_EQ(f("x", "y", "z"), "xyz");
}
