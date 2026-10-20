// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <typeinfo>
#include "TestTemporaryDirectory.hpp"
#include "System/ArgumentException.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileStream.hpp"

TEST(FileStreamPathValidationTest, InvalidNamesCannotAliasOrTruncateThePrefixFile)
{
    SharpRuntime::Tests::TestTemporaryDirectory fixture;
    const std::string seed = fixture.path("seed.bin");
    for (const auto mode : {System::IO::FileMode::CreateNew, System::IO::FileMode::Create,
                           System::IO::FileMode::Open, System::IO::FileMode::OpenOrCreate,
                           System::IO::FileMode::Truncate, System::IO::FileMode::Append})
    for (const auto access : {System::IO::FileAccess::Read, System::IO::FileAccess::Write,
                             System::IO::FileAccess::ReadWrite})
    {
        SCOPED_TRACE(static_cast<int>(mode));
        SCOPED_TRACE(static_cast<int>(access));
        System::IO::File::WriteAllText(seed, "retained");
        for (const auto& path : {seed + std::string("\0suffix", 7), std::string{}})
        {
            SCOPED_TRACE(path.empty() ? "empty" : "embedded NUL");
            try
            {
                System::IO::FileStream stream(path, mode, access);
                ADD_FAILURE() << "Invalid path was accepted";
            }
            catch (const std::exception& error)
            {
                EXPECT_EQ(typeid(error), typeid(System::ArgumentException));
            }
        }
        EXPECT_EQ(System::IO::File::ReadAllText(seed), "retained");
    }
}
