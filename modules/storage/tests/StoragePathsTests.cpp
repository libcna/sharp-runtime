// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include "SharpRuntime/Storage/StoragePaths.hpp"

namespace
{
namespace fs = std::filesystem;
using SharpRuntime::Storage::StoragePaths;

class StoragePathsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_ = fs::temp_directory_path() /
            ("SharpStoragePolicy-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(fs::create_directory(root_));
        owned_ = true;
    }
    void TearDown() override
    {
        StoragePaths::SetIsolatedStorageRootOverride({});
        if (owned_)
        {
            std::error_code error;
            fs::remove_all(root_, error);
        }
    }
    fs::path root_;
    bool owned_ = false;
};

TEST_F(StoragePathsTest, ResolverRegistrationDoesNotAccessTheDirectory)
{
    int calls = 0;
    const auto target = root_ / "lazy";
    StoragePaths::SetIsolatedStorageRootResolver([&] { ++calls; return target; });
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(fs::exists(target));
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), target);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(fs::is_directory(target));
    StoragePaths::SetIsolatedStorageRootResolver({});
}

TEST_F(StoragePathsTest, FailedResolutionPropagatesAndCanBeRetried)
{
    int calls = 0;
    const auto target = root_ / "recovered";
    StoragePaths::SetIsolatedStorageRootResolver([&]() -> fs::path
    {
        if (++calls == 1) throw std::runtime_error("device unavailable");
        return target;
    });
    EXPECT_THROW((void)StoragePaths::GetIsolatedStorageRoot(), std::runtime_error);
    EXPECT_FALSE(fs::exists(target));
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), target);
    EXPECT_EQ(calls, 2);
    StoragePaths::SetIsolatedStorageRootResolver({});
}

TEST_F(StoragePathsTest, DirectoryCreationErrorsAreDeferredToAccess)
{
    const auto blocker = root_ / "file";
    std::ofstream(blocker) << "not a directory";
    ASSERT_NO_THROW(StoragePaths::SetIsolatedStorageRootResolver([=] { return blocker / "child"; }));
    EXPECT_THROW((void)StoragePaths::GetIsolatedStorageRoot(), fs::filesystem_error);
}

TEST_F(StoragePathsTest, AFixedOverrideReplacesTheResolverAndViceVersa)
{
    const auto fixed = root_ / "fixed";
    const auto lazy = root_ / "lazy";
    StoragePaths::SetIsolatedStorageRootResolver([]() -> fs::path
    {
        throw std::runtime_error("superseded resolver must not run");
    });
    StoragePaths::SetIsolatedStorageRootOverride(fixed);
    EXPECT_FALSE(fs::exists(fixed));
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), fixed);
    StoragePaths::SetIsolatedStorageRootResolver([=] { return lazy; });
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), lazy);
}

TEST_F(StoragePathsTest, AResolverRunsOutsideThePolicyLockAndMayReplaceItself)
{
    const auto first = root_ / "first";
    const auto second = root_ / "second";
    StoragePaths::SetIsolatedStorageRootResolver([=]
    {
        StoragePaths::SetIsolatedStorageRootOverride(second);
        return first;
    });
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), first);
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), second);
}

TEST_F(StoragePathsTest, DefaultPathIgnoresOverridesAndDoesNotCreateTheDirectory)
{
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    struct CurrentDirectory
    {
        fs::path previous = fs::current_path();
        ~CurrentDirectory() { fs::current_path(previous); }
    } restore;
    fs::current_path(root_);
    const auto expected = root_ / ".cna_isolated_storage";
    StoragePaths::SetIsolatedStorageRootResolver([]() -> fs::path
    {
        throw std::runtime_error("default path must not use the host policy");
    });
    EXPECT_EQ(StoragePaths::GetDefaultIsolatedStorageRootPath(), expected);
    EXPECT_FALSE(fs::exists(expected));
    StoragePaths::SetIsolatedStorageRootResolver({});
    EXPECT_EQ(StoragePaths::GetIsolatedStorageRoot(), expected);
    EXPECT_TRUE(fs::is_directory(expected));
#else
    EXPECT_FALSE(StoragePaths::GetDefaultIsolatedStorageRootPath().empty());
#endif
}
}
