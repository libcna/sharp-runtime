// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "System/Exception.hpp"
#include "System/SystemException.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArithmeticException.hpp"
#include "System/OverflowException.hpp"
#include "System/FormatException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NotImplementedException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/NullReferenceException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/ArrayTypeMismatchException.hpp"
#include "System/FieldAccessException.hpp"
#include "System/InsufficientMemoryException.hpp"
#include "System/OutOfMemoryException.hpp"
#include "System/InvalidTimeZoneException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/ContextMarshalException.hpp"

using System::Exception;
using System::SystemException;
using System::ArgumentException;
using System::ArgumentNullException;
using System::ArgumentOutOfRangeException;
using System::ArithmeticException;
using System::OverflowException;
using System::FormatException;
using System::InvalidOperationException;
using System::NotImplementedException;
using System::NotSupportedException;
using System::NullReferenceException;
using System::ObjectDisposedException;

// ---------------------------------------------------------------------------
// Exception (base)
// ---------------------------------------------------------------------------

TEST(ExceptionTests, DefaultCtorEmptyMessage) {
    Exception e;
    EXPECT_TRUE(e.getMessageProperty().empty() || e.getMessageProperty() == "");
}

TEST(ExceptionTests, MessageFromCString) {
    Exception e("hello");
    EXPECT_EQ(e.getMessageProperty(), "hello");
}

TEST(ExceptionTests, MessageFromStdString) {
    Exception e(std::string("world"));
    EXPECT_EQ(e.getMessageProperty(), "world");
}

TEST(ExceptionTests, WhatMatchesMessage) {
    Exception e("test message");
    EXPECT_EQ(std::string(e.what()), "test message");
}

TEST(ExceptionTests, IsStdException) {
    // Exception must be catchable as std::exception
    bool caught = false;
    try { throw Exception("oops"); }
    catch (const std::exception& ex) { caught = true; EXPECT_NE(std::string(ex.what()), ""); }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// SystemException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, SystemExceptionMessage) {
    SystemException e("sys error");
    EXPECT_EQ(e.getMessageProperty(), "sys error");
}

TEST(ExceptionTests, SystemExceptionCatchableAsException) {
    bool caught = false;
    try { throw SystemException("x"); }
    catch (const Exception&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// ArgumentException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, ArgumentExceptionMessage) {
    ArgumentException e("bad arg");
    EXPECT_EQ(e.getMessageProperty(), "bad arg");
}

TEST(ExceptionTests, ArgumentExceptionWithParamName) {
    ArgumentException e("bad arg", "param1");
    EXPECT_NE(e.getMessageProperty().find("bad arg"), std::string::npos);
}

TEST(ExceptionTests, ArgumentExceptionCatchableAsSystemException) {
    bool caught = false;
    try { throw ArgumentException("arg"); }
    catch (const SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(ExceptionTests, ArgumentException_ParamName_Stored) {
    ArgumentException e("bad value", "myParam");
    EXPECT_EQ(e.getParamNameProperty(), "myParam");
}

TEST(ExceptionTests, ArgumentException_ParamName_InMessage) {
    ArgumentException e("bad value", "myParam");
    EXPECT_NE(e.getMessageProperty().find("myParam"), std::string::npos);
}

TEST(ExceptionTests, ArgumentException_NoParamName_Empty) {
    ArgumentException e("bad arg");
    EXPECT_EQ(e.getParamNameProperty(), "");
}

TEST(ExceptionTests, ArgumentException_InnerException_Stored) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    ArgumentException e("bad arg", inner);
    EXPECT_NE(e.getInnerExceptionProperty(), nullptr);
}

TEST(ExceptionTests, ArgumentException_ParamAndInner) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    ArgumentException e("bad arg", "p", inner);
    EXPECT_EQ(e.getParamNameProperty(), "p");
    EXPECT_NE(e.getInnerExceptionProperty(), nullptr);
}

TEST(ExceptionTests, ArgumentException_ThrowIfNullOrEmpty_Throws) {
    EXPECT_THROW(ArgumentException::ThrowIfNullOrEmpty("", "arg"), ArgumentException);
}

TEST(ExceptionTests, ArgumentException_ThrowIfNullOrEmpty_NoThrow) {
    EXPECT_NO_THROW(ArgumentException::ThrowIfNullOrEmpty("hello", "arg"));
}

TEST(ExceptionTests, ArgumentException_ThrowIfNullOrWhiteSpace_Throws) {
    EXPECT_THROW(ArgumentException::ThrowIfNullOrWhiteSpace("   ", "arg"), ArgumentException);
}

TEST(ExceptionTests, ArgumentException_ThrowIfNullOrWhiteSpace_NoThrow) {
    EXPECT_NO_THROW(ArgumentException::ThrowIfNullOrWhiteSpace("hello", "arg"));
}

TEST(ExceptionTests, ArgumentException_ThrowIfNullOrWhiteSpace_EmptyThrows) {
    EXPECT_THROW(ArgumentException::ThrowIfNullOrWhiteSpace("", "arg"), ArgumentException);
}

// ---------------------------------------------------------------------------
// ArgumentNullException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, ArgumentNullExceptionDefault) {
    ArgumentNullException e;
    EXPECT_NO_THROW((void)e.getMessageProperty());
}

TEST(ExceptionTests, ArgumentNullExceptionParamName) {
    ArgumentNullException e("myParam");
    EXPECT_NO_THROW((void)e.what());
}

TEST(ExceptionTests, ArgumentNullExceptionCatchableAsArgumentException) {
    bool caught = false;
    try { throw ArgumentNullException("p"); }
    catch (const ArgumentException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// ArgumentOutOfRangeException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, ArgumentOutOfRangeExceptionParamName) {
    ArgumentOutOfRangeException e("myParam");
    EXPECT_EQ(e.getParamNameProperty(), "myParam");
}

TEST(ExceptionTests, ArgumentOutOfRangeExceptionIsArgumentException) {
    bool caught = false;
    try { throw ArgumentOutOfRangeException("value must be positive"); }
    catch (const ArgumentException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// OverflowException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, OverflowExceptionDefault) {
    OverflowException e;
    EXPECT_NO_THROW((void)e.what());
}

TEST(ExceptionTests, OverflowExceptionMessage) {
    OverflowException e("arithmetic overflow");
    EXPECT_EQ(e.getMessageProperty(), "arithmetic overflow");
}

TEST(ExceptionTests, OverflowExceptionIsArithmeticException) {
    bool caught = false;
    try { throw OverflowException("over"); }
    catch (const ArithmeticException&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(ArithmeticExceptionTests, DefaultCtor_MessageContainsOverflow) {
    ArithmeticException ex;
    std::string msg = ex.what();
    EXPECT_NE(msg.find("arithmetic"), std::string::npos);
}

TEST(ArithmeticExceptionTests, MessageCtor_WhatContainsMessage) {
    ArithmeticException ex("division by zero");
    EXPECT_NE(std::string(ex.what()).find("division by zero"), std::string::npos);
}

TEST(ArithmeticExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw ArithmeticException(), System::SystemException);
}

TEST(ArithmeticExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    ArithmeticException ex("outer msg", inner);
    EXPECT_NE(std::string(ex.what()).find("outer msg"), std::string::npos);
}

// ---------------------------------------------------------------------------
// FormatException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, FormatExceptionMessage) {
    FormatException e("bad format");
    EXPECT_EQ(e.getMessageProperty(), "bad format");
}

TEST(ExceptionTests, FormatExceptionIsSystemException) {
    bool caught = false;
    try { throw FormatException("fmt"); }
    catch (const SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// InvalidOperationException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, InvalidOperationExceptionMessage) {
    InvalidOperationException e("invalid op");
    EXPECT_EQ(e.getMessageProperty(), "invalid op");
}

TEST(ExceptionTests, InvalidOperationExceptionIsSystemException) {
    bool caught = false;
    try { throw InvalidOperationException("op"); }
    catch (const SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// NotImplementedException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, NotImplementedExceptionDefault) {
    NotImplementedException e;
    EXPECT_NO_THROW((void)e.what());
}

TEST(ExceptionTests, NotImplementedExceptionMessage) {
    NotImplementedException e("not done yet");
    EXPECT_EQ(e.getMessageProperty(), "not done yet");
}

TEST(ExceptionTests, NotImplementedExceptionIsSystemException) {
    bool caught = false;
    try { throw NotImplementedException(); }
    catch (const SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// NotSupportedException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, NotSupportedExceptionMessage) {
    NotSupportedException e("not supported");
    EXPECT_EQ(e.getMessageProperty(), "not supported");
}

// ---------------------------------------------------------------------------
// NullReferenceException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, NullReferenceExceptionMessage) {
    NullReferenceException e("null ref");
    EXPECT_EQ(e.getMessageProperty(), "null ref");
}

TEST(ExceptionTests, NullReferenceExceptionIsSystemException) {
    bool caught = false;
    try { throw NullReferenceException("null"); }
    catch (const SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// ObjectDisposedException
// ---------------------------------------------------------------------------

TEST(ExceptionTests, ObjectDisposedExceptionDefault) {
    ObjectDisposedException e;
    EXPECT_NO_THROW((void)e.what());
}

TEST(ExceptionTests, ObjectDisposedExceptionObjectName) {
    ObjectDisposedException e("MyObject");
    EXPECT_NO_THROW((void)e.what());
}

TEST(ExceptionTests, ObjectDisposedExceptionIsInvalidOperationException) {
    bool caught = false;
    try { throw ObjectDisposedException("obj"); }
    catch (const InvalidOperationException&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// Catch hierarchy
// ---------------------------------------------------------------------------

TEST(ExceptionTests, AllExceptionsAreCatchableAsStdException) {
    // Spot-check: all these must be catchable as std::exception
    auto check = [](auto ex) {
        bool caught = false;
        try { throw ex; }
        catch (const std::exception&) { caught = true; }
        return caught;
    };
    EXPECT_TRUE(check(FormatException("x")));
    EXPECT_TRUE(check(OverflowException("x")));
    EXPECT_TRUE(check(ArgumentNullException("p")));
    EXPECT_TRUE(check(NotImplementedException("x")));
    EXPECT_TRUE(check(ObjectDisposedException("o")));
}

TEST(ExceptionTests, CatchByBaseInsteadOfDerived) {
    // Throw ArgumentNullException, catch as Exception — must work
    bool caught = false;
    try { throw ArgumentNullException("p"); }
    catch (const Exception&) { caught = true; }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// ArrayTypeMismatchException
// ---------------------------------------------------------------------------

TEST(ArrayTypeMismatchExceptionTests, DefaultCtor_MessageContainsWrongType) {
    System::ArrayTypeMismatchException ex;
    std::string msg(ex.what());
    EXPECT_NE(msg.find("wrong type"), std::string::npos);
}

TEST(ArrayTypeMismatchExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw System::ArrayTypeMismatchException(), SystemException);
}

TEST(ArrayTypeMismatchExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("inner cause");
    System::ArrayTypeMismatchException ex("outer msg", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("outer msg"), std::string::npos);
    EXPECT_NE(msg.find("inner cause"), std::string::npos);
}

// ---------------------------------------------------------------------------
// FieldAccessException
// ---------------------------------------------------------------------------

TEST(FieldAccessExceptionTests, DefaultCtor_MessageContainsField) {
    System::FieldAccessException ex;
    EXPECT_NE(std::string(ex.what()).find("field"), std::string::npos);
}

TEST(FieldAccessExceptionTests, IsA_MemberAccessException) {
    EXPECT_THROW(throw System::FieldAccessException(), System::MemberAccessException);
}

TEST(FieldAccessExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("root cause");
    System::FieldAccessException ex("cannot access field", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("cannot access field"), std::string::npos);
    EXPECT_NE(msg.find("root cause"), std::string::npos);
}

// ---------------------------------------------------------------------------
// OutOfMemoryException
// ---------------------------------------------------------------------------

TEST(OutOfMemoryExceptionTests, DefaultCtor_ContainsMemory) {
    System::OutOfMemoryException ex;
    EXPECT_NE(std::string(ex.what()).find("memory"), std::string::npos);
}

TEST(OutOfMemoryExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw System::OutOfMemoryException(), SystemException);
}

TEST(OutOfMemoryExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("root cause");
    System::OutOfMemoryException ex("not enough memory", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("not enough memory"), std::string::npos);
    EXPECT_NE(msg.find("root cause"), std::string::npos);
}

// ---------------------------------------------------------------------------
// InsufficientMemoryException
// ---------------------------------------------------------------------------

TEST(InsufficientMemoryExceptionTests, DefaultCtor_MessageContainsInsufficient) {
    System::InsufficientMemoryException ex;
    EXPECT_NE(std::string(ex.what()).find("Insufficient"), std::string::npos);
}

TEST(InsufficientMemoryExceptionTests, IsA_OutOfMemoryException) {
    bool caught = false;
    try { throw System::InsufficientMemoryException(); }
    catch (const System::OutOfMemoryException&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(InsufficientMemoryExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("no mem");
    System::InsufficientMemoryException ex("allocation failed", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("allocation failed"), std::string::npos);
    EXPECT_NE(msg.find("no mem"), std::string::npos);
}

// ---------------------------------------------------------------------------
// InvalidTimeZoneException
// ---------------------------------------------------------------------------

TEST(InvalidTimeZoneExceptionTests, DefaultCtor_MessageContainsTimeZone) {
    System::InvalidTimeZoneException ex;
    std::string msg(ex.what());
    EXPECT_NE(msg.find("time zone"), std::string::npos);
}

TEST(InvalidTimeZoneExceptionTests, CatchableAsException) {
    bool caught = false;
    try { throw System::InvalidTimeZoneException(); }
    catch (const System::Exception&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(InvalidTimeZoneExceptionTests, CStringCtor_WhatContainsMessage) {
    System::InvalidTimeZoneException ex("bad tz data");
    EXPECT_NE(std::string(ex.what()).find("bad tz data"), std::string::npos);
}

TEST(InvalidTimeZoneExceptionTests, StringCtor_WhatContainsMessage) {
    System::InvalidTimeZoneException ex(std::string("corrupted zone"));
    EXPECT_NE(std::string(ex.what()).find("corrupted zone"), std::string::npos);
}

TEST(InvalidTimeZoneExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("registry error");
    System::InvalidTimeZoneException ex("invalid time zone data", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("invalid time zone data"), std::string::npos);
    EXPECT_NE(msg.find("registry error"), std::string::npos);
}

TEST(InvalidTimeZoneExceptionTests, CatchableAsStdException) {
    bool caught = false;
    try { throw System::InvalidTimeZoneException("tz error"); }
    catch (const std::exception& e) { caught = true; EXPECT_NE(std::string(e.what()), ""); }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// IndexOutOfRangeException
// ---------------------------------------------------------------------------

TEST(IndexOutOfRangeExceptionTests, DefaultCtor_MessageContainsBounds) {
    System::IndexOutOfRangeException ex;
    EXPECT_NE(std::string(ex.what()).find("bounds"), std::string::npos);
}

TEST(IndexOutOfRangeExceptionTests, CStringCtor_WhatContainsMessage) {
    System::IndexOutOfRangeException ex("index 99 is out of range");
    EXPECT_NE(std::string(ex.what()).find("index 99"), std::string::npos);
}

TEST(IndexOutOfRangeExceptionTests, StringCtor_WhatContainsMessage) {
    System::IndexOutOfRangeException ex(std::string("bad index"));
    EXPECT_NE(std::string(ex.what()).find("bad index"), std::string::npos);
}

TEST(IndexOutOfRangeExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("root cause");
    System::IndexOutOfRangeException ex("index exceeded array size", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("index exceeded array size"), std::string::npos);
    EXPECT_NE(msg.find("root cause"), std::string::npos);
}

TEST(IndexOutOfRangeExceptionTests, CatchableAsSystemException) {
    bool caught = false;
    try { throw System::IndexOutOfRangeException(); }
    catch (const System::SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(IndexOutOfRangeExceptionTests, CatchableAsStdException) {
    bool caught = false;
    try { throw System::IndexOutOfRangeException("oob"); }
    catch (const std::exception& e) { caught = true; EXPECT_NE(std::string(e.what()), ""); }
    EXPECT_TRUE(caught);
}

// ---------------------------------------------------------------------------
// ContextMarshalException
// ---------------------------------------------------------------------------

TEST(ContextMarshalExceptionTests, DefaultCtor_MessageContainsMarshal) {
    System::ContextMarshalException ex;
    EXPECT_NE(std::string(ex.what()).find("marshal"), std::string::npos);
}

TEST(ContextMarshalExceptionTests, CStringCtor_WhatContainsMessage) {
    System::ContextMarshalException ex("cannot cross context");
    EXPECT_NE(std::string(ex.what()).find("cannot cross context"), std::string::npos);
}

TEST(ContextMarshalExceptionTests, StringCtor_WhatContainsMessage) {
    System::ContextMarshalException ex(std::string("bad marshal"));
    EXPECT_NE(std::string(ex.what()).find("bad marshal"), std::string::npos);
}

TEST(ContextMarshalExceptionTests, InnerExceptionCtor_ContainsBoth) {
    std::runtime_error inner("root cause");
    System::ContextMarshalException ex("context error", inner);
    std::string msg(ex.what());
    EXPECT_NE(msg.find("context error"), std::string::npos);
    EXPECT_NE(msg.find("root cause"), std::string::npos);
}

TEST(ContextMarshalExceptionTests, CatchableAsSystemException) {
    bool caught = false;
    try { throw System::ContextMarshalException(); }
    catch (const System::SystemException&) { caught = true; }
    EXPECT_TRUE(caught);
}

TEST(ContextMarshalExceptionTests, CatchableAsStdException) {
    bool caught = false;
    try { throw System::ContextMarshalException("err"); }
    catch (const std::exception& e) { caught = true; EXPECT_NE(std::string(e.what()), ""); }
    EXPECT_TRUE(caught);
}
