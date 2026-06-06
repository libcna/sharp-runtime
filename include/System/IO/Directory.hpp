// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

namespace System::IO {

    /**
     * @brief Exposes static methods for creating, moving, and enumerating
     * through directories and subdirectories.
     *
     * Partial C++ counterpart of .NET System.IO.Directory.
     *
     * @note Status: Partial
     */
    class Directory {
    public:
        Directory() = delete;

        [[nodiscard]] static bool Exists(const std::string& path);
        static void               CreateDirectory(const std::string& path);
        static void               Delete(const std::string& path, bool recursive = false);
        static void               Move(const std::string& src, const std::string& dst);

        [[nodiscard]] static std::vector<std::string> GetFiles(const std::string& path);
        [[nodiscard]] static std::vector<std::string> GetFiles(const std::string& path,
                                                                const std::string& searchPattern);
        [[nodiscard]] static std::vector<std::string> GetDirectories(const std::string& path);

        [[nodiscard]] static std::string GetCurrentDirectory();
        static void                      SetCurrentDirectory(const std::string& path);
    };

} // namespace System::IO
