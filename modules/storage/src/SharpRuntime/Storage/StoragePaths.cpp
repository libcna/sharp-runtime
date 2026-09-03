// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "SharpRuntime/Storage/StoragePaths.hpp"

#include <filesystem>
#include <mutex>
#include <optional>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#endif

namespace SharpRuntime::Storage
{
    namespace
    {
        std::mutex rootOverrideMutex;
        std::optional<std::filesystem::path> rootOverride;
    }

    std::filesystem::path StoragePaths::GetIsolatedStorageRoot()
    {
        std::filesystem::path configuredRoot;
        {
            const std::scoped_lock lock(rootOverrideMutex);
            if (rootOverride.has_value())
            {
                configuredRoot = *rootOverride;
            }
        }

        if (!configuredRoot.empty())
        {
            std::filesystem::create_directories(configuredRoot);
            return configuredRoot;
        }

#if defined(__EMSCRIPTEN__)
        // On Emscripten, persist save data under /save which is mounted as
        // IDBFS by the application startup code so data survives page reloads.
        const std::filesystem::path root = std::filesystem::path("/save") / ".cna_isolated_storage";
#elif defined(__ANDROID__)
        // Android applications cannot write to their process working directory. SDL exposes the
        // package-private files directory directly; unlike SDL_GetPrefPath this does not require
        // Sharp Runtime to invent an organization/application identity (the old implementation
        // accidentally hardcoded one consumer's "speedyblupi" name for every application).
        const char* internalPath = SDL_GetAndroidInternalStoragePath();
        const std::filesystem::path root = internalPath != nullptr && *internalPath != '\0'
            ? std::filesystem::path(internalPath) / ".cna_isolated_storage"
            : std::filesystem::path("/data/local/tmp") / ".cna_isolated_storage";
#else
        const std::filesystem::path root = std::filesystem::current_path() / ".cna_isolated_storage";
#endif
        std::filesystem::create_directories(root);
        return root;
    }

    void StoragePaths::SetIsolatedStorageRootOverride(const std::filesystem::path& root)
    {
        const std::scoped_lock lock(rootOverrideMutex);
        if (root.empty())
        {
            rootOverride.reset();
        }
        else
        {
            rootOverride = root;
        }
    }
}
