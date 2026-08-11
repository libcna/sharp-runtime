// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <functional>
#include <stdexcept>
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

TEST(FuncTests, FuncT4_FourArgs_ReturnsSum) {
    System::FuncT4<int, int, int, int, int> f = [](int a, int b, int c, int d) { return a + b + c + d; };
    EXPECT_EQ(f(1, 2, 3, 4), 10);
}

TEST(FuncTests, FuncT8_EightArgs_ReturnsSum) {
    System::FuncT8<int, int, int, int, int, int, int, int, int> f =
        [](int a, int b, int c, int d, int e, int g, int h, int i) {
            return a + b + c + d + e + g + h + i;
        };
    EXPECT_EQ(f(1, 2, 3, 4, 5, 6, 7, 8), 36);
}

TEST(FuncTests, FuncT16_SixteenArgs_ReturnsSum) {
    System::FuncT16<int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int> f =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l, int m,
           int n, int o, int p, int q) {
            return a + b + c + d + e + g + h + i + j + k + l + m + n + o + p + q;
        };
    EXPECT_EQ(f(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 136);
}

// ---------------------------------------------------------------------------
// #2300, from the SR-AUD-126 report's "Other missing assertions" bullets.
// These pin arity coverage and std::function's own empty-target diagnostic.
// None of them pins whether Func<void> should exist -- that is SR-AUD-126 and
// ticket #2299, and pinning it here would entrench the disputed surface.
// ---------------------------------------------------------------------------

TEST(FuncTests, Arities5Through7_CompileAndInvoke) {
    System::FuncT5<int, int, int, int, int, int> f5 =
        [](int a, int b, int c, int d, int e) { return a + b + c + d + e; };
    EXPECT_EQ(f5(1, 2, 3, 4, 5), 15);

    System::FuncT6<int, int, int, int, int, int, int> f6 =
        [](int a, int b, int c, int d, int e, int g) { return a + b + c + d + e + g; };
    EXPECT_EQ(f6(1, 2, 3, 4, 5, 6), 21);

    System::FuncT7<int, int, int, int, int, int, int, int> f7 =
        [](int a, int b, int c, int d, int e, int g, int h) { return a + b + c + d + e + g + h; };
    EXPECT_EQ(f7(1, 2, 3, 4, 5, 6, 7), 28);
}

TEST(FuncTests, Arities9Through15_CompileAndInvoke) {
    System::FuncT9<int, int, int, int, int, int, int, int, int, int> f9 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j) {
            return a + b + c + d + e + g + h + i + j;
        };
    EXPECT_EQ(f9(1, 2, 3, 4, 5, 6, 7, 8, 9), 45);

    System::FuncT10<int, int, int, int, int, int, int, int, int, int, int> f10 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k) {
            return a + b + c + d + e + g + h + i + j + k;
        };
    EXPECT_EQ(f10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), 55);

    System::FuncT11<int, int, int, int, int, int, int, int, int, int, int, int> f11 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l) {
            return a + b + c + d + e + g + h + i + j + k + l;
        };
    EXPECT_EQ(f11(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11), 66);

    System::FuncT12<int, int, int, int, int, int, int, int, int, int, int, int, int> f12 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l, int m) {
            return a + b + c + d + e + g + h + i + j + k + l + m;
        };
    EXPECT_EQ(f12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), 78);

    System::FuncT13<int, int, int, int, int, int, int, int, int, int, int, int, int, int> f13 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l, int m,
           int n) { return a + b + c + d + e + g + h + i + j + k + l + m + n; };
    EXPECT_EQ(f13(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13), 91);

    System::FuncT14<int, int, int, int, int, int, int, int, int, int, int, int, int, int, int> f14 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l, int m,
           int n, int o) { return a + b + c + d + e + g + h + i + j + k + l + m + n + o; };
    EXPECT_EQ(f14(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14), 105);

    System::FuncT15<int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int> f15 =
        [](int a, int b, int c, int d, int e, int g, int h, int i, int j, int k, int l, int m,
           int n, int o, int p) { return a + b + c + d + e + g + h + i + j + k + l + m + n + o + p; };
    EXPECT_EQ(f15(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15), 120);
}

// An empty Func is contextually false and throws std::bad_function_call when
// invoked. That is std::function's own diagnostic, not a System::Exception --
// the report notes it was undocumented, and the header now says so.
TEST(FuncTests, EmptyFunc_ThrowsBadFunctionCall) {
    System::Func<int> empty;
    EXPECT_FALSE(static_cast<bool>(empty));
    EXPECT_THROW(empty(), std::bad_function_call);

    System::FuncT<int, int> emptyOneArg;
    EXPECT_FALSE(static_cast<bool>(emptyOneArg));
    EXPECT_THROW(emptyOneArg(1), std::bad_function_call);
}

// A Func propagates whatever its target throws, unchanged; std::function adds
// no wrapping of its own.
TEST(FuncTests, TargetExceptionPropagatesUnchanged) {
    System::Func<int> thrower = [] () -> int { throw std::runtime_error("from target"); };
    try {
        (void)thrower();
        FAIL() << "expected the target's exception";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "from target");
    }
}

// The result type is preserved exactly, references included: a Func returning
// a reference hands back the caller's object, not a copy of it.
TEST(FuncTests, ReferenceResultIsNotCopied) {
    int storage = 5;
    System::Func<int&> byReference = [&storage] () -> int& { return storage; };
    byReference() = 11;
    EXPECT_EQ(storage, 11);
    EXPECT_EQ(&byReference(), &storage);
}
