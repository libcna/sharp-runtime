// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/MissingMemberException.hpp"

using System::MissingMemberException;

TEST(MissingMemberExceptionTests, DefaultCtor_WhatNotEmpty_New) {
    MissingMemberException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(MissingMemberExceptionTests, DefaultCtor_WhatContainsMember) {
    MissingMemberException e;
    EXPECT_NE(std::string(e.what()).find("member"), std::string::npos);
}

TEST(MissingMemberExceptionTests, StringCtor_MessageStored_New) {
    MissingMemberException e("custom message");
    EXPECT_EQ(std::string(e.what()), "custom message");
}

TEST(MissingMemberExceptionTests, ClassMemberCtor_ContainsClassName) {
    MissingMemberException e("Foo", "Bar");
    EXPECT_NE(std::string(e.what()).find("Foo"), std::string::npos);
}

TEST(MissingMemberExceptionTests, ClassMemberCtor_ContainsMemberName) {
    MissingMemberException e("Foo", "Bar");
    EXPECT_NE(std::string(e.what()).find("Bar"), std::string::npos);
}

TEST(MissingMemberExceptionTests, ClassMemberCtor_ContainsDot) {
    MissingMemberException e("A", "B");
    EXPECT_NE(std::string(e.what()).find("A.B"), std::string::npos);
}

TEST(MissingMemberExceptionTests, InnerExceptionCtor_ContainsBothMessages) {
    std::runtime_error inner("inner cause");
    MissingMemberException e("outer msg", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("outer msg"), std::string::npos);
    EXPECT_NE(msg.find("inner cause"), std::string::npos);
}

TEST(MissingMemberExceptionTests, Catchable_AsStdException) {
    EXPECT_THROW({ throw MissingMemberException(); }, std::exception);
}

TEST(MissingMemberExceptionTests, Catchable_AsMissingMemberException) {
    EXPECT_THROW({ throw MissingMemberException("x"); }, MissingMemberException);
}
