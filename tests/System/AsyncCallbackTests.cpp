// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/AsyncCallback.hpp"
#include "System/IAsyncResult.hpp"

using System::AsyncCallback;
using System::IAsyncResult;

namespace {

struct FakeAsyncResult : public IAsyncResult {
    [[nodiscard]] bool getIsCompletedProperty() const override { return true; }
    [[nodiscard]] bool getCompletedSynchronouslyProperty() const override { return true; }
};

} // namespace

TEST(AsyncCallbackTests, DefaultConstructed_IsEmpty) {
    AsyncCallback cb;
    EXPECT_FALSE(static_cast<bool>(cb));
}

TEST(AsyncCallbackTests, AssignedLambda_IsCallable) {
    AsyncCallback cb = [](IAsyncResult&) {};
    EXPECT_TRUE(static_cast<bool>(cb));
}

TEST(AsyncCallbackTests, Invoke_LambdaCalled) {
    bool called = false;
    AsyncCallback cb = [&](IAsyncResult&) { called = true; };
    FakeAsyncResult ar;
    cb(ar);
    EXPECT_TRUE(called);
}

TEST(AsyncCallbackTests, Invoke_ReceivesCorrectArg) {
    bool completed = false;
    AsyncCallback cb = [&](IAsyncResult& ar) {
        completed = ar.getIsCompletedProperty();
    };
    FakeAsyncResult ar;
    cb(ar);
    EXPECT_TRUE(completed);
}
