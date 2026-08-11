// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-101 / ticket #2278. Covers the (message, directoryPath, inner)
// constructor the finding records as absent, plus the routes it records as
// unasserted: the null C-string, the retained path, the identity of the stored
// inner exception, and UTF-8 path and message text.
#include <gtest/gtest.h>

#include <exception>
#include <stdexcept>
#include <string>

#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/IOException.hpp"

using System::IO::DirectoryNotFoundException;

namespace {
    constexpr SharpRuntime::intcs CorEDirectoryNotFound = static_cast<SharpRuntime::intcs>(0x80070003);
} // namespace

TEST(DirectoryNotFoundExceptionTests, CStringCtor_NullMessage_StoresAnEmptyMessage) {
    const DirectoryNotFoundException ex(static_cast<const char*>(nullptr));
    EXPECT_TRUE(ex.getMessageProperty().empty());
    EXPECT_EQ(ex.getHResultProperty(), CorEDirectoryNotFound);
}

TEST(DirectoryNotFoundExceptionTests, TheDirectoryPathIsEmptyUnlessTheCallerSuppliesOne) {
    EXPECT_TRUE(DirectoryNotFoundException().getDirectoryPathProperty().empty());
    EXPECT_TRUE(DirectoryNotFoundException("c string").getDirectoryPathProperty().empty());
    EXPECT_TRUE(DirectoryNotFoundException(std::string("m")).getDirectoryPathProperty().empty());
    EXPECT_TRUE(
        DirectoryNotFoundException(std::string("m"), std::exception_ptr{}).getDirectoryPathProperty().empty());
}

TEST(DirectoryNotFoundExceptionTests, PathCtor_LeavesTheInnerExceptionEmpty) {
    const DirectoryNotFoundException ex(std::string("m"), std::string("/var/missing"));
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
    EXPECT_EQ(ex.getDirectoryPathProperty(), "/var/missing");
}

// --- the constructor SR-AUD-101 records as absent -----------------------------------------

TEST(DirectoryNotFoundExceptionTests, PathAndInnerCtor_RetainsBothThePathAndTheCause) {
    const auto inner = std::make_exception_ptr(std::runtime_error("ENOENT"));
    const DirectoryNotFoundException ex(std::string("no such dir"), std::string("/var/missing"), inner);

    EXPECT_EQ(ex.getMessageProperty(), "no such dir");
    EXPECT_EQ(ex.getDirectoryPathProperty(), "/var/missing");
    ASSERT_TRUE(static_cast<bool>(ex.getInnerExceptionProperty()));

    std::string rethrown;
    try {
        std::rethrow_exception(ex.getInnerExceptionProperty());
    } catch (const std::runtime_error& e) {
        rethrown = e.what();
    }
    EXPECT_EQ(rethrown, "ENOENT");
}

TEST(DirectoryNotFoundExceptionTests, PathAndInnerCtor_AssignsCorEDirectoryNotFound) {
    const DirectoryNotFoundException ex(std::string("m"), std::string("/p"), std::exception_ptr{});
    EXPECT_EQ(ex.getHResultProperty(), CorEDirectoryNotFound);
}

TEST(DirectoryNotFoundExceptionTests, PathAndInnerCtor_AcceptsANullptrCause) {
    const DirectoryNotFoundException ex(std::string("m"), std::string("/p"), nullptr);
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()));
    EXPECT_EQ(ex.getDirectoryPathProperty(), "/p");
}

TEST(DirectoryNotFoundExceptionTests, PathAndInnerCtor_IsCaughtAsIOException) {
    EXPECT_THROW(throw DirectoryNotFoundException(std::string("m"), std::string("/p"), std::exception_ptr{}),
                 System::IO::IOException);
}

TEST(DirectoryNotFoundExceptionTests, Utf8MessageAndPathArePreservedByteForByte) {
    const std::string message = "adres\xc3\xa1\xc5\x99 nebyl nalezen";
    const std::string path = "/tmp/slo\xc5\xbe" "ka/\xc3\xa1\xc4\x8d";
    const DirectoryNotFoundException ex(message, path, std::exception_ptr{});

    EXPECT_EQ(ex.getMessageProperty(), message);
    EXPECT_EQ(ex.getDirectoryPathProperty(), path);
    EXPECT_EQ(ex.getDirectoryPathProperty().size(), path.size());
}
