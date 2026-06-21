// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/OperationCanceledException.hpp"
#include "System/SystemException.hpp"

using System::OperationCanceledException;

TEST(OperationCanceledExceptionTests, DefaultCtor_WhatContainsCanceled) {
    OperationCanceledException e;
    EXPECT_NE(std::string(e.what()).find("canceled"), std::string::npos);
}

TEST(OperationCanceledExceptionTests, DefaultCtor_IsSystemException) {
    OperationCanceledException e;
    EXPECT_NE(dynamic_cast<System::SystemException*>(&e), nullptr);
}

TEST(OperationCanceledExceptionTests, CStringCtor_MessageStored) {
    OperationCanceledException e("task was aborted");
    EXPECT_EQ(std::string(e.what()), "task was aborted");
}

TEST(OperationCanceledExceptionTests, StringCtor_MessageStored) {
    OperationCanceledException e(std::string("operation stopped"));
    EXPECT_EQ(std::string(e.what()), "operation stopped");
}

TEST(OperationCanceledExceptionTests, InnerExceptionCtor_ContainsBothMessages) {
    std::runtime_error inner("timeout");
    OperationCanceledException e("task canceled", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("task canceled"), std::string::npos);
    EXPECT_NE(msg.find("timeout"), std::string::npos);
}

TEST(OperationCanceledExceptionTests, Catchable_AsStdException) {
    EXPECT_THROW(
        { throw OperationCanceledException(); },
        std::exception
    );
}

TEST(OperationCanceledExceptionTests, Catchable_AsOperationCanceledException) {
    EXPECT_THROW(
        { throw OperationCanceledException("x"); },
        OperationCanceledException
    );
}

TEST(OperationCanceledExceptionTests, DefaultMsg_NotEmpty) {
    OperationCanceledException e;
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(OperationCanceledExceptionTests, InnerCtor_WhatNotEmpty) {
    std::runtime_error inner("inner");
    OperationCanceledException e("outer", inner);
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(OperationCanceledExceptionTests, GetMessage_MatchesWhat) {
    OperationCanceledException e("match me");
    EXPECT_EQ(e.getMessageProperty(), std::string(e.what()));
}
