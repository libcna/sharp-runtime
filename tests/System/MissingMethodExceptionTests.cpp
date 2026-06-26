// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/MissingMethodException.hpp"
#include "System/MissingMemberException.hpp"

using System::MissingMethodException;

TEST(MissingMethodExceptionTests, DefaultCtor_WhatNotEmpty_New) {
    MissingMethodException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(MissingMethodExceptionTests, DefaultCtor_WhatContainsMethod) {
    MissingMethodException e;
    EXPECT_NE(std::string(e.what()).find("method"), std::string::npos);
}

TEST(MissingMethodExceptionTests, DefaultCtor_IsAMissingMemberException) {
    MissingMethodException e;
    EXPECT_NE(dynamic_cast<System::MissingMemberException*>(&e), nullptr);
}

TEST(MissingMethodExceptionTests, StringCtor_MessageStored) {
    MissingMethodException e("no such method");
    EXPECT_EQ(std::string(e.what()), "no such method");
}

TEST(MissingMethodExceptionTests, ClassMethodCtor_ContainsClassName) {
    MissingMethodException e("MyClass", "MyMethod");
    EXPECT_NE(std::string(e.what()).find("MyClass"), std::string::npos);
}

TEST(MissingMethodExceptionTests, ClassMethodCtor_ContainsMethodName) {
    MissingMethodException e("MyClass", "MyMethod");
    EXPECT_NE(std::string(e.what()).find("MyMethod"), std::string::npos);
}

TEST(MissingMethodExceptionTests, ClassMethodCtor_ContainsDot) {
    MissingMethodException e("A", "B");
    EXPECT_NE(std::string(e.what()).find("A.B"), std::string::npos);
}

TEST(MissingMethodExceptionTests, InnerExceptionCtor_ContainsBothMessages) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    MissingMethodException e("method gone", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("method gone"), std::string::npos);
}

TEST(MissingMethodExceptionTests, Catchable_AsStdException) {
    EXPECT_THROW({ throw MissingMethodException(); }, std::exception);
}

TEST(MissingMethodExceptionTests, Catchable_AsMissingMemberException) {
    EXPECT_THROW({ throw MissingMethodException(); }, System::MissingMemberException);
}
