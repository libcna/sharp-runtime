// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/OutOfMemoryException.hpp"

using System::OutOfMemoryException;

TEST(OutOfMemoryExceptionTest, DefaultCtor) {
    OutOfMemoryException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(OutOfMemoryExceptionTest, CStringCtor) {
    OutOfMemoryException e("out of memory");
    EXPECT_NE(std::string(e.what()).find("out of memory"), std::string::npos);
}

TEST(OutOfMemoryExceptionTest, StringCtor) {
    OutOfMemoryException e(std::string("oom"));
    EXPECT_NE(std::string(e.what()).find("oom"), std::string::npos);
}

TEST(OutOfMemoryExceptionTest, InnerExceptionCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    OutOfMemoryException e("out of memory", inner);
    EXPECT_NE(std::string(e.what()).find("out of memory"), std::string::npos);
}

TEST(OutOfMemoryExceptionTest, IsSystemException) {
    OutOfMemoryException e;
    EXPECT_NO_THROW({ System::SystemException& ref = e; (void)ref; });
}
