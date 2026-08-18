// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/IO/IOException.hpp"
#include "System/SystemException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/NotImplementedException.hpp"
#include "System/Exception.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ObjectDisposedException.hpp"

// Additional tests for Exception, ArgumentNullException, ObjectDisposedException
// (basic coverage already in ExceptionTests.cpp / ExceptionRemainingTests.cpp)

TEST(ExceptionNewTests, CStringCtor_MessageMatches) {
    System::Exception e("hello");
    EXPECT_EQ(e.getMessageProperty(), "hello");
}

// FLIPPED by #2323 (2026-08-18). The empty message was not .NET's; .NET's is
// `_message ?? SR.Format(SR.Exception_WasThrown, GetClassName())` (Exception.cs:61,65).
TEST(ExceptionNewTests, DefaultCtor_MessageIsDotNetsFallback) {
    System::Exception e;
    EXPECT_EQ(e.getMessageProperty(), "Exception of type 'System.Exception' was thrown.");
    EXPECT_STREQ(e.what(), "Exception of type 'System.Exception' was thrown.");
}

TEST(ExceptionNewTests, InnerExceptionPtr_StoredAndRetrievable) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::Exception e("outer", inner);
    EXPECT_NE(e.getInnerExceptionProperty(), nullptr);
}

TEST(ExceptionNewTests, StackTrace_AlwaysEmpty) {
    System::Exception e("x");
    EXPECT_TRUE(e.getStackTraceProperty().empty());
}

TEST(ExceptionNewTests, Data_Modifiable) {
    System::Exception e("e");
    e.getDataProperty()["key"] = "val";
    EXPECT_EQ(e.getDataProperty().at("key"), "val");
}

TEST(ArgumentNullExceptionNewTests, DefaultCtor_IsArgumentException) {
    System::ArgumentNullException e;
    EXPECT_NE(dynamic_cast<System::ArgumentException*>(&e), nullptr);
}

TEST(ArgumentNullExceptionNewTests, ParamNameMessageCtor_StoresMessage) {
    System::ArgumentNullException e("param", "custom message");
    EXPECT_FALSE(std::string(e.what()).empty());
}

TEST(ObjectDisposedExceptionNewTests, ThrowIf_True_Throws) {
    EXPECT_THROW(
        System::ObjectDisposedException::ThrowIf(true, "obj"),
        System::ObjectDisposedException
    );
}

TEST(ObjectDisposedExceptionNewTests, ThrowIf_False_NoThrow) {
    EXPECT_NO_THROW(
        System::ObjectDisposedException::ThrowIf(false, "obj")
    );
}

TEST(ObjectDisposedExceptionNewTests, ObjectName_Stored) {
    System::ObjectDisposedException e("myResource");
    EXPECT_EQ(e.getObjectNameProperty(), "myResource");
}

// ===========================================================================
// #2323 — the base fallback, and the guard that stops it spreading
// ===========================================================================
//
// .NET's Exception.Message is `_message ?? SR.Format(SR.Exception_WasThrown, GetClassName())`
// (Exception.cs:61,65), i.e. "Exception of type '{0}' was thrown." (Strings.resx:2333) with {0}
// the RUNTIME TYPE NAME. {0} is reflection, which this port permanently does not have, so it is
// resolved STATICALLY at each site by the one entity that knows the answer: the type itself.
//
// THE REVIEW'S PREMISE MEASUREMENT WAS WRONG, and that is why this guard exists. It recorded
// that "18 subclasses supply a NON-EMPTY default message and exactly ONE type does not --
// System::Exception itself", so the blast radius was "the base type constructed directly".
// Re-measured over all 103 subclasses: THREE reach the base fallback, not one. `= default` on a
// derived exception default-constructs its base, and two types spell it that way.
//
// The cost of the static resolution is that a FUTURE subclass written as `= default` would
// silently report `System.Exception` -- a message naming the wrong type, which is a lie where an
// empty one was merely an absence. That is what this test is for.
TEST(ExceptionFallbackMessageTests, TheBaseNamesItself) {
    EXPECT_EQ(System::Exception{}.getMessageProperty(),
              "Exception of type 'System.Exception' was thrown.");
    // The other two types that reach this fallback live in modules that Core.Base does not
    // depend on, so their rows are in their own suites -- modules/net-http and modules/text-json.
    // A refactor is not a reason to add a public component edge (#2354's rule), and neither is a
    // test.
}

TEST(ExceptionFallbackMessageTests, NoOtherExceptionInheritsTheBaseFallback) {
    // The guard. Every one of these has its own default message, so none may report the base's
    // string; if one starts to, a subclass has been added or changed to reach the fallback and
    // must be given its own name in the same change.
    const std::string baseFallback = "Exception of type 'System.Exception' was thrown.";
    const auto notTheBase = [&baseFallback](const std::string& message, const char* what) {
        EXPECT_NE(message, baseFallback) << what << " now inherits the base fallback";
        EXPECT_FALSE(message.empty()) << what << " has no default message at all";
    };
    notTheBase(System::SystemException{}.getMessageProperty(), "SystemException");
    notTheBase(System::ArgumentException{}.getMessageProperty(), "ArgumentException");
    notTheBase(System::ArgumentNullException{}.getMessageProperty(), "ArgumentNullException");
    notTheBase(System::InvalidOperationException{}.getMessageProperty(), "InvalidOperationException");
    notTheBase(System::NotSupportedException{}.getMessageProperty(), "NotSupportedException");
    notTheBase(System::NotImplementedException{}.getMessageProperty(), "NotImplementedException");
    notTheBase(System::FormatException{}.getMessageProperty(), "FormatException");
    notTheBase(System::IndexOutOfRangeException{}.getMessageProperty(), "IndexOutOfRangeException");
    notTheBase(System::IO::IOException{}.getMessageProperty(), "IOException");
}

TEST(ExceptionFallbackMessageTests, AnExplicitlyEmptyMessageStaysEmpty) {
    // .NET's fallback fires on a NULL message, not an empty one: `new Exception("")` has
    // `Message == ""`. This port cannot distinguish null from empty in a std::string, and does
    // not need to -- the fallback lives in the constructor that was given no message at all.
    EXPECT_TRUE(System::Exception("").getMessageProperty().empty());
}
