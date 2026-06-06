// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

namespace System::IO {

    /**
     * @brief Performs operations on String instances that contain file or
     * directory path information.
     *
     * Partial C++ counterpart of .NET System.IO.Path.
     *
     * @note Status: Implemented
     */
    class Path {
    public:
        Path() = delete;

#ifdef _WIN32
        static inline const char DirectorySeparatorChar     = '\\';
        static inline const char AltDirectorySeparatorChar  = '/';
        static inline const char PathSeparator              = ';';
#else
        static inline const char DirectorySeparatorChar     = '/';
        static inline const char AltDirectorySeparatorChar  = '/';
        static inline const char PathSeparator              = ':';
#endif

        [[nodiscard]] static std::string Combine(const std::string& path1, const std::string& path2);
        [[nodiscard]] static std::string Combine(const std::string& path1, const std::string& path2,
                                                  const std::string& path3);

        [[nodiscard]] static std::string GetFileName(const std::string& path);
        [[nodiscard]] static std::string GetFileNameWithoutExtension(const std::string& path);
        [[nodiscard]] static std::string GetExtension(const std::string& path);
        [[nodiscard]] static std::string GetDirectoryName(const std::string& path);
        [[nodiscard]] static std::string GetFullPath(const std::string& path);
        [[nodiscard]] static std::string GetTempPath();
        [[nodiscard]] static std::string GetTempFileName();
        [[nodiscard]] static std::string ChangeExtension(const std::string& path,
                                                          const std::string& extension);

        [[nodiscard]] static bool HasExtension(const std::string& path);
        [[nodiscard]] static bool IsPathRooted(const std::string& path);
    };

} // namespace System::IO
