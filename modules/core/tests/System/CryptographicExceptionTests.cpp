// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-101 / ticket #2278. System::Security::Cryptography::CryptographicException
// is owned by Core.Base while its only direct tests lived in
// SharpRuntimeTests_Security_Cryptography. Covers the (format, insert)
// constructor the finding records as absent, the nullptr overload that keeps
// CryptographicException(message, nullptr) compiling, and the default HResult,
// inner identity and UTF-8 routes the finding records as unasserted.
#include <gtest/gtest.h>

#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>

#include "System/FormatException.hpp"
#include "System/Security/Cryptography/CryptographicException.hpp"
#include "System/SystemException.hpp"

using System::Security::Cryptography::CryptographicException;

namespace {
    // CryptographicException deliberately inherits SystemException's code; .NET does
    // the same rather than defining a cryptography-specific default.
    constexpr SharpRuntime::intcs CorESystem = static_cast<SharpRuntime::intcs>(0x80131501);
} // namespace

TEST(CryptographicExceptionTests, DefaultAndMessageCtors_InheritCorESystem) {
    EXPECT_EQ(CryptographicException().getHResultProperty(), CorESystem);
    EXPECT_EQ(CryptographicException(std::string("m")).getHResultProperty(), CorESystem);
    EXPECT_EQ(CryptographicException(std::string("m"), std::exception_ptr{}).getHResultProperty(), CorESystem);
}

TEST(CryptographicExceptionTests, InnerCtor_StoresTheInnerExceptionAndRethrowsIt) {
    const auto inner = std::make_exception_ptr(std::runtime_error("bad padding"));
    const CryptographicException ex(std::string("decryption failed"), inner);

    ASSERT_TRUE(static_cast<bool>(ex.getInnerExceptionProperty()));
    std::string rethrown;
    try {
        std::rethrow_exception(ex.getInnerExceptionProperty());
    } catch (const std::runtime_error& e) {
        rethrown = e.what();
    }
    EXPECT_EQ(rethrown, "bad padding");
}

// --- the constructor SR-AUD-101 records as absent -----------------------------------------

TEST(CryptographicExceptionTests, FormatCtor_SubstitutesTheInsert) {
    const CryptographicException ex(std::string("Bad key size {0}."), std::string("512"));
    EXPECT_EQ(ex.getMessageProperty(), "Bad key size 512.");
    EXPECT_EQ(ex.getHResultProperty(), CorESystem);
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
}

TEST(CryptographicExceptionTests, FormatCtor_RepeatsTheInsertForEveryOccurrence) {
    const CryptographicException ex(std::string("{0}-{0}"), std::string("x"));
    EXPECT_EQ(ex.getMessageProperty(), "x-x");
}

TEST(CryptographicExceptionTests, FormatCtor_TreatsDoubledBracesAsLiteralBraces) {
    const CryptographicException ex(std::string("{{0}}"), std::string("ignored"));
    EXPECT_EQ(ex.getMessageProperty(), "{0}");
}

TEST(CryptographicExceptionTests, FormatCtor_AFormatWithNoItemIgnoresTheInsert) {
    const CryptographicException ex(std::string("plain text"), std::string("ignored"));
    EXPECT_EQ(ex.getMessageProperty(), "plain text");
}

TEST(CryptographicExceptionTests, FormatCtor_AcceptsAnEmptyInsert) {
    const CryptographicException ex(std::string("[{0}]"), std::string());
    EXPECT_EQ(ex.getMessageProperty(), "[]");
}

TEST(CryptographicExceptionTests, FormatCtor_PreservesAUtf8InsertByteForByte) {
    const std::string insert = "kl\xc3\xad\xc4\x8d \xe2\x82\xac";
    const CryptographicException ex(std::string("chyba: {0}"), insert);
    EXPECT_EQ(ex.getMessageProperty(), "chyba: " + insert);
}

TEST(CryptographicExceptionTests, FormatCtor_RejectsAMalformedFormatWithFormatException) {
    EXPECT_THROW(CryptographicException(std::string("{0"), std::string("x")), System::FormatException);
    EXPECT_THROW(CryptographicException(std::string("0}"), std::string("x")), System::FormatException);
    // Only argument 0 exists, exactly as in .NET's single-insert overload.
    EXPECT_THROW(CryptographicException(std::string("{1}"), std::string("x")), System::FormatException);
}

TEST(CryptographicExceptionTests, FormatCtor_IsSelectedForTwoCStringArguments) {
    const CryptographicException ex("Bad key size {0}.", "512");
    EXPECT_EQ(ex.getMessageProperty(), "Bad key size 512.");
}

// --- the spelling the disambiguating overload keeps alive ----------------------------------

TEST(CryptographicExceptionTests, NullptrSecondArgument_StillMeansNoInnerException) {
    // Without the std::nullptr_t overload this call becomes ambiguous once
    // (format, insert) exists, because nullptr reaches both std::exception_ptr and
    // the C++23 deleted std::basic_string(std::nullptr_t).
    const CryptographicException ex(std::string("m"), nullptr);
    EXPECT_EQ(ex.getMessageProperty(), "m");
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
    EXPECT_EQ(ex.getHResultProperty(), CorESystem);
}

TEST(CryptographicExceptionTests, NullptrOverload_MatchesTheEmptyExceptionPtrOverloadExactly) {
    const CryptographicException viaNullptr(std::string("m"), nullptr);
    const CryptographicException viaEmptyPtr(std::string("m"), std::exception_ptr{});
    EXPECT_EQ(viaNullptr.getMessageProperty(), viaEmptyPtr.getMessageProperty());
    EXPECT_EQ(viaNullptr.getHResultProperty(), viaEmptyPtr.getHResultProperty());
    EXPECT_EQ(static_cast<bool>(viaNullptr.getInnerExceptionProperty()),
              static_cast<bool>(viaEmptyPtr.getInnerExceptionProperty()));
}

TEST(CryptographicExceptionTests, FormatCtor_IsThrowableAsSystemException) {
    EXPECT_THROW(throw CryptographicException(std::string("bad {0}"), std::string("key")),
                 System::SystemException);
}
