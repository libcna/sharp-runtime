// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// CNA-DEMOS-ISSUE-145: the lower-level directory-open exception precedes title translation.
#include <gtest/gtest.h>
#include <filesystem>
#include <typeinfo>
#include "TestTemporaryDirectory.hpp"
#include "System/ArgumentException.hpp"
#include "System/UnauthorizedAccessException.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/IOException.hpp"

using namespace System::IO;

TEST(FileStreamDirectoryTest, ModeAndAccessPrecedenceMatchesTheGenuineDirectoryMatrix)
{
    SharpRuntime::Tests::TestTemporaryDirectory fixture;
    const std::string path = fixture.path("directory");
    ASSERT_TRUE(std::filesystem::create_directory(path));
    const std::string seed = (std::filesystem::path(path) / "seed.bin").string();
    File::WriteAllText(seed, "retained");
    for (const FileMode mode : {FileMode::CreateNew, FileMode::Create, FileMode::Open,
                               FileMode::OpenOrCreate, FileMode::Truncate, FileMode::Append})
    {
        for (const FileAccess access : {FileAccess::Read, FileAccess::Write, FileAccess::ReadWrite})
        {
            SCOPED_TRACE(static_cast<int>(mode));
            SCOPED_TRACE(static_cast<int>(access));
            const bool invalid = (mode == FileMode::Append && access != FileAccess::Write) ||
                (access == FileAccess::Read && (mode == FileMode::CreateNew || mode == FileMode::Create ||
                                               mode == FileMode::Truncate));
            const std::type_info& expected = invalid ? typeid(System::ArgumentException) :
                mode == FileMode::CreateNew ? typeid(IOException) : typeid(System::UnauthorizedAccessException);
            try
            {
                FileStream stream(path, mode, access);
                ADD_FAILURE() << "An existing directory must not become a file stream";
            }
            catch (const std::exception& error)
            {
                EXPECT_EQ(typeid(error), expected);
            }
            EXPECT_TRUE(std::filesystem::is_directory(path));
            EXPECT_EQ(File::ReadAllText(seed), "retained");
        }
    }
}
