// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Batch tests for remaining exception types in System::, System::Globalization::,
// plus richer exceptions: AggregateException, NotFiniteNumberException,
// TypeInitializationException, CultureNotFoundException.
#include <gtest/gtest.h>
#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include "System/Exception.hpp"
#include "System/AccessViolationException.hpp"
#include "System/AggregateException.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/AppDomainUnloadedException.hpp"
#include "System/ApplicationException.hpp"
#include "System/ArrayTypeMismatchException.hpp"
#include "System/BadImageFormatException.hpp"
#include "System/CannotUnloadAppDomainException.hpp"
#include "System/ContextMarshalException.hpp"
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
TEST(AppDomainUnloadedExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::AppDomainUnloadedException ex("outer", inner);
    EXPECT_NE(std::string(ex.what()).find("outer"), std::string::npos);
}
EXCEPT_SIMPLE(ApplicationException)
TEST(ApplicationExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::ApplicationException ex("outer", inner);
    EXPECT_NE(std::string(ex.what()).find("outer"), std::string::npos);
}
EXCEPT_SIMPLE(ArrayTypeMismatchException)
EXCEPT_SIMPLE(BadImageFormatException)
EXCEPT_SIMPLE(CannotUnloadAppDomainException)
EXCEPT_SIMPLE(DataMisalignedException)
EXCEPT_SIMPLE(DivideByZeroException)
EXCEPT_SIMPLE(DllNotFoundException)
EXCEPT_SIMPLE(DuplicateWaitObjectException)
EXCEPT_SIMPLE(EntryPointNotFoundException)
EXCEPT_SIMPLE(ExecutionEngineException)

TEST(ExecutionEngineExceptionTests, DefaultMessage_ContainsRuntime) {
    System::ExecutionEngineException ex;
    EXPECT_NE(std::string(ex.what()).find("runtime"), std::string::npos);
}

TEST(ExecutionEngineExceptionTests, IsA_SystemException) {
    EXPECT_THROW(throw System::ExecutionEngineException(), System::SystemException);
}

TEST(ExecutionEngineExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("root"));
    System::ExecutionEngineException ex("engine fault", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("engine fault"), std::string::npos);
}

EXCEPT_SIMPLE(FieldAccessException)
EXCEPT_SIMPLE(IndexOutOfRangeException)
EXCEPT_SIMPLE(InsufficientExecutionStackException)
TEST(InsufficientExecutionStackExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::InsufficientExecutionStackException ex("stack too deep", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("stack too deep"), std::string::npos);
}
TEST(InsufficientExecutionStackExceptionTests, HResult_MatchesCorEInsufficientExecutionStack) {
    constexpr SharpRuntime::intcs corE = static_cast<SharpRuntime::intcs>(0x80131578);
    System::InsufficientExecutionStackException defaultCtor;
    System::InsufficientExecutionStackException messageCtor("stack too deep");
    System::InsufficientExecutionStackException innerCtor("stack too deep", std::make_exception_ptr(std::runtime_error("inner")));
    EXPECT_EQ(defaultCtor.getHResultProperty(), corE);
    EXPECT_EQ(messageCtor.getHResultProperty(), corE);
    EXPECT_EQ(innerCtor.getHResultProperty(), corE);
}
EXCEPT_SIMPLE(InsufficientMemoryException)
EXCEPT_SIMPLE(InvalidCastException)
TEST(InvalidCastExceptionTests, HResult_MatchesCorEInvalidCast) {
    constexpr SharpRuntime::intcs corE = static_cast<SharpRuntime::intcs>(0x80004002);
    System::InvalidCastException defaultCtor;
    System::InvalidCastException messageCtor("bad cast");
    System::InvalidCastException innerCtor("bad cast", std::make_exception_ptr(std::runtime_error("inner")));
    EXPECT_EQ(defaultCtor.getHResultProperty(), corE);
    EXPECT_EQ(messageCtor.getHResultProperty(), corE);
    EXPECT_EQ(innerCtor.getHResultProperty(), corE);
}
TEST(InvalidCastExceptionTests, ErrorCodeCtor_UsesCustomHResult) {
    System::InvalidCastException ex("custom cast failure", static_cast<SharpRuntime::intcs>(42));
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(42));
    EXPECT_NE(std::string(ex.what()).find("custom cast failure"), std::string::npos);
}
EXCEPT_SIMPLE(InvalidProgramException)
TEST(InvalidProgramExceptionTests, InnerExceptionCtor_ContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    System::InvalidProgramException ex("bad IL", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("bad IL"), std::string::npos);
}
TEST(InvalidProgramExceptionTests, HResult_MatchesCorEInvalidProgram) {
    constexpr SharpRuntime::intcs corE = static_cast<SharpRuntime::intcs>(0x8013153A);
    System::InvalidProgramException defaultCtor;
    System::InvalidProgramException messageCtor("bad IL");
    System::InvalidProgramException innerCtor("bad IL", std::make_exception_ptr(std::runtime_error("inner")));
    EXPECT_EQ(defaultCtor.getHResultProperty(), corE);
    EXPECT_EQ(messageCtor.getHResultProperty(), corE);
    EXPECT_EQ(innerCtor.getHResultProperty(), corE);
}
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

TEST(TypeAccessExceptionTests, IsA_TypeLoadException) {
    EXPECT_THROW(throw System::TypeAccessException(), System::TypeLoadException);
}

TEST(TypeAccessExceptionTests, InnerExceptionCtor_MessageContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner cause"));
    System::TypeAccessException ex("access denied", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("access denied"), std::string::npos);
}

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
// --- Empty predicate rejected before the loop (#1867 / SR-AUD-099 / CCF-011) ---
//
// Handle used to pass an unvalidated std::function to predicate(ep), raising the native
// std::bad_function_call at the first inner exception -- and to accept the same wrong call
// silently when the aggregate had no inner exception at all. .NET runs
// ArgumentNullException.ThrowIfNull(predicate) before the loop, so both cases throw.
// See docs/EmptyCallableBoundaryPlan.md.
TEST(AggregateExceptionTests, Handle_EmptyPredicate_ThrowsArgumentNullException) {
    auto ep = std::make_exception_ptr(std::runtime_error("boom"));
    System::AggregateException ex({ep});
    EXPECT_THROW(ex.Handle(std::function<bool(std::exception_ptr)>{}),
                 System::ArgumentNullException);
}
TEST(AggregateExceptionTests, Handle_EmptyPredicate_NoInnerExceptions_StillThrows) {
    System::AggregateException ex;
    EXPECT_EQ(ex.getInnerExceptionCountProperty(), 0u);
    EXPECT_THROW(ex.Handle(std::function<bool(std::exception_ptr)>{}),
                 System::ArgumentNullException);
}
TEST(AggregateExceptionTests, Handle_EmptyPredicate_ParamNameIsPredicate) {
    System::AggregateException ex;
    try {
        ex.Handle(std::function<bool(std::exception_ptr)>{});
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'predicate')");
    }
}
TEST(AggregateExceptionTests, Handle_EmptyPredicate_IsCatchableAsSystemException) {
    auto ep = std::make_exception_ptr(std::runtime_error("boom"));
    System::AggregateException ex({ep});
    bool caught = false;
    try {
        ex.Handle(std::function<bool(std::exception_ptr)>{});
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}
TEST(AggregateExceptionTests, Handle_NonEmptyStdFunction_StillBehavesAsBefore) {
    auto ep = std::make_exception_ptr(std::runtime_error("handled"));
    System::AggregateException ex({ep});
    std::function<bool(std::exception_ptr)> all = [](std::exception_ptr) { return true; };
    std::function<bool(std::exception_ptr)> none = [](std::exception_ptr) { return false; };
    EXPECT_NO_THROW(ex.Handle(all));
    EXPECT_THROW(ex.Handle(none), System::AggregateException);
}

TEST(AggregateExceptionTests, Flatten_NestedAggregate_FlattensToLeaves) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("leaf1"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("leaf2"));
    auto inner = std::make_exception_ptr(System::AggregateException({ep1, ep2}));
    System::AggregateException outer({inner});
    auto flat = outer.Flatten();
    EXPECT_EQ(flat.getInnerExceptionCountProperty(), 2u);
}
TEST(AggregateExceptionTests, Flatten_AlreadyFlat_PreservesCount) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    System::AggregateException ex({ep1, ep2});
    auto flat = ex.Flatten();
    EXPECT_EQ(flat.getInnerExceptionCountProperty(), 2u);
}
TEST(AggregateExceptionTests, InitializerList_Ctor_CountMatches) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("x"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("y"));
    System::AggregateException ex({ep1, ep2});
    EXPECT_EQ(ex.getInnerExceptionsProperty().size(), 2u);
}
TEST(AggregateExceptionTests, MessageAndSingle_Ctor_StoresInner) {
    auto ep = std::make_exception_ptr(std::runtime_error("inner"));
    System::AggregateException ex("outer", ep);
    EXPECT_EQ(ex.getInnerExceptionsProperty().size(), 1u);
    EXPECT_EQ(ex.getInnerExceptionProperty(), ep);
    // The bare "outer" is the message this port stores today: the custom-message
    // constructors do not append the contained diagnostics the way the default-message
    // constructors do. That asymmetry is SR-AUD-098 clause C2 and is DEFERRED to ticket
    // #2309, not endorsed here -- closing it requires an object-layout change and .NET
    // reference text that are both unavailable. See docs/AggregateExceptionCausalPlan.md.
    EXPECT_STREQ(ex.what(), "outer");
}

// ---------------------------------------------------------------------------
// AggregateException — the base Exception::innerException_ names the first cause
// (ticket #2307 / SR-AUD-098 clause C1)
//
// System::Exception documents getInnerExceptionProperty() as "the exception that caused
// the current exception". Every constructor of this class used to leave it null while
// getInnerExceptionsProperty() held the causes, so an aggregate over N causes reported
// none. The frozen finding names only the message-taking constructors; measured, all six
// had the gap. See docs/AggregateExceptionCausalPlan.md.
// ---------------------------------------------------------------------------

TEST(AggregateExceptionTests, VectorCtor_BaseInnerExceptionIsTheFirstInner) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    System::AggregateException ex(std::vector<std::exception_ptr>{ep1, ep2});
    EXPECT_EQ(ex.getInnerExceptionProperty(), ep1);
    EXPECT_NE(ex.getInnerExceptionProperty(), ep2);
}
TEST(AggregateExceptionTests, InitializerListCtor_BaseInnerExceptionIsTheFirstInner) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    System::AggregateException ex({ep1, ep2});
    EXPECT_EQ(ex.getInnerExceptionProperty(), ep1);
}
TEST(AggregateExceptionTests, MessageAndVectorCtor_BaseInnerExceptionIsTheFirstInner) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    System::AggregateException ex("outer", std::vector<std::exception_ptr>{ep1, ep2});
    EXPECT_EQ(ex.getInnerExceptionProperty(), ep1);
}
TEST(AggregateExceptionTests, ConstructorsThatStoreNoCause_LeaveTheBaseInnerNull) {
    EXPECT_EQ(System::AggregateException().getInnerExceptionProperty(), nullptr);
    EXPECT_EQ(System::AggregateException("outer").getInnerExceptionProperty(), nullptr);
    EXPECT_EQ(System::AggregateException(std::vector<std::exception_ptr>{})
                  .getInnerExceptionProperty(),
              nullptr);
}
TEST(AggregateExceptionTests, BaseInnerExceptionIsRethrowableAsTheOriginalLeaf) {
    auto ep = std::make_exception_ptr(std::invalid_argument("leaf detail"));
    System::AggregateException ex({ep});
    ASSERT_TRUE(ex.getInnerExceptionProperty() != nullptr);
    try {
        std::rethrow_exception(ex.getInnerExceptionProperty());
        FAIL() << "expected the stored leaf";
    } catch (const std::invalid_argument& e) {
        EXPECT_STREQ(e.what(), "leaf detail");
    }
}
TEST(AggregateExceptionTests, NamingTheFirstCauseDoesNotChangeAnyMessage) {
    // Control: the C1 repair touches only the base inner-exception field. Every message
    // this class produced before it is byte-identical after it.
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    EXPECT_STREQ(System::AggregateException().what(), "One or more errors occurred.");
    EXPECT_STREQ(System::AggregateException(std::vector<std::exception_ptr>{}).what(),
                 "One or more errors occurred.");
    EXPECT_STREQ(System::AggregateException(std::vector<std::exception_ptr>{ep1, ep2}).what(),
                 "One or more errors occurred. (a) (b)");
    EXPECT_STREQ(System::AggregateException("outer", std::vector<std::exception_ptr>{ep1}).what(),
                 "outer");
    EXPECT_STREQ(System::AggregateException("outer", ep1).what(), "outer");
}
TEST(AggregateExceptionTests, NullRejectionKeepsItsExactTypeAndTextAfterC1) {
    // The C1 repair moved the null-element scan into the base-class initializer, which is
    // sequenced ahead of innerExceptions_. The exception type, message and paramName that
    // ticket #1807 / SR-AUD-097 pinned must be unchanged by that move.
    try {
        System::AggregateException ex(std::vector<std::exception_ptr>{nullptr});
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentNullException&) {
        FAIL() << "a null ELEMENT must stay an ArgumentException, not ArgumentNullException";
    } catch (const System::ArgumentException& e) {
        EXPECT_STREQ(e.what(), "An element of innerExceptions was null.");
    }
    try {
        System::AggregateException ex("outer", std::exception_ptr());
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'innerException')");
    }
}
TEST(AggregateExceptionTests, HandleRethrow_NamesItsOwnFirstUnhandledLeaf) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    auto ep3 = std::make_exception_ptr(std::runtime_error("c"));
    System::AggregateException ex({ep1, ep2, ep3});
    try {
        ex.Handle([&](std::exception_ptr ep) { return ep == ep1; });
        FAIL() << "expected the unhandled leaves to be rethrown";
    } catch (const System::AggregateException& out) {
        ASSERT_EQ(out.getInnerExceptionCountProperty(), 2u);
        EXPECT_EQ(out.getInnerExceptionProperty(), ep2);
    }
}
TEST(AggregateExceptionTests, GetBaseException_SingleInner_ReturnsInner) {
    auto ep = std::make_exception_ptr(std::runtime_error("leaf"));
    System::AggregateException ex({ep});
    auto base = ex.GetBaseException();
    EXPECT_EQ(base, ep);
}
TEST(AggregateExceptionTests, GetBaseException_MultipleInner_ReturnsSelf) {
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    System::AggregateException ex({ep1, ep2});
    auto base = ex.GetBaseException();
    EXPECT_NE(base, ep1);
}

// ---------------------------------------------------------------------------
// AggregateException — null inner exceptions (ticket #1807 / SR-AUD-097)
//
// std::rethrow_exception is undefined for a null argument, and three members
// call it, so one accepted null armed three crashes -- buildMessage from the
// collection constructors, collectLeaves from Flatten(), and GetBaseException()
// -- each an ASan SEGV inside std::rethrow_exception itself. Handle() and
// Unwrap() did not crash and were arguably worse: they handed the null to the
// caller, so the crash surfaced in consumer code with no trace of where the
// null entered. .NET rejects both shapes at construction, with two different
// exception types, which these tests pin.
// ---------------------------------------------------------------------------

TEST(AggregateExceptionTests, NullInnerInVector_ThrowsArgumentException) {
    EXPECT_THROW(System::AggregateException(std::vector<std::exception_ptr>{nullptr}),
                 System::ArgumentException);
}

TEST(AggregateExceptionTests, NullInnerInInitializerList_ThrowsArgumentException) {
    EXPECT_THROW(System::AggregateException({std::exception_ptr()}),
                 System::ArgumentException);
}

TEST(AggregateExceptionTests, NullInnerAfterValidEntry_ThrowsArgumentException) {
    // The check is a full scan, not a look at the first entry.
    auto ok = std::make_exception_ptr(std::runtime_error("first"));
    EXPECT_THROW(System::AggregateException(std::vector<std::exception_ptr>{ok, nullptr}),
                 System::ArgumentException);
}

TEST(AggregateExceptionTests, NullInnerInVector_UsesTheDotNetMessage) {
    try {
        System::AggregateException ex(std::vector<std::exception_ptr>{nullptr});
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        // AggregateException.cs -> SR.AggregateException_ctor_InnerExceptionNull.
        EXPECT_NE(std::string(e.what()).find("An element of innerExceptions was null."),
                  std::string::npos)
            << "message was: " << e.what();
    }
}

TEST(AggregateExceptionTests, NullInnerWithMessageAndVector_ThrowsArgumentException) {
    // This constructor never built a message from the vector, so it stored the
    // null silently and deferred the crash to Flatten/GetBaseException/the caller.
    EXPECT_THROW(System::AggregateException("outer", std::vector<std::exception_ptr>{nullptr}),
                 System::ArgumentException);
}

TEST(AggregateExceptionTests, NullSingleInner_ThrowsArgumentNullException) {
    // .NET splits these two: a missing single argument is ArgumentNullException,
    // a null inside a collection is ArgumentException naming the collection.
    EXPECT_THROW(System::AggregateException("outer", std::exception_ptr()),
                 System::ArgumentNullException);
}

TEST(AggregateExceptionTests, NullSingleInner_NamesTheParameter) {
    try {
        System::AggregateException ex("outer", std::exception_ptr());
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("innerException"), std::string::npos)
            << "message was: " << e.what();
    }
}

TEST(AggregateExceptionTests, NullSingleInner_IsNotMerelyAnArgumentException) {
    // Pins the type split rather than accepting the base type: ArgumentNullException
    // derives from ArgumentException, so catching the base alone would pass even if
    // the two constructors were collapsed onto one exception type.
    try {
        System::AggregateException ex("outer", std::exception_ptr());
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException&) {
        SUCCEED();
    }
    try {
        System::AggregateException ex(std::vector<std::exception_ptr>{nullptr});
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentNullException&) {
        FAIL() << "a null collection ELEMENT must be ArgumentException, not ArgumentNullException";
    } catch (const System::ArgumentException&) {
        SUCCEED();
    }
}

TEST(AggregateExceptionTests, NoConstructedAggregateCanHoldANullInner) {
    // The reachability argument in one assertion: every stored inner is non-null,
    // so the three std::rethrow_exception call sites cannot be reached with a null.
    auto ep1 = std::make_exception_ptr(std::runtime_error("a"));
    auto ep2 = std::make_exception_ptr(std::runtime_error("b"));
    const System::AggregateException flat({ep1, ep2});
    const System::AggregateException nested({std::make_exception_ptr(flat), ep1});
    for (const auto& source : {flat, nested, flat.Flatten(), nested.Flatten()}) {
        for (const auto& ep : source.getInnerExceptionsProperty()) {
            EXPECT_NE(ep, nullptr);
        }
    }
}

TEST(AggregateExceptionTests, ValidInnerExceptionsStillWorkAfterValidationAdded) {
    // The normal paths must be untouched by the new checks.
    auto ep1 = std::make_exception_ptr(std::runtime_error("boom"));
    System::AggregateException fromVector(std::vector<std::exception_ptr>{ep1});
    EXPECT_EQ(fromVector.getInnerExceptionCountProperty(), 1u);
    EXPECT_NE(std::string(fromVector.what()).find("boom"), std::string::npos);

    System::AggregateException withMessage("outer", ep1);
    EXPECT_EQ(withMessage.getInnerExceptionCountProperty(), 1u);
    EXPECT_EQ(std::string(withMessage.what()), "outer");

    System::AggregateException empty(std::vector<std::exception_ptr>{});
    EXPECT_EQ(empty.getInnerExceptionCountProperty(), 0u);
    EXPECT_EQ(std::string(empty.what()), "One or more errors occurred.");
}

// ===========================================================================
// Exception — InnerException / Data / StackTrace
// ===========================================================================

TEST(ExceptionTests, InnerException_DefaultNullptr) {
    Exception ex("msg");
    EXPECT_EQ(ex.getInnerExceptionProperty(), nullptr);
}
TEST(ExceptionTests, InnerException_StoredAndRetrievable) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    Exception ex("outer", inner);
    EXPECT_NE(ex.getInnerExceptionProperty(), nullptr);
}
TEST(ExceptionTests, StackTrace_ReturnsEmptyString) {
    Exception ex("msg");
    EXPECT_TRUE(ex.getStackTraceProperty().empty());
}
TEST(ExceptionTests, Data_EmptyByDefault) {
    Exception ex("msg");
    EXPECT_TRUE(ex.getDataProperty().empty());
}
TEST(ExceptionTests, Data_CanStoreAndRetrieveValues) {
    Exception ex("msg");
    ex.getDataProperty()["key"] = "value";
    EXPECT_EQ(ex.getDataProperty().at("key"), "value");
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
TEST(CultureNotFoundExceptionTests, ParamNameAndInvalidCultureNameAndMessage_StoreName) {
    System::Globalization::CultureNotFoundException ex("cultureName", "xx-XX", "bad culture");
    EXPECT_EQ(ex.getInvalidCultureNameProperty(), "xx-XX");
}
TEST(CultureNotFoundExceptionTests, IsA_Exception) {
    EXPECT_THROW(throw System::Globalization::CultureNotFoundException(), System::Exception);
}

// ---------------------------------------------------------------------------
// MethodAccessException — additional tests
// ---------------------------------------------------------------------------

TEST(MethodAccessExceptionTests, DefaultCtor_IsMemberAccessException) {
    EXPECT_THROW(throw System::MethodAccessException(), System::MemberAccessException);
}
TEST(MethodAccessExceptionTests, DefaultCtor_IsSystemException) {
    EXPECT_THROW(throw System::MethodAccessException(), System::SystemException);
}
TEST(MethodAccessExceptionTests, DefaultMsg_ContainsMethod) {
    System::MethodAccessException ex;
    EXPECT_NE(std::string(ex.what()).find("method"), std::string::npos);
}
TEST(MethodAccessExceptionTests, InnerExceptionPtr_Stored) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    System::MethodAccessException ex("access denied", inner);
    EXPECT_EQ(std::string(ex.what()), "access denied");
}

// ---------------------------------------------------------------------------
// MemberAccessException — additional tests
// ---------------------------------------------------------------------------

TEST(MemberAccessExceptionTests, IsSystemException) {
    EXPECT_THROW(throw System::MemberAccessException(), System::SystemException);
}
TEST(MemberAccessExceptionTests, InnerExceptionPtr_Stored) {
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    System::MemberAccessException ex("no access", inner);
    EXPECT_EQ(std::string(ex.what()), "no access");
}

// ---------------------------------------------------------------------------
// CCF-016 — every derived exception reports its own documented HResult
//
// SR-AUD-093/094/095/096/100. Eleven types were written as pure forwarding
// constructors, so none called setHResultProperty and every instance reported
// whatever its nearest base last wrote: COR_E_SYSTEM (0x80131501), COR_E_EXCEPTION
// (0x80131500), COR_E_TYPELOAD (0x80131522) or COR_E_ARGUMENT (0x80070057).
// Values below are read from Common/src/System/HResults.cs; one assertion per
// public constructor, because that is where the value is decided.
// See docs/DerivedExceptionHResultPlan.md (tickets #1873/#1874).
// ---------------------------------------------------------------------------

namespace {
    constexpr SharpRuntime::intcs kCorEException           = static_cast<SharpRuntime::intcs>(0x80131500);
    constexpr SharpRuntime::intcs kCorESystem              = static_cast<SharpRuntime::intcs>(0x80131501);
    constexpr SharpRuntime::intcs kCorETypeLoad            = static_cast<SharpRuntime::intcs>(0x80131522);
    constexpr SharpRuntime::intcs kCorEArgument            = static_cast<SharpRuntime::intcs>(0x80070057);

    constexpr SharpRuntime::intcs kCorEArrayTypeMismatch   = static_cast<SharpRuntime::intcs>(0x80131503);
    constexpr SharpRuntime::intcs kCorEApplication         = static_cast<SharpRuntime::intcs>(0x80131600);
    constexpr SharpRuntime::intcs kCorEAppDomainUnloaded   = static_cast<SharpRuntime::intcs>(0x80131014);
    constexpr SharpRuntime::intcs kCorEBadImageFormat      = static_cast<SharpRuntime::intcs>(0x8007000B);
    constexpr SharpRuntime::intcs kCorECannotUnloadAppDomain = static_cast<SharpRuntime::intcs>(0x80131015);
    constexpr SharpRuntime::intcs kCorEDataMisaligned      = static_cast<SharpRuntime::intcs>(0x80131541);
    constexpr SharpRuntime::intcs kCorEDllNotFound         = static_cast<SharpRuntime::intcs>(0x80131524);
    constexpr SharpRuntime::intcs kCorEEntryPointNotFound  = static_cast<SharpRuntime::intcs>(0x80131523);
    constexpr SharpRuntime::intcs kEPointer                = static_cast<SharpRuntime::intcs>(0x80004003);
    constexpr SharpRuntime::intcs kCorEContextMarshal      = static_cast<SharpRuntime::intcs>(0x80131504);
    constexpr SharpRuntime::intcs kCorEDuplicateWaitObject = static_cast<SharpRuntime::intcs>(0x80131529);

    std::exception_ptr hresultInner() {
        return std::make_exception_ptr(std::runtime_error("inner"));
    }
} // namespace

TEST(DerivedExceptionHResultTests, ArrayTypeMismatchException_EveryCtor) {
    EXPECT_EQ(System::ArrayTypeMismatchException().getHResultProperty(), kCorEArrayTypeMismatch);
    EXPECT_EQ(System::ArrayTypeMismatchException(std::string("m")).getHResultProperty(), kCorEArrayTypeMismatch);
    EXPECT_EQ(System::ArrayTypeMismatchException("m").getHResultProperty(), kCorEArrayTypeMismatch);
    EXPECT_EQ(System::ArrayTypeMismatchException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEArrayTypeMismatch);
}

TEST(DerivedExceptionHResultTests, ApplicationException_EveryCtor) {
    EXPECT_EQ(System::ApplicationException().getHResultProperty(), kCorEApplication);
    EXPECT_EQ(System::ApplicationException(std::string("m")).getHResultProperty(), kCorEApplication);
    EXPECT_EQ(System::ApplicationException(std::string("m"), hresultInner()).getHResultProperty(), kCorEApplication);
}

TEST(DerivedExceptionHResultTests, AppDomainUnloadedException_EveryCtor) {
    EXPECT_EQ(System::AppDomainUnloadedException().getHResultProperty(), kCorEAppDomainUnloaded);
    EXPECT_EQ(System::AppDomainUnloadedException(std::string("m")).getHResultProperty(), kCorEAppDomainUnloaded);
    EXPECT_EQ(System::AppDomainUnloadedException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEAppDomainUnloaded);
}

TEST(DerivedExceptionHResultTests, BadImageFormatException_EveryCtor) {
    EXPECT_EQ(System::BadImageFormatException().getHResultProperty(), kCorEBadImageFormat);
    EXPECT_EQ(System::BadImageFormatException(std::string("m")).getHResultProperty(), kCorEBadImageFormat);
    EXPECT_EQ(System::BadImageFormatException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEBadImageFormat);
    EXPECT_EQ(System::BadImageFormatException(std::string("m"), std::string("f.dll")).getHResultProperty(),
              kCorEBadImageFormat);
    EXPECT_EQ(System::BadImageFormatException(std::string("m"), std::string("f.dll"), hresultInner()).getHResultProperty(),
              kCorEBadImageFormat);
}

TEST(DerivedExceptionHResultTests, CannotUnloadAppDomainException_EveryCtor) {
    EXPECT_EQ(System::CannotUnloadAppDomainException().getHResultProperty(), kCorECannotUnloadAppDomain);
    EXPECT_EQ(System::CannotUnloadAppDomainException(std::string("m")).getHResultProperty(), kCorECannotUnloadAppDomain);
    EXPECT_EQ(System::CannotUnloadAppDomainException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorECannotUnloadAppDomain);
}

TEST(DerivedExceptionHResultTests, DataMisalignedException_EveryCtor) {
    EXPECT_EQ(System::DataMisalignedException().getHResultProperty(), kCorEDataMisaligned);
    EXPECT_EQ(System::DataMisalignedException(std::string("m")).getHResultProperty(), kCorEDataMisaligned);
    EXPECT_EQ(System::DataMisalignedException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEDataMisaligned);
}

TEST(DerivedExceptionHResultTests, DllNotFoundException_EveryCtor) {
    EXPECT_EQ(System::DllNotFoundException().getHResultProperty(), kCorEDllNotFound);
    EXPECT_EQ(System::DllNotFoundException(std::string("m")).getHResultProperty(), kCorEDllNotFound);
    EXPECT_EQ(System::DllNotFoundException(std::string("m"), hresultInner()).getHResultProperty(), kCorEDllNotFound);
}

TEST(DerivedExceptionHResultTests, EntryPointNotFoundException_EveryCtor) {
    EXPECT_EQ(System::EntryPointNotFoundException().getHResultProperty(), kCorEEntryPointNotFound);
    EXPECT_EQ(System::EntryPointNotFoundException(std::string("m")).getHResultProperty(), kCorEEntryPointNotFound);
    EXPECT_EQ(System::EntryPointNotFoundException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEEntryPointNotFound);
}

TEST(DerivedExceptionHResultTests, AccessViolationException_EveryCtor) {
    EXPECT_EQ(System::AccessViolationException().getHResultProperty(), kEPointer);
    EXPECT_EQ(System::AccessViolationException(std::string("m")).getHResultProperty(), kEPointer);
    EXPECT_EQ(System::AccessViolationException(std::string("m"), hresultInner()).getHResultProperty(), kEPointer);
}

TEST(DerivedExceptionHResultTests, DuplicateWaitObjectException_EveryCtor) {
    EXPECT_EQ(System::DuplicateWaitObjectException().getHResultProperty(), kCorEDuplicateWaitObject);
    EXPECT_EQ(System::DuplicateWaitObjectException(std::string("p")).getHResultProperty(), kCorEDuplicateWaitObject);
    EXPECT_EQ(System::DuplicateWaitObjectException(std::string("p"), std::string("m")).getHResultProperty(),
              kCorEDuplicateWaitObject);
    EXPECT_EQ(System::DuplicateWaitObjectException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEDuplicateWaitObject);
}

TEST(DerivedExceptionHResultTests, DuplicateWaitObjectException_DefaultMessageMatchesReferenceVerbatim) {
    // SR-AUD-100 additionally claimed the default text diverges from .NET. It does not:
    // SR.Arg_DuplicateWaitObjectException is "Duplicate objects in argument."
    // (System.Private.CoreLib/src/Resources/Strings.resx:319-321), byte-identical to the
    // port's. That half of the finding is a false positive; the text is deliberately
    // unchanged and pinned here so a future "fix" cannot silently diverge from .NET.
    EXPECT_STREQ(System::DuplicateWaitObjectException().what(), "Duplicate objects in argument.");
}

TEST(DerivedExceptionHResultTests, DuplicateWaitObjectException_ParamNameAndMessagePreserved) {
    System::DuplicateWaitObjectException byParam(std::string("waitHandles"));
    const std::string w = byParam.what();
    EXPECT_NE(w.find("Duplicate objects in argument."), std::string::npos);
    EXPECT_NE(w.find("(Parameter 'waitHandles')"), std::string::npos);
    // exactly once -- guards the #1776 duplicate-suffix regression
    EXPECT_EQ(w.find("(Parameter 'waitHandles')"), w.rfind("(Parameter 'waitHandles')"));

    System::DuplicateWaitObjectException withMessage(std::string("waitHandles"), std::string("custom text"));
    const std::string w2 = withMessage.what();
    EXPECT_NE(w2.find("custom text"), std::string::npos);
    EXPECT_NE(w2.find("(Parameter 'waitHandles')"), std::string::npos);
}

TEST(DerivedExceptionHResultTests, CustomMessagesArePreservedAlongsideTheHResult) {
    EXPECT_NE(std::string(System::ApplicationException(std::string("boom")).what()).find("boom"), std::string::npos);
    EXPECT_NE(std::string(System::DllNotFoundException(std::string("boom")).what()).find("boom"), std::string::npos);
    EXPECT_NE(std::string(System::AccessViolationException(std::string("boom")).what()).find("boom"), std::string::npos);
    EXPECT_NE(std::string(System::BadImageFormatException(std::string("boom")).what()).find("boom"), std::string::npos);
}

TEST(DerivedExceptionHResultTests, HResultSurvivesCatchThroughBaseReference) {
    try {
        throw System::DllNotFoundException();
    } catch (const System::TypeLoadException& e) {
        EXPECT_EQ(e.getHResultProperty(), kCorEDllNotFound);
    }
    try {
        throw System::AccessViolationException();
    } catch (const System::SystemException& e) {
        EXPECT_EQ(e.getHResultProperty(), kEPointer);
    }
    try {
        throw System::DuplicateWaitObjectException();
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getHResultProperty(), kCorEDuplicateWaitObject);
    }
    try {
        throw System::ApplicationException();
    } catch (const System::Exception& e) {
        EXPECT_EQ(e.getHResultProperty(), kCorEApplication);
    }
}

TEST(DerivedExceptionHResultTests, HResultSurvivesCopyAndAssignment) {
    System::DataMisalignedException original(std::string("m"));
    System::DataMisalignedException copy(original);
    EXPECT_EQ(copy.getHResultProperty(), kCorEDataMisaligned);
    System::DataMisalignedException assigned(std::string("other"));
    assigned = original;
    EXPECT_EQ(assigned.getHResultProperty(), kCorEDataMisaligned);
}

TEST(DerivedExceptionHResultTests, BaseTypesKeepTheirOwnCodes) {
    // The derived corrections must not leak upward.
    EXPECT_EQ(System::Exception("m").getHResultProperty(), kCorEException);
    EXPECT_EQ(System::SystemException("m").getHResultProperty(), kCorESystem);
    EXPECT_EQ(System::TypeLoadException("m").getHResultProperty(), kCorETypeLoad);
    EXPECT_EQ(System::ArgumentException("m").getHResultProperty(), kCorEArgument);
}

TEST(DerivedExceptionHResultTests, AggregateExceptionDeliberatelyInheritsCorEException) {
    // Same "no setHResultProperty anywhere" shape as the eleven, but CORRECT: .NET's
    // AggregateException.cs assigns no HResult either, so inheriting COR_E_EXCEPTION is
    // parity. Pinned so a future sweep does not "fix" it into a divergence.
    EXPECT_EQ(System::AggregateException().getHResultProperty(), kCorEException);
    auto ep = std::make_exception_ptr(std::runtime_error("x"));
    EXPECT_EQ(System::AggregateException({ep}).getHResultProperty(), kCorEException);
}

TEST(DerivedExceptionHResultTests, ContextMarshalException_EveryCtor) {
    EXPECT_EQ(System::ContextMarshalException().getHResultProperty(), kCorEContextMarshal);
    EXPECT_EQ(System::ContextMarshalException("m").getHResultProperty(), kCorEContextMarshal);
    EXPECT_EQ(System::ContextMarshalException(std::string("m")).getHResultProperty(), kCorEContextMarshal);
    EXPECT_EQ(System::ContextMarshalException(std::string("m"), hresultInner()).getHResultProperty(),
              kCorEContextMarshal);
    try {
        throw System::ContextMarshalException();
    } catch (const System::SystemException& e) {
        EXPECT_EQ(e.getHResultProperty(), kCorEContextMarshal);
    }
}
