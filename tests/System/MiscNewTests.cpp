// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// Tests for ParamArrayAttribute, AsyncCallback, ICustomFormatter
#include <gtest/gtest.h>
#include "System/ParamArrayAttribute.hpp"
#include "System/AsyncCallback.hpp"
#include "System/IAsyncResult.hpp"

// ---------------------------------------------------------------------------
// ParamArrayAttribute
// ---------------------------------------------------------------------------
TEST(ParamArrayAttributeTests, DefaultCtor_DoesNotThrow) {
    EXPECT_NO_THROW(System::ParamArrayAttribute attr);
}
TEST(ParamArrayAttributeTests, IsA_Attribute) {
    System::ParamArrayAttribute attr;
    EXPECT_NE(dynamic_cast<System::Attribute*>(&attr), nullptr);
}

// ---------------------------------------------------------------------------
// AsyncCallback
// ---------------------------------------------------------------------------
struct DummyAsyncResult : System::IAsyncResult {
    bool getIsCompletedProperty() const noexcept override { return true; }
    bool getCompletedSynchronouslyProperty() const noexcept override { return false; }
};

TEST(AsyncCallbackTests, Callable_InvokesAction) {
    bool called = false;
    System::AsyncCallback cb = [&](System::IAsyncResult&) { called = true; };
    DummyAsyncResult ar;
    cb(ar);
    EXPECT_TRUE(called);
}

TEST(AsyncCallbackTests, NullCallback_IsFalsy) {
    System::AsyncCallback cb;
    EXPECT_FALSE(static_cast<bool>(cb));
}

TEST(AsyncCallbackTests, ValidCallback_IsTruthy) {
    System::AsyncCallback cb = [](System::IAsyncResult&) {};
    EXPECT_TRUE(static_cast<bool>(cb));
}

TEST(AsyncCallbackTests, ReceivesCorrectResult) {
    DummyAsyncResult ar;
    bool completed = false;
    System::AsyncCallback cb = [&](System::IAsyncResult& r) {
        completed = r.getIsCompletedProperty();
    };
    cb(ar);
    EXPECT_TRUE(completed);
}
