// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <typeinfo>
#include "System/IO/File.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/ArgumentException.hpp"
#include "System/UnauthorizedAccessException.hpp"

namespace {
namespace fs = std::filesystem;
class FileDeletionTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() /
            ("SharpFileDeletion-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(fs::create_directory(root_));
        owned_ = true;
    }
    void TearDown() override {
        if (owned_) {
            std::error_code error;
            fs::remove_all(root_, error);
        }
    }
    std::string Path(const fs::path& leaf) const {
        const auto bytes = (root_ / leaf).u8string();
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
    fs::path root_;
    bool owned_ = false;
};

TEST_F(FileDeletionTest, DirectoryIsRefusedWithUnauthorizedAccessAndPreserved) {
    ASSERT_TRUE(fs::create_directory(root_ / "directory"));
    EXPECT_THROW(System::IO::File::Delete(Path("directory")), System::UnauthorizedAccessException);
    EXPECT_TRUE(fs::is_directory(root_ / "directory"));
    std::ofstream(root_ / "directory" / "sentinel") << "keep";
    EXPECT_THROW(System::IO::File::Delete(Path("directory")), System::UnauthorizedAccessException);
    EXPECT_TRUE(fs::exists(root_ / "directory" / "sentinel"));
}

TEST_F(FileDeletionTest, MissingParentAndMissingLeafHaveDifferentContracts) {
    EXPECT_THROW(System::IO::File::Delete(Path("absent/file")), System::IO::DirectoryNotFoundException);
    EXPECT_NO_THROW(System::IO::File::Delete(Path("absent-file")));
}

TEST_F(FileDeletionTest, EmbeddedNullCannotDeleteTheExistingPrefix) {
    std::ofstream(root_ / "sentinel") << "keep";
    std::string path = Path("sentinel");
    path.append("\0suffix", 7);
    EXPECT_THROW(System::IO::File::Delete(path), System::ArgumentException);
    EXPECT_TRUE(fs::exists(root_ / "sentinel"));
}

TEST_F(FileDeletionTest, UnicodeFileIsDeletedWithoutTouchingItsSibling) {
    const fs::path leaf = u8"save-žluťoučký-世界.bin";
    std::ofstream(root_ / leaf) << "delete";
    std::ofstream(root_ / "sentinel") << "keep";
    EXPECT_NO_THROW(System::IO::File::Delete(Path(leaf)));
    EXPECT_FALSE(fs::exists(root_ / leaf));
    EXPECT_TRUE(fs::exists(root_ / "sentinel"));
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST_F(FileDeletionTest, DirectorySymlinkIsUnlinkedWithoutFollowingOrRemovingItsTarget) {
    ASSERT_TRUE(fs::create_directory(root_ / "directory"));
    std::ofstream(root_ / "directory" / "sentinel") << "keep";
    fs::create_directory_symlink(root_ / "directory", root_ / "link");
    EXPECT_NO_THROW(System::IO::File::Delete(Path("link")));
    EXPECT_FALSE(fs::is_symlink(fs::symlink_status(root_ / "link")));
    EXPECT_TRUE(fs::exists(root_ / "directory" / "sentinel"));
}
#endif
} // namespace
