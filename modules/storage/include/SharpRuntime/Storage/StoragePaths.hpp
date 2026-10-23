// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <filesystem>
#include <functional>

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
         * @brief Resolves the platform default without creating directories or applying overrides.
         *
         * Hosts can derive their application-specific root from this path without recursively
         * invoking their own resolver. This is a hosting extension, not a .NET API.
         */
        [[nodiscard]] static std::filesystem::path GetDefaultIsolatedStorageRootPath();

        /**
         * @brief Overrides the isolated-storage root selected by the platform policy.
         *
         * Hosting frameworks use this during application startup to keep System.IO isolated
         * storage in the same per-user application directory as their own persistent data. An
         * empty path clears the override and restores the platform default. This also replaces
         * any resolver. Setting an override never creates directories.
         *
         * @param root Root directory to use, or an empty path to restore the default.
         */
        static void SetIsolatedStorageRootOverride(const std::filesystem::path& root);

        /**
         * @brief Selects a lazy host policy in place of any fixed root override.
         *
         * Registration performs no filesystem access. GetIsolatedStorageRoot invokes the
         * resolver outside the policy lock, then creates its returned directory. Exceptions
         * propagate to that access; a failure is not cached. An empty function restores the
         * platform default. A resolver must return a nonempty path and must not recursively
         * call GetIsolatedStorageRoot. It may use GetDefaultIsolatedStorageRootPath instead.
         *
         * Callers must synchronize their captured state and unregister the resolver before
         * unloading its code or destroying borrowed state, after outstanding accesses finish.
         * This is a hosting extension, not a .NET API.
         * @param resolver Deferred host policy, or an empty function to restore the default.
         */
        static void SetIsolatedStorageRootResolver(std::function<std::filesystem::path()> resolver);
    };
}
