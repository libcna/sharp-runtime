// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// Batch 4 tests: ResolveEventHandler, IAsyncResult, AccessViolationException,
// DataMisalignedException, PlatformNotSupportedException, GCGenerationInfo
// (EXCEPT_SIMPLE covers DefaultCtor_WhatNotEmpty/MessageCtor/IsA for the exception types)
#include <any>
#include <gtest/gtest.h>
#include <optional>
#include "System/ResolveEventHandler.hpp"
#include "System/ResolveEventArgs.hpp"
#include "System/IAsyncResult.hpp"
#include "System/AccessViolationException.hpp"
#include "System/DataMisalignedException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/GCGenerationInfo.hpp"
#include "System/SystemException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Threading/EventWaitHandle.hpp"

// ---------------------------------------------------------------------------
// ResolveEventHandler
// ---------------------------------------------------------------------------
TEST(ResolveEventHandlerTests, Callable_ReturnString) {
    System::ResolveEventArgs args("MyAssembly");
    System::ResolveEventHandler h = [](void*, System::ResolveEventArgs& a) -> std::string {
        return "resolved:" + a.getNameProperty();
    };
    EXPECT_EQ(h(nullptr, args), "resolved:MyAssembly");
}
TEST(ResolveEventHandlerTests, NullHandler_IsFalsy) {
    System::ResolveEventHandler h;
    EXPECT_FALSE(static_cast<bool>(h));
}
TEST(ResolveEventHandlerTests, ValidHandler_IsTruthy) {
    System::ResolveEventHandler h = [](void*, System::ResolveEventArgs&) -> std::string { return ""; };
    EXPECT_TRUE(static_cast<bool>(h));
}

// ---------------------------------------------------------------------------
// IAsyncResult
// ---------------------------------------------------------------------------
struct SyncResult : System::IAsyncResult {
    std::any state;
    mutable System::Threading::EventWaitHandle waitHandle{true, System::Threading::EventResetMode::ManualReset};
    bool getIsCompletedProperty()            const noexcept override { return true; }
    bool getCompletedSynchronouslyProperty() const noexcept override { return true; }
    const std::any& getAsyncStateProperty() const override { return state; }
    System::Threading::WaitHandle& getAsyncWaitHandleProperty() const override { return waitHandle; }
};

struct AsyncResult : System::IAsyncResult {
    std::any state;
    mutable System::Threading::EventWaitHandle waitHandle{false, System::Threading::EventResetMode::ManualReset};
    bool getIsCompletedProperty()            const noexcept override { return false; }
    bool getCompletedSynchronouslyProperty() const noexcept override { return false; }
    const std::any& getAsyncStateProperty() const override { return state; }
    System::Threading::WaitHandle& getAsyncWaitHandleProperty() const override { return waitHandle; }
};

TEST(IAsyncResultTests, Completed_ReturnsTrue) {
    SyncResult r;
    EXPECT_TRUE(r.getIsCompletedProperty());
}
TEST(IAsyncResultTests, CompletedSynchronously_ReturnsTrue) {
    SyncResult r;
    EXPECT_TRUE(r.getCompletedSynchronouslyProperty());
}
TEST(IAsyncResultTests, NotCompleted_ReturnsFalse) {
    AsyncResult r;
    EXPECT_FALSE(r.getIsCompletedProperty());
}
TEST(IAsyncResultTests, PolymorphicAccess_ViaBasePtr) {
    SyncResult concrete;
    System::IAsyncResult* base = &concrete;
    EXPECT_TRUE(base->getIsCompletedProperty());
}

// ---------------------------------------------------------------------------
// AccessViolationException (extra; DefaultCtor/MessageCtor/IsA covered by EXCEPT_SIMPLE)
// ---------------------------------------------------------------------------
TEST(AccessViolationExceptionNewTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("segfault"));
    System::AccessViolationException e("bad access", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("bad access"), std::string::npos);
}
TEST(AccessViolationExceptionNewTests, IsA_SystemException) {
    System::AccessViolationException e;
    EXPECT_NE(dynamic_cast<System::SystemException*>(&e), nullptr);
}
TEST(AccessViolationExceptionNewTests, DefaultMsg_ContainsProtected) {
    System::AccessViolationException e;
    EXPECT_NE(std::string(e.what()).find("protected"), std::string::npos);
}

// ---------------------------------------------------------------------------
// DataMisalignedException (extra; DefaultCtor/MessageCtor/IsA covered by EXCEPT_SIMPLE)
// ---------------------------------------------------------------------------
TEST(DataMisalignedExceptionNewTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("hw fault"));
    System::DataMisalignedException e("misaligned load", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("misaligned load"), std::string::npos);
}
TEST(DataMisalignedExceptionNewTests, IsA_SystemException) {
    System::DataMisalignedException e;
    EXPECT_NE(dynamic_cast<System::SystemException*>(&e), nullptr);
}
TEST(DataMisalignedExceptionNewTests, DefaultMsg_ContainsMisalignment) {
    System::DataMisalignedException e;
    EXPECT_NE(std::string(e.what()).find("misalignment"), std::string::npos);
}

// ---------------------------------------------------------------------------
// PlatformNotSupportedException (extra; DefaultCtor/MessageCtor/IsA covered by EXCEPT_SIMPLE)
// ---------------------------------------------------------------------------
TEST(PlatformNotSupportedExceptionNewTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("no impl"));
    System::PlatformNotSupportedException e("unsupported op", inner);
    std::string msg(e.what());
    EXPECT_NE(msg.find("unsupported op"), std::string::npos);
}
TEST(PlatformNotSupportedExceptionNewTests, IsA_NotSupportedException) {
    System::PlatformNotSupportedException e;
    EXPECT_NE(dynamic_cast<System::NotSupportedException*>(&e), nullptr);
}
TEST(PlatformNotSupportedExceptionNewTests, DefaultMsg_ContainsPlatform) {
    System::PlatformNotSupportedException e;
    EXPECT_NE(std::string(e.what()).find("platform"), std::string::npos);
}

// ---------------------------------------------------------------------------
// GCGenerationInfo
// ---------------------------------------------------------------------------
TEST(GCGenerationInfoTests, DefaultCtor_DoesNotThrow) {
    EXPECT_NO_THROW(System::GCGenerationInfo());
}
TEST(GCGenerationInfoTests, SizeBeforeBytes_IsZero) {
    System::GCGenerationInfo gi;
    EXPECT_EQ(gi.getSizeBeforeBytesProperty(), 0LL);
}
TEST(GCGenerationInfoTests, FragmentationBeforeBytes_IsZero) {
    System::GCGenerationInfo gi;
    EXPECT_EQ(gi.getFragmentationBeforeBytesProperty(), 0LL);
}
TEST(GCGenerationInfoTests, SizeAfterBytes_IsZero) {
    System::GCGenerationInfo gi;
    EXPECT_EQ(gi.getSizeAfterBytesProperty(), 0LL);
}
TEST(GCGenerationInfoTests, FragmentationAfterBytes_IsZero) {
    System::GCGenerationInfo gi;
    EXPECT_EQ(gi.getFragmentationAfterBytesProperty(), 0LL);
}

// ---------------------------------------------------------------------------
// #2325 / SR-AUD-123 — the handler can say "unresolved"
// ---------------------------------------------------------------------------

TEST(ResolveEventHandlerTests, Fix2325_TheReturnIsOptionalSoAHandlerCanDecline) {
    // .NET's delegate returns `Assembly?` (`ResolveEventHandler.cs:8`) and null means "I could
    // not resolve this", after which the runtime tries the next handler. This port returned a
    // plain std::string -- a TOTAL function, with no way to decline.
    //
    // The empty string could NOT be borrowed for it: empty already means "absent requesting
    // assembly" elsewhere in ResolveEventArgs, so a documented sentinel would have been
    // unenforceable by the type, which is the defect rather than a repair for it. These three
    // states must therefore be distinguishable, and this row is that assertion.
    System::ResolveEventArgs args("Some.Assembly");

    System::ResolveEventHandler resolves = [](void*, System::ResolveEventArgs&) {
        return std::optional<std::string>("Resolved.Assembly");
    };
    System::ResolveEventHandler declines = [](void*, System::ResolveEventArgs&) {
        return std::optional<std::string>{};
    };
    System::ResolveEventHandler resolvesToEmpty = [](void*, System::ResolveEventArgs&) {
        return std::optional<std::string>("");
    };

    EXPECT_EQ(std::optional<std::string>("Resolved.Assembly"), resolves(nullptr, args));
    EXPECT_EQ(std::nullopt, declines(nullptr, args));
    EXPECT_EQ(std::optional<std::string>(""), resolvesToEmpty(nullptr, args));
    EXPECT_NE(declines(nullptr, args), resolvesToEmpty(nullptr, args))
        << "'unresolved' and 'resolved to an empty name' were indistinguishable before #2325";
}

TEST(ResolveEventHandlerTests, Fix2325_AHandlerThatAlwaysSucceedsNeedsNoEdit) {
    // The change is a WIDENING on the handler side: a lambda returning std::string still binds,
    // because std::string converts implicitly to std::optional<std::string>. Only a CALLER that
    // assigned the result to a std::string breaks, and nothing in this repository calls it --
    // the assembly-resolution surface is still a stub (SR-AUD-103).
    System::ResolveEventArgs args("Some.Assembly");
    System::ResolveEventHandler legacyShape = [](void*, System::ResolveEventArgs& a) -> std::string {
        return a.getNameProperty() + ".dll";
    };
    ASSERT_TRUE(static_cast<bool>(legacyShape));
    EXPECT_EQ(std::optional<std::string>("Some.Assembly.dll"), legacyShape(nullptr, args));

    System::ResolveEventHandler empty;
    EXPECT_FALSE(static_cast<bool>(empty));
}
