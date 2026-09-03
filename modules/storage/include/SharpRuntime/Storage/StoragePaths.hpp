// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>

namespace SharpRuntime::Storage
{
    /**
     * @brief Provides filesystem paths for persistent storage used by the application.
     *
     * @note Status: IMPLEMENTED
     */
    class StoragePaths
    {
    public:
        StoragePaths() = delete;
        ~StoragePaths() = delete;

        /**
         * @brief Gets the root directory for isolated storage.
         *
         * The directory is created if it does not already exist.
         *
         * @return Filesystem path to isolated storage root.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static std::filesystem::path GetIsolatedStorageRoot();

        /**
         * @brief Overrides the isolated-storage root selected by the platform policy.
         *
         * Hosting frameworks use this during application startup to keep System.IO isolated
         * storage in the same per-user application directory as their own persistent data. An
         * empty path clears the override and restores the platform default.
         *
         * @param root Root directory to use, or an empty path to restore the default.
         */
        static void SetIsolatedStorageRootOverride(const std::filesystem::path& root);
    };
}
