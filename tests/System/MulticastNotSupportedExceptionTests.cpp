// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/MulticastNotSupportedException.hpp"

using System::MulticastNotSupportedException;

TEST(MulticastNotSupportedExceptionTest, DefaultCtor) {
    MulticastNotSupportedException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(MulticastNotSupportedExceptionTest, MessageCtor) {
    MulticastNotSupportedException e("no multicast");
    EXPECT_NE(std::string(e.what()).find("no multicast"), std::string::npos);
}

TEST(MulticastNotSupportedExceptionTest, InnerExceptionCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    MulticastNotSupportedException e("no multicast", inner);
    EXPECT_NE(std::string(e.what()).find("no multicast"), std::string::npos);
}

TEST(MulticastNotSupportedExceptionTest, IsException) {
    MulticastNotSupportedException e("test");
    EXPECT_NO_THROW({ System::Exception& ref = e; (void)ref; });
}
