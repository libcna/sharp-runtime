// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UIntPtr.hpp"

using System::UIntPtr;

TEST(UIntPtrTest, DefaultCtorIsZero) {
    UIntPtr p;
    EXPECT_EQ(p.value, 0UL);
}

TEST(UIntPtrTest, ConstructFromValue) {
    UIntPtr p(42);
    EXPECT_EQ(p.value, 42UL);
}

TEST(UIntPtrTest, ConstructFromPointer) {
    int x = 0;
    UIntPtr p(&x);
    EXPECT_EQ(p.value, reinterpret_cast<uintptr_t>(&x));
}

TEST(UIntPtrTest, ZeroStatic) {
    EXPECT_EQ(UIntPtr::Zero.value, 0UL);
}

TEST(UIntPtrTest, IsZeroTrue) {
    UIntPtr p;
    EXPECT_TRUE(p.IsZero());
}

TEST(UIntPtrTest, IsZeroFalse) {
    UIntPtr p(1);
    EXPECT_FALSE(p.IsZero());
}

TEST(UIntPtrTest, EqualityTrue) {
    UIntPtr a(10), b(10);
    EXPECT_TRUE(a == b);
}

TEST(UIntPtrTest, EqualityFalse) {
    UIntPtr a(10), b(20);
    EXPECT_TRUE(a != b);
}

TEST(UIntPtrTest, CastToUintptr) {
    UIntPtr p(99);
    EXPECT_EQ(static_cast<uintptr_t>(p), 99UL);
}

TEST(UIntPtrTest, CastToVoidPtr) {
    UIntPtr p(uintptr_t(0));
    void* vp = static_cast<void*>(p);
    EXPECT_EQ(vp, nullptr);
}
