// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UnauthorizedAccessException.hpp"

using System::UnauthorizedAccessException;

TEST(UnauthorizedAccessExceptionTest, DefaultCtor) {
    UnauthorizedAccessException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(UnauthorizedAccessExceptionTest, CStringCtor) {
    UnauthorizedAccessException e("access denied to file");
    EXPECT_NE(std::string(e.what()).find("access denied"), std::string::npos);
}

TEST(UnauthorizedAccessExceptionTest, StringCtor) {
    UnauthorizedAccessException e(std::string("no permission"));
    EXPECT_NE(std::string(e.what()).find("no permission"), std::string::npos);
}

TEST(UnauthorizedAccessExceptionTest, IsSystemException) {
    UnauthorizedAccessException e;
    EXPECT_NO_THROW({ System::SystemException& ref = e; (void)ref; });
}

TEST(UnauthorizedAccessExceptionTest, IsException) {
    UnauthorizedAccessException e;
    EXPECT_NO_THROW({ System::Exception& ref = e; (void)ref; });
}

TEST(UnauthorizedAccessExceptionTest, Throwable) {
    EXPECT_THROW({ throw UnauthorizedAccessException("denied"); }, UnauthorizedAccessException);
}

TEST(UnauthorizedAccessExceptionTest, CaughtAsStdException) {
    bool caught = false;
    try { throw UnauthorizedAccessException("denied"); }
    catch (const std::exception&) { caught = true; }
    EXPECT_TRUE(caught);
}
