// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentException.hpp"
#include "System/Exception.hpp"

using System::ArgumentNullException;

namespace {
    // Counts non-overlapping occurrences of @p needle in @p haystack.
    std::size_t countOccurrences(const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    }
}

TEST(ArgumentNullExceptionTests, DefaultCtor_WhatNotEmpty) {
    ArgumentNullException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(ArgumentNullExceptionTests, IsA_ArgumentException) {
    EXPECT_THROW(throw ArgumentNullException(), System::ArgumentException);
}

TEST(ArgumentNullExceptionTests, HResult_MatchesEPointer) {
    // Regression: previously inherited ArgumentException's HResult; .NET's real value
    // (ArgumentNullException.cs) is E_POINTER, not COR_E_ARGUMENT -- "Use E_POINTER,
    // COM used that for null pointers."
    ArgumentNullException ex;
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(0x80004003));
}

TEST(ArgumentNullExceptionTests, ParamNameCtor_StoresParamName) {
    ArgumentNullException ex("myParam");
    EXPECT_EQ(ex.getParamNameProperty(), "myParam");
}

TEST(ArgumentNullExceptionTests, ParamNameCtor_WhatContainsParamName) {
    ArgumentNullException ex("myParam");
    EXPECT_NE(std::string(ex.what()).find("myParam"), std::string::npos);
}

TEST(ArgumentNullExceptionTests, ParamNameAndMessage_ParamNameStored) {
    ArgumentNullException ex("myParam", "custom message");
    EXPECT_EQ(ex.getParamNameProperty(), "myParam");
}

TEST(ArgumentNullExceptionTests, ParamNameAndMessage_WhatContainsMessage) {
    ArgumentNullException ex("myParam", "custom message");
    EXPECT_NE(std::string(ex.what()).find("custom message"), std::string::npos);
}

TEST(ArgumentNullExceptionTests, MessageAndInner_WhatContainsMessage) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    ArgumentNullException ex("something was null", inner);
    EXPECT_NE(std::string(ex.what()).find("something was null"), std::string::npos);
}

TEST(ArgumentNullExceptionTests, ThrowIfNull_NullThrows) {
    int* p = nullptr;
    EXPECT_THROW(ArgumentNullException::ThrowIfNull(p, "p"), ArgumentNullException);
}

TEST(ArgumentNullExceptionTests, ThrowIfNull_NonNullNoThrow) {
    int x = 42;
    EXPECT_NO_THROW(ArgumentNullException::ThrowIfNull(&x, "x"));
}

TEST(ArgumentNullExceptionTests, ThrowIfNull_ParamNameInException) {
    int* p = nullptr;
    try {
        ArgumentNullException::ThrowIfNull(p, "myPtr");
        FAIL();
    } catch (const ArgumentNullException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "myPtr");
    }
}

// ---------------------------------------------------------------------------
// Regression coverage for ticket #1776 (REMED-CORE-ARGNULL-MESSAGE / SR-AUD-090):
// ArgumentNullException(paramName) used to compose "(Parameter 'x')" twice --
// once in its own private makeMsg() and once again in the ArgumentException
// (message, paramName) base constructor's appendParamName(). Every assertion
// below pins the exact final message so the duplicate cannot silently return.
// ---------------------------------------------------------------------------

TEST(ArgumentNullExceptionTests, DefaultCtor_ExactMessage) {
    ArgumentNullException ex;
    EXPECT_STREQ(ex.what(), "Value cannot be null.");
}

TEST(ArgumentNullExceptionTests, DefaultCtor_ParamNameEmpty) {
    ArgumentNullException ex;
    EXPECT_TRUE(ex.getParamNameProperty().empty());
}

TEST(ArgumentNullExceptionTests, CharPtrParamNameCtor_ExactMessage_SingleSuffix) {
    ArgumentNullException ex("destination");
    EXPECT_STREQ(ex.what(), "Value cannot be null. (Parameter 'destination')");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter 'destination')"), 1u);
}

TEST(ArgumentNullExceptionTests, StringParamNameCtor_ExactMessage_SingleSuffix) {
    ArgumentNullException ex(std::string("destination"));
    EXPECT_STREQ(ex.what(), "Value cannot be null. (Parameter 'destination')");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter 'destination')"), 1u);
}

TEST(ArgumentNullExceptionTests, CharPtrParamNameCtor_ParamNamePropertyExact) {
    ArgumentNullException ex("destination");
    EXPECT_EQ(ex.getParamNameProperty(), "destination");
}

TEST(ArgumentNullExceptionTests, StringParamNameCtor_ParamNamePropertyExact) {
    ArgumentNullException ex(std::string("destination"));
    EXPECT_EQ(ex.getParamNameProperty(), "destination");
}

TEST(ArgumentNullExceptionTests, CharPtrParamNameCtor_EmptyParamName_NoSuffix) {
    // Matches .NET: ArgumentException.Message only appends the marker when
    // ParamName is non-null AND non-empty (string.IsNullOrEmpty(_paramName)).
    ArgumentNullException ex("");
    EXPECT_STREQ(ex.what(), "Value cannot be null.");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter"), 0u);
}

TEST(ArgumentNullExceptionTests, StringParamNameCtor_EmptyParamName_NoSuffix) {
    ArgumentNullException ex(std::string(""));
    EXPECT_STREQ(ex.what(), "Value cannot be null.");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter"), 0u);
}

TEST(ArgumentNullExceptionTests, ParamNameCtor_NameWithSpacesAndPunctuation) {
    ArgumentNullException ex(std::string("my Param.Name'2"));
    EXPECT_EQ(ex.getParamNameProperty(), "my Param.Name'2");
    EXPECT_STREQ(ex.what(), "Value cannot be null. (Parameter 'my Param.Name'2')");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter '"), 1u);
}

TEST(ArgumentNullExceptionTests, CharPtrParamNameAndMessageCtor_ExactMessage_SingleSuffix) {
    ArgumentNullException ex("myParam", "custom message");
    EXPECT_STREQ(ex.what(), "custom message (Parameter 'myParam')");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter 'myParam')"), 1u);
}

TEST(ArgumentNullExceptionTests, StringParamNameAndMessageCtor_ExactMessage_SingleSuffix) {
    ArgumentNullException ex(std::string("myParam"), std::string("custom message"));
    EXPECT_STREQ(ex.what(), "custom message (Parameter 'myParam')");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter 'myParam')"), 1u);
}

TEST(ArgumentNullExceptionTests, MessageAndInnerCtor_ExactMessage_NoSuffix) {
    // The (message, innerException) overload has no paramName concept, so it
    // must never introduce a "(Parameter '...')" marker.
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    ArgumentNullException ex("something was null", inner);
    EXPECT_STREQ(ex.what(), "something was null");
    EXPECT_EQ(countOccurrences(ex.what(), "(Parameter"), 0u);
}

TEST(ArgumentNullExceptionTests, MessageAndInnerCtor_ParamNameEmpty) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    ArgumentNullException ex("something was null", inner);
    EXPECT_TRUE(ex.getParamNameProperty().empty());
}

TEST(ArgumentNullExceptionTests, HResult_UnaffectedByMessageFix) {
    ArgumentNullException ex("p");
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(0x80004003));
}

TEST(ArgumentNullExceptionTests, CopyConstruction_PreservesMessageAndParamName) {
    ArgumentNullException original("copiedParam");
    ArgumentNullException copy(original); // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_STREQ(copy.what(), original.what());
    EXPECT_EQ(copy.getParamNameProperty(), original.getParamNameProperty());
    EXPECT_STREQ(copy.what(), "Value cannot be null. (Parameter 'copiedParam')");
}

TEST(ArgumentNullExceptionTests, MoveConstruction_PreservesMessageAndParamName) {
    ArgumentNullException original("movedParam");
    ArgumentNullException moved(std::move(original));
    EXPECT_STREQ(moved.what(), "Value cannot be null. (Parameter 'movedParam')");
    EXPECT_EQ(moved.getParamNameProperty(), "movedParam");
}

TEST(ArgumentNullExceptionTests, CatchThrough_ArgumentNullExceptionReference) {
    try {
        throw ArgumentNullException("destination");
    } catch (const ArgumentNullException& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'destination')");
    }
}

TEST(ArgumentNullExceptionTests, CatchThrough_ArgumentExceptionReference) {
    try {
        throw ArgumentNullException("destination");
    } catch (const System::ArgumentException& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'destination')");
        EXPECT_EQ(e.getParamNameProperty(), "destination");
    }
}

TEST(ArgumentNullExceptionTests, CatchThrough_SystemExceptionBaseReference) {
    try {
        throw ArgumentNullException("destination");
    } catch (const System::Exception& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'destination')");
    }
}

// Incidental fix verification for audit finding SR-AUD-089 (high, confirmed): the
// pre-#1776 makeMsg() helper concatenated the const char* paramName without a null
// check, so ArgumentNullException(static_cast<const char*>(nullptr)) reached
// std::char_traits<char>::length(nullptr) -- an ASan/UBSan-confirmed null read. This
// ticket's fix routes the raw default message straight to the already null-safe
// ArgumentException(const char*, const char*) base constructor instead, which
// resolves this crash as a natural consequence rather than a separate change.
TEST(ArgumentNullExceptionTests, CharPtrParamNameCtor_NullParamName_DoesNotCrash) {
    ArgumentNullException ex(static_cast<const char*>(nullptr));
    EXPECT_STREQ(ex.what(), "Value cannot be null.");
    EXPECT_TRUE(ex.getParamNameProperty().empty());
}
