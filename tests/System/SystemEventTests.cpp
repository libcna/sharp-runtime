// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for event-related types: EventArgs, AssemblyLoadEventArgs,
// ResolveEventArgs, UnhandledExceptionEventArgs.
#include <gtest/gtest.h>
#include <stdexcept>
#include "System/EventArgs.hpp"
#include "System/AssemblyLoadEventArgs.hpp"
#include "System/ResolveEventArgs.hpp"
#include "System/UnhandledExceptionEventArgs.hpp"

using System::EventArgs;
using System::AssemblyLoadEventArgs;
using System::ResolveEventArgs;
using System::UnhandledExceptionEventArgs;

// ===========================================================================
// EventArgs
// ===========================================================================

TEST(EventArgsTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(EventArgs{});
}

TEST(EventArgsTests, Empty_IsSameInstanceOnEveryAccess) {
    const EventArgs& a = EventArgs::Empty;
    const EventArgs& b = EventArgs::Empty;
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// AssemblyLoadEventArgs
// ===========================================================================

TEST(AssemblyLoadEventArgsTests, Constructor_StoresName) {
    AssemblyLoadEventArgs args("MyAssembly");
    EXPECT_EQ(args.getLoadedAssemblyProperty(), "MyAssembly");
}

TEST(AssemblyLoadEventArgsTests, IsA_EventArgs) {
    AssemblyLoadEventArgs args("X");
    EventArgs& base = args;
    (void)base;
    SUCCEED();
}

// ===========================================================================
// ResolveEventArgs
// ===========================================================================

TEST(ResolveEventArgsTests, SingleArgCtor_StoresName) {
    ResolveEventArgs args("System.Foo");
    EXPECT_EQ(args.getNameProperty(), "System.Foo");
    EXPECT_TRUE(args.getRequestingAssemblyNameProperty().empty());
}

TEST(ResolveEventArgsTests, TwoArgCtor_StoresBoth) {
    ResolveEventArgs args("System.Foo", "MyApp");
    EXPECT_EQ(args.getNameProperty(), "System.Foo");
    EXPECT_EQ(args.getRequestingAssemblyNameProperty(), "MyApp");
}

// ===========================================================================
// UnhandledExceptionEventArgs
// ===========================================================================

TEST(UnhandledExceptionEventArgsTests, IsTerminating_True) {
    auto ex = std::make_exception_ptr(std::runtime_error("boom"));
    UnhandledExceptionEventArgs args(ex, true);
    EXPECT_TRUE(args.getIsTerminatingProperty());
}

TEST(UnhandledExceptionEventArgsTests, IsTerminating_False) {
    UnhandledExceptionEventArgs args(nullptr, false);
    EXPECT_FALSE(args.getIsTerminatingProperty());
}

TEST(UnhandledExceptionEventArgsTests, ExceptionObject_Stored) {
    auto ex = std::make_exception_ptr(std::runtime_error("test"));
    UnhandledExceptionEventArgs args(ex, false);
    EXPECT_NE(args.getExceptionObjectProperty(), nullptr);
}

TEST(UnhandledExceptionEventArgsTests, NullException_Stored) {
    UnhandledExceptionEventArgs args(nullptr, false);
    EXPECT_EQ(args.getExceptionObjectProperty(), nullptr);
}
