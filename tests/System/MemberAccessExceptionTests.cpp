// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/MemberAccessException.hpp"

using System::MemberAccessException;

TEST(MemberAccessExceptionTest, DefaultCtor) {
    MemberAccessException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(MemberAccessExceptionTest, MessageCtor) {
    MemberAccessException e("access denied");
    EXPECT_NE(std::string(e.what()).find("access denied"), std::string::npos);
}

TEST(MemberAccessExceptionTest, InnerExceptionCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    MemberAccessException e("access denied", inner);
    EXPECT_NE(std::string(e.what()).find("access denied"), std::string::npos);
}

TEST(MemberAccessExceptionTest, IsSystemException) {
    MemberAccessException e("test");
    EXPECT_NO_THROW({ System::SystemException& ref = e; (void)ref; });
}
