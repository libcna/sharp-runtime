// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include "System/NotFiniteNumberException.hpp"

using System::NotFiniteNumberException;

TEST(NotFiniteNumberExceptionTest, DefaultCtor) {
    NotFiniteNumberException e;
    EXPECT_FALSE(std::string(e.what()).empty());
    EXPECT_EQ(e.getOffendingNumberProperty(), 0.0);
}

TEST(NotFiniteNumberExceptionTest, OffendingNumberCtor) {
    NotFiniteNumberException e(std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isinf(e.getOffendingNumberProperty()));
}

TEST(NotFiniteNumberExceptionTest, MessageCtor) {
    NotFiniteNumberException e("not finite");
    EXPECT_NE(std::string(e.what()).find("not finite"), std::string::npos);
    EXPECT_EQ(e.getOffendingNumberProperty(), 0.0);
}

TEST(NotFiniteNumberExceptionTest, MessageAndOffendingNumberCtor) {
    NotFiniteNumberException e("bad value", std::numeric_limits<double>::quiet_NaN());
    EXPECT_NE(std::string(e.what()).find("bad value"), std::string::npos);
    EXPECT_TRUE(std::isnan(e.getOffendingNumberProperty()));
}

TEST(NotFiniteNumberExceptionTest, MessageAndInnerCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    NotFiniteNumberException e("not finite", inner);
    EXPECT_NE(std::string(e.what()).find("not finite"), std::string::npos);
}

TEST(NotFiniteNumberExceptionTest, MessageOffendingAndInnerCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    NotFiniteNumberException e("bad", -std::numeric_limits<double>::infinity(), inner);
    EXPECT_NE(std::string(e.what()).find("bad"), std::string::npos);
    EXPECT_TRUE(std::isinf(e.getOffendingNumberProperty()));
    EXPECT_LT(e.getOffendingNumberProperty(), 0.0);
}

TEST(NotFiniteNumberExceptionTest, IsArithmeticException) {
    NotFiniteNumberException e;
    EXPECT_NO_THROW({ System::ArithmeticException& ref = e; (void)ref; });
}
