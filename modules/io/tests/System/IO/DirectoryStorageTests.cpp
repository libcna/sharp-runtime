// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/IOException.hpp"
#include "System/ArgumentException.hpp"

namespace {
namespace fs = std::filesystem;
using System::IO::Directory;
class DirectoryStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() /
            ("SharpDirectoryStorage-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(fs::create_directory(root_));
        owned_ = true;
    }
    void TearDown() override {
        if (owned_) {
            std::error_code error;
            fs::remove_all(root_, error);
        }
    }
    std::string Path(const fs::path& leaf = {}) const {
        const auto bytes = (leaf.empty() ? root_ : root_ / leaf).u8string();
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
    fs::path root_;
    bool owned_ = false;
};

TEST_F(DirectoryStorageTest, CreateRefusesFilesAndAllowsExistingDirectories) {
    std::ofstream(root_ / "seed") << "keep";
    EXPECT_THROW(Directory::CreateDirectory(Path("seed")), System::IO::IOException);
    EXPECT_THROW(Directory::CreateDirectory(Path("seed/child")), System::IO::IOException);
    std::ifstream read(root_ / "seed");
    std::string contents;
    read >> contents;
    EXPECT_EQ(contents, "keep");
    EXPECT_NO_THROW(Directory::CreateDirectory(Path("parent/child")));
    EXPECT_NO_THROW(Directory::CreateDirectory(Path("parent/child")));
    EXPECT_TRUE(fs::is_directory(root_ / "parent/child"));
}

TEST_F(DirectoryStorageTest, MissingEnumerationRootUsesTypedException) {
    EXPECT_THROW((void)Directory::GetFiles(Path("absent")), System::IO::DirectoryNotFoundException);
    EXPECT_THROW((void)Directory::GetFiles(Path("absent"), "*"), System::IO::DirectoryNotFoundException);
    EXPECT_THROW((void)Directory::GetDirectories(Path("absent")), System::IO::DirectoryNotFoundException);
}

TEST_F(DirectoryStorageTest, CreateUsesNativeUnicodePath) {
    const fs::path name = u8"žluťoučký-世界/😀";
    EXPECT_NO_THROW(Directory::CreateDirectory(Path(name)));
    EXPECT_TRUE(fs::is_directory(root_ / name));
}

TEST_F(DirectoryStorageTest, EnumerationReadsNativeUnicodeRootAndReturnsUtf8Paths) {
    const fs::path parent = u8"žluťoučký-世界";
    const fs::path directory = parent / u8"😀";
    const fs::path file = parent / u8"soubor-世界.bin";
    ASSERT_TRUE(fs::create_directories(root_ / directory));
    std::ofstream(root_ / file) << "keep";
    EXPECT_EQ(Directory::GetFiles(Path(parent)), std::vector<std::string>{Path(file)});
    EXPECT_EQ(Directory::GetFiles(Path(parent), "*.bin"), std::vector<std::string>{Path(file)});
    EXPECT_EQ(Directory::GetDirectories(Path(parent)), std::vector<std::string>{Path(directory)});
}

TEST_F(DirectoryStorageTest, NullInPathCannotCreateOrEnumerateItsPrefix) {
    auto create = Path("prefix");
    create.append("\0suffix", 7);
    EXPECT_THROW(Directory::CreateDirectory(create), System::ArgumentException);
    EXPECT_FALSE(fs::exists(root_ / "prefix"));
    auto enumerate = Path();
    enumerate.append("\0suffix", 7);
    EXPECT_THROW((void)Directory::GetFiles(enumerate), System::ArgumentException);
    EXPECT_THROW((void)Directory::GetFiles(enumerate, "*"), System::ArgumentException);
    EXPECT_THROW((void)Directory::GetDirectories(enumerate), System::ArgumentException);
}
} // namespace
