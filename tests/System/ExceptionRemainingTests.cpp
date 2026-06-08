// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Batch tests for remaining exception types in System::, System::Globalization::,
// plus richer exceptions: AggregateException, NotFiniteNumberException,
// TypeInitializationException, CultureNotFoundException.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "System/Exception.hpp"
#include "System/AccessViolationException.hpp"
#include "System/AggregateException.hpp"
#include "System/AppDomainUnloadedException.hpp"
#include "System/ApplicationException.hpp"
#include "System/ArrayTypeMismatchException.hpp"
#include "System/BadImageFormatException.hpp"
#include "System/CannotUnloadAppDomainException.hpp"
#include "System/DataMisalignedException.hpp"
#include "System/DivideByZeroException.hpp"
#include "System/DllNotFoundException.hpp"
#include "System/DuplicateWaitObjectException.hpp"
#include "System/EntryPointNotFoundException.hpp"
#include "System/ExecutionEngineException.hpp"
#include "System/FieldAccessException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/InsufficientExecutionStackException.hpp"
#include "System/InsufficientMemoryException.hpp"
#include "System/InvalidCastException.hpp"
#include "System/InvalidProgramException.hpp"
#include "System/InvalidTimeZoneException.hpp"
#include "System/MemberAccessException.hpp"
#include "System/MethodAccessException.hpp"
#include "System/MissingFieldException.hpp"
#include "System/MissingMemberException.hpp"
#include "System/MissingMethodException.hpp"
#include "System/MulticastNotSupportedException.hpp"
#include "System/NotFiniteNumberException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/OutOfMemoryException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/RankException.hpp"
#include "System/StackOverflowException.hpp"
#include "System/TimeoutException.hpp"
#include "System/TimeZoneNotFoundException.hpp"
#include "System/TypeAccessException.hpp"
#include "System/TypeInitializationException.hpp"
#include "System/TypeLoadException.hpp"
#include "System/TypeUnloadedException.hpp"
#include "System/UnauthorizedAccessException.hpp"
#include "System/Globalization/CultureNotFoundException.hpp"

using System::Exception;

// Helper macro: default ctor produces non-empty what()
#define EXCEPT_DEFAULT_NONEMPTY(ExType) \
    TEST(ExType##Tests, DefaultCtor_WhatNotEmpty) { \
        System::ExType ex; \
        EXPECT_FALSE(std::string(ex.what()).empty()); \
    }

// Helper macro: message ctor stores message in what()
#define EXCEPT_MSG_CTOR(ExType) \
    TEST(ExType##Tests, MessageCtor_WhatContainsMessage) { \
        System::ExType ex("test message"); \
        EXPECT_NE(std::string(ex.what()).find("test message"), std::string::npos); \
    }

// Helper macro: is catchable as System::Exception
#define EXCEPT_IS_EXCEPTION(ExType) \
    TEST(ExType##Tests, IsA_Exception) { \
        EXPECT_THROW(throw System::ExType(), System::Exception); \
    }

// Expand all three for each simple type
#define EXCEPT_SIMPLE(ExType) \
    EXCEPT_DEFAULT_NONEMPTY(ExType) \
    EXCEPT_MSG_CTOR(ExType) \
    EXCEPT_IS_EXCEPTION(ExType)

EXCEPT_SIMPLE(AccessViolationException)
EXCEPT_SIMPLE(AppDomainUnloadedException)
EXCEPT_SIMPLE(ApplicationException)
EXCEPT_SIMPLE(ArrayTypeMismatchException)
EXCEPT_SIMPLE(BadImageFormatException)
EXCEPT_SIMPLE(CannotUnloadAppDomainException)
EXCEPT_SIMPLE(DataMisalignedException)
EXCEPT_SIMPLE(DivideByZeroException)
EXCEPT_SIMPLE(DllNotFoundException)
EXCEPT_SIMPLE(DuplicateWaitObjectException)
EXCEPT_SIMPLE(EntryPointNotFoundException)
EXCEPT_SIMPLE(ExecutionEngineException)
EXCEPT_SIMPLE(FieldAccessException)
EXCEPT_SIMPLE(IndexOutOfRangeException)
EXCEPT_SIMPLE(InsufficientExecutionStackException)
EXCEPT_SIMPLE(InsufficientMemoryException)
EXCEPT_SIMPLE(InvalidCastException)
EXCEPT_SIMPLE(InvalidProgramException)
EXCEPT_SIMPLE(InvalidTimeZoneException)
EXCEPT_SIMPLE(MemberAccessException)
EXCEPT_SIMPLE(MethodAccessException)
EXCEPT_SIMPLE(MissingFieldException)
EXCEPT_SIMPLE(MissingMemberException)
EXCEPT_SIMPLE(MissingMethodException)
EXCEPT_SIMPLE(MulticastNotSupportedException)
EXCEPT_SIMPLE(OperationCanceledException)
EXCEPT_SIMPLE(OutOfMemoryException)
EXCEPT_SIMPLE(PlatformNotSupportedException)
EXCEPT_SIMPLE(RankException)
EXCEPT_SIMPLE(StackOverflowException)
EXCEPT_SIMPLE(TimeoutException)
EXCEPT_SIMPLE(TimeZoneNotFoundException)
EXCEPT_SIMPLE(TypeAccessException)
EXCEPT_SIMPLE(TypeLoadException)
EXCEPT_SIMPLE(TypeUnloadedException)
EXCEPT_SIMPLE(UnauthorizedAccessException)

// ===========================================================================
// AggregateException — richer API
// ===========================================================================

TEST(AggregateExceptionTests, DefaultCtor_WhatNotEmpty) {
    System::AggregateException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}
TEST(AggregateExceptionTests, IsA_Exception) {
    EXPECT_THROW(throw System::AggregateException(), System::Exception);
}
TEST(AggregateExceptionTests, WithInnerExceptions_CountMatches) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("e1"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("e2"));
    System::AggregateException ex({ep1, ep2});
    EXPECT_EQ(ex.getInnerExceptionCountProperty(), 2u);
}
TEST(AggregateExceptionTests, WithInnerExceptions_WhatContainsMessages) {
    auto ep = std::make_exception_ptr(std::runtime_error("inner msg"));
    System::AggregateException ex({ep});
    EXPECT_NE(std::string(ex.what()).find("inner msg"), std::string::npos);
}
TEST(AggregateExceptionTests, Unwrap_SingleInner_ReturnsInner) {
    auto ep = std::make_exception_ptr(std::runtime_error("only one"));
    System::AggregateException ex({ep});
    auto unwrapped = ex.Unwrap();
    EXPECT_TRUE(unwrapped != nullptr);
}
TEST(AggregateExceptionTests, Handle_AllHandled_NoRethrow) {
    auto ep = std::make_exception_ptr(std::runtime_error("handled"));
    System::AggregateException ex({ep});
    EXPECT_NO_THROW(ex.Handle([](std::exception_ptr) { return true; }));
}
TEST(AggregateExceptionTests, Handle_SomeUnhandled_Rethrows) {
    auto ep = std::make_exception_ptr(std::runtime_error("not handled"));
    System::AggregateException ex({ep});
    EXPECT_THROW(ex.Handle([](std::exception_ptr) { return false; }),
                 System::AggregateException);
}

// ===========================================================================
// NotFiniteNumberException — has offendingNumber property
// ===========================================================================

TEST(NotFiniteNumberExceptionTests, DefaultCtor_WhatNotEmpty) {
    System::NotFiniteNumberException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}
TEST(NotFiniteNumberExceptionTests, OffendingNumberCtor_StoresValue) {
    System::NotFiniteNumberException ex(1.0 / 0.0);
    EXPECT_TRUE(std::isinf(ex.getOffendingNumberProperty()));
}
TEST(NotFiniteNumberExceptionTests, MessageAndNumber_Ctor) {
    System::NotFiniteNumberException ex("bad number", 999.0);
    EXPECT_NE(std::string(ex.what()).find("bad number"), std::string::npos);
    EXPECT_DOUBLE_EQ(ex.getOffendingNumberProperty(), 999.0);
}
TEST(NotFiniteNumberExceptionTests, IsA_Exception) {
    EXPECT_THROW(throw System::NotFiniteNumberException(), System::Exception);
}

// ===========================================================================
// TypeInitializationException — has typeName property
// ===========================================================================

TEST(TypeInitializationExceptionTests, Constructor_StoresTypeName) {
    System::TypeInitializationException ex("MyClass", nullptr);
    EXPECT_EQ(ex.getTypeNameProperty(), "MyClass");
}
TEST(TypeInitializationExceptionTests, WhatContainsTypeName) {
    System::TypeInitializationException ex("Foo.Bar", nullptr);
    EXPECT_NE(std::string(ex.what()).find("Foo.Bar"), std::string::npos);
}
TEST(TypeInitializationExceptionTests, IsA_Exception) {
    EXPECT_THROW((throw System::TypeInitializationException("T", nullptr)), System::Exception);
}

// ===========================================================================
// CultureNotFoundException — has invalidCultureName property
// ===========================================================================

TEST(CultureNotFoundExceptionTests, DefaultCtor_WhatNotEmpty) {
    System::Globalization::CultureNotFoundException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}
TEST(CultureNotFoundExceptionTests, MessageAndName_StoreName) {
    System::Globalization::CultureNotFoundException ex("bad culture", "xx-XX");
    EXPECT_EQ(ex.getInvalidCultureNameProperty(), "xx-XX");
}
TEST(CultureNotFoundExceptionTests, IsA_Exception) {
    EXPECT_THROW(throw System::Globalization::CultureNotFoundException(), System::Exception);
}
