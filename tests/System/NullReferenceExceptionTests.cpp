// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/NullReferenceException.hpp"

using System::NullReferenceException;

TEST(NullReferenceExceptionTest, DefaultCtor) {
    NullReferenceException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(NullReferenceExceptionTest, CStringCtor) {
    NullReferenceException e("null ref");
    EXPECT_NE(std::string(e.what()).find("null ref"), std::string::npos);
}

TEST(NullReferenceExceptionTest, StringCtor) {
    NullReferenceException e(std::string("null ref"));
    EXPECT_NE(std::string(e.what()).find("null ref"), std::string::npos);
}

TEST(NullReferenceExceptionTest, InnerExceptionCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    NullReferenceException e("null ref", inner);
    EXPECT_NE(std::string(e.what()).find("null ref"), std::string::npos);
}

TEST(NullReferenceExceptionTest, IsSystemException) {
    NullReferenceException e;
    EXPECT_NO_THROW({ System::SystemException& ref = e; (void)ref; });
}
