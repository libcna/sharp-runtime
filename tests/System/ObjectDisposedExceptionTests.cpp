// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ObjectDisposedException.hpp"

using System::ObjectDisposedException;

TEST(ObjectDisposedExceptionTest, DefaultCtor) {
    ObjectDisposedException e;
    EXPECT_FALSE(std::string(e.what()).empty());
    EXPECT_TRUE(e.getObjectNameProperty().empty());
}

TEST(ObjectDisposedExceptionTest, ObjectNameCtor) {
    ObjectDisposedException e("MyStream");
    EXPECT_NE(std::string(e.what()).find("MyStream"), std::string::npos);
    EXPECT_EQ(e.getObjectNameProperty(), "MyStream");
}

TEST(ObjectDisposedExceptionTest, ObjectNameAndMessageCtor) {
    ObjectDisposedException e("MyStream", "Stream is closed");
    std::string msg(e.what());
    EXPECT_NE(msg.find("Stream is closed"), std::string::npos);
    EXPECT_EQ(e.getObjectNameProperty(), "MyStream");
}

TEST(ObjectDisposedExceptionTest, MessageAndInnerExceptionCtor) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    ObjectDisposedException e("object disposed", inner);
    EXPECT_NE(std::string(e.what()).find("object disposed"), std::string::npos);
}

TEST(ObjectDisposedExceptionTest, ThrowIf_True) {
    EXPECT_THROW(ObjectDisposedException::ThrowIf(true, "obj"), ObjectDisposedException);
}

TEST(ObjectDisposedExceptionTest, ThrowIf_False) {
    EXPECT_NO_THROW(ObjectDisposedException::ThrowIf(false, "obj"));
}

TEST(ObjectDisposedExceptionTest, IsInvalidOperationException) {
    ObjectDisposedException e("obj");
    EXPECT_NO_THROW({ System::InvalidOperationException& ref = e; (void)ref; });
}
