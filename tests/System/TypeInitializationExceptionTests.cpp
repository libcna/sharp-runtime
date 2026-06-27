// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/TypeInitializationException.hpp"

using System::TypeInitializationException;

TEST(TypeInitializationExceptionTest, CtorWithTypeName) {
    TypeInitializationException e("My.Namespace.MyType", nullptr);
    EXPECT_NE(std::string(e.what()).find("MyType"), std::string::npos);
}

TEST(TypeInitializationExceptionTest, TypeNameProperty) {
    TypeInitializationException e("System.MyClass", nullptr);
    EXPECT_EQ(e.getTypeNameProperty(), "System.MyClass");
}

TEST(TypeInitializationExceptionTest, MessageContainsTypeName) {
    TypeInitializationException e("Foo.Bar", nullptr);
    std::string msg = e.what();
    EXPECT_NE(msg.find("Foo.Bar"), std::string::npos);
}

TEST(TypeInitializationExceptionTest, WithInnerException) {
    std::runtime_error inner("inner cause");
    TypeInitializationException e("MyType", &inner);
    std::string msg = e.what();
    EXPECT_NE(msg.find("inner cause"), std::string::npos);
}

TEST(TypeInitializationExceptionTest, IsSystemException) {
    TypeInitializationException e("T", nullptr);
    EXPECT_NO_THROW({ System::SystemException& ref = e; (void)ref; });
}

TEST(TypeInitializationExceptionTest, IsException) {
    TypeInitializationException e("T", nullptr);
    EXPECT_NO_THROW({ System::Exception& ref = e; (void)ref; });
}

TEST(TypeInitializationExceptionTest, Throwable) {
    EXPECT_THROW({ throw TypeInitializationException("T", nullptr); }, TypeInitializationException);
}
