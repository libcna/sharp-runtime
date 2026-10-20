// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// CNA-DEMOS-ISSUE-056: genuine .NET Framework4 FileStreamEofOracle.cs checks.
#include <gtest/gtest.h>
#include <array>
#include "TestTemporaryDirectory.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/SeekOrigin.hpp"

using namespace System::IO;
namespace
{
class FileStreamEofTest : public ::testing::Test
{
protected:
    SharpRuntime::Tests::TestTemporaryDirectory fixture;
    std::string path;
    void SetUp() override
    {
        path = fixture.path("seed.bin");
        File::WriteAllText(path, "abcd");
    }
};
}

TEST_F(FileStreamEofTest, ShortAndRepeatedReadsPreserveTheEndPositionAndBufferGuards)
{
    FileStream stream(path, FileMode::Open, FileAccess::Read);
    std::array<SharpRuntime::bytecs, 8> bytes{77,77,77,77,77,77,77,77};
    EXPECT_EQ(stream.Read(bytes.data(), 1, 6), 4);
    EXPECT_EQ(bytes, (std::array<SharpRuntime::bytecs, 8>{77,'a','b','c','d',77,77,77}));
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_EQ(stream.Read(bytes.data(), 1, 2), 0);
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_EQ(stream.ReadByte(), -1);
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_EQ(bytes, (std::array<SharpRuntime::bytecs, 8>{77,'a','b','c','d',77,77,77}));
}

TEST_F(FileStreamEofTest, RelativeSeekAfterEofUsesTheActualPosition)
{
    FileStream stream(path, FileMode::Open, FileAccess::Read);
    std::array<SharpRuntime::bytecs, 8> bytes{};
    EXPECT_EQ(stream.Read(bytes.data(), 0, 8), 4);
    EXPECT_EQ(stream.Seek(-1, SeekOrigin::Current), 3);
    EXPECT_EQ(stream.ReadByte(), 'd');
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_EQ(stream.Seek(0, SeekOrigin::Begin), 0);
    EXPECT_EQ(stream.ReadByte(), 'a');
}

TEST_F(FileStreamEofTest, BothWritePathsAfterEofAdvanceAndPersistTheirBytes)
{
    for (bool singleByte : {false, true})
    {
        SCOPED_TRACE(singleByte);
        File::WriteAllText(path, "abcd");
        {
            FileStream stream(path, FileMode::Open, FileAccess::ReadWrite);
            std::array<SharpRuntime::bytecs, 8> bytes{};
            EXPECT_EQ(stream.Read(bytes.data(), 0, 8), 4);
            if (singleByte) stream.WriteByte('!');
            else
            {
                const SharpRuntime::bytecs value = '!';
                stream.Write(&value, 0, 1);
            }
            EXPECT_EQ(stream.getPositionProperty(), 5);
            EXPECT_EQ(stream.getLengthProperty(), 5);
            stream.setPositionProperty(0);
            EXPECT_EQ(stream.Read(bytes.data(), 0, 8), 5);
            EXPECT_EQ(bytes[4], '!');
            EXPECT_EQ(stream.getPositionProperty(), 5);
        }
        EXPECT_EQ(File::ReadAllText(path), "abcd!");
    }
}
