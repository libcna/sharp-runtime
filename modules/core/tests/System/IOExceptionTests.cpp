// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-101 / ticket #2278. System::IO::IOException is owned by Core.Base, but
// before this file its only direct tests lived in SharpRuntimeTests_IO, so the
// finding's focused filter selected nothing in the owning component's binary.
// These tests cover the constructor .NET spells IOException(string?, int) and
// the routes the finding recorded as unasserted: the null C-string, the exact
// default text, and the identity of the stored inner exception.
#include <gtest/gtest.h>

#include <exception>
#include <stdexcept>
#include <string>

#include "System/IO/IOException.hpp"
#include "System/SystemException.hpp"

using System::IO::IOException;

namespace {
    constexpr SharpRuntime::intcs CorEIo = static_cast<SharpRuntime::intcs>(0x80131620);
} // namespace

// --- the routes that existed before ------------------------------------------------------

TEST(IOExceptionTests, DefaultCtor_MessageIsTheDotNetDefaultText) {
    IOException ex;
    EXPECT_EQ(ex.getMessageProperty(), "I/O error occurred.");
    EXPECT_STREQ(ex.what(), "I/O error occurred.");
}

TEST(IOExceptionTests, CStringCtor_NullMessage_StoresAnEmptyMessage) {
    IOException ex(static_cast<const char*>(nullptr));
    EXPECT_TRUE(ex.getMessageProperty().empty());
    EXPECT_STREQ(ex.what(), "");
    EXPECT_EQ(ex.getHResultProperty(), CorEIo);
}

TEST(IOExceptionTests, EveryPreExistingCtor_AssignsCorEIo) {
    EXPECT_EQ(IOException().getHResultProperty(), CorEIo);
    EXPECT_EQ(IOException("c string").getHResultProperty(), CorEIo);
    EXPECT_EQ(IOException(std::string("std string")).getHResultProperty(), CorEIo);
    EXPECT_EQ(IOException(std::string("inner"), std::exception_ptr{}).getHResultProperty(), CorEIo);
}

TEST(IOExceptionTests, InnerCtor_StoresTheInnerExceptionAndRethrowsIt) {
    const auto inner = std::make_exception_ptr(std::runtime_error("the cause"));
    const IOException ex(std::string("outer"), inner);

    ASSERT_TRUE(static_cast<bool>(ex.getInnerExceptionProperty()));
    std::string rethrown;
    try {
        std::rethrow_exception(ex.getInnerExceptionProperty());
    } catch (const std::runtime_error& e) {
        rethrown = e.what();
    }
    EXPECT_EQ(rethrown, "the cause");
    EXPECT_EQ(ex.getMessageProperty(), "outer");
}

TEST(IOExceptionTests, IsThrowableAsSystemExceptionAndAsStdException) {
    EXPECT_THROW(throw IOException("err"), System::SystemException);
    bool caught = false;
    try {
        throw IOException("err");
    } catch (const std::exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

// --- the constructor SR-AUD-101 records as absent -----------------------------------------

TEST(IOExceptionTests, HResultCtor_ReplacesCorEIoWithTheCallerCode) {
    const IOException ex(std::string("access denied"), static_cast<SharpRuntime::intcs>(0x80070005));
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(0x80070005));
    EXPECT_EQ(ex.getMessageProperty(), "access denied");
}

TEST(IOExceptionTests, HResultCtor_StoresNoInnerException) {
    const IOException ex(std::string("m"), static_cast<SharpRuntime::intcs>(1));
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
}

TEST(IOExceptionTests, HResultCtor_AcceptsTheWholeIntegerRangeIncludingZero) {
    EXPECT_EQ(IOException(std::string("m"), static_cast<SharpRuntime::intcs>(0)).getHResultProperty(),
              static_cast<SharpRuntime::intcs>(0));
    EXPECT_EQ(IOException(std::string("m"), SharpRuntime::INTCS_MIN).getHResultProperty(), SharpRuntime::INTCS_MIN);
    EXPECT_EQ(IOException(std::string("m"), SharpRuntime::INTCS_MAX).getHResultProperty(), SharpRuntime::INTCS_MAX);
    // An explicit COR_E_IO is honoured rather than being treated as "unset".
    EXPECT_EQ(IOException(std::string("m"), CorEIo).getHResultProperty(), CorEIo);
}

TEST(IOExceptionTests, HResultCtor_PreservesAUtf8MessageByteForByte) {
    const std::string message = "cesta k soubor\xc3\xad nen\xc3\xad dostupn\xc3\xa1";
    const IOException ex(message, static_cast<SharpRuntime::intcs>(42));
    EXPECT_EQ(ex.getMessageProperty(), message);
    EXPECT_EQ(ex.getMessageProperty().size(), message.size());
}

// --- overload resolution, pinned because adding the overload moved one spelling ------------

TEST(IOExceptionTests, LiteralZeroSelectsTheHResultOverload) {
    // Documented consequence of adding IOException(string, intcs): the literal 0 is a null
    // pointer constant and used to mean "no inner exception", leaving COR_E_IO in place. It
    // now means HResult 0. The message and the absent inner exception are unchanged.
    const IOException ex(std::string("m"), 0);
    EXPECT_EQ(ex.getHResultProperty(), static_cast<SharpRuntime::intcs>(0));
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
    EXPECT_EQ(ex.getMessageProperty(), "m");
}

TEST(IOExceptionTests, NullptrStillSelectsTheInnerExceptionOverload) {
    // std::nullptr_t does not convert to an integer, so this spelling is unaffected.
    const IOException ex(std::string("m"), nullptr);
    EXPECT_EQ(ex.getHResultProperty(), CorEIo);
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
}

TEST(IOExceptionTests, AnExceptionPtrLvalueStillSelectsTheInnerExceptionOverload) {
    const auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    const IOException ex(std::string("m"), inner);
    EXPECT_EQ(ex.getHResultProperty(), CorEIo);
    EXPECT_TRUE(static_cast<bool>(ex.getInnerExceptionProperty()));
}
