// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    /**
     * @brief Provides static methods for the creation, copying, deletion,
     * moving, and opening of files.
     *
     * Partial C++ counterpart of .NET System.IO.File.
     *
     * @note Status: Partial
     */
    class File {
    public:
        File() = delete;

        [[nodiscard]] static bool        Exists(const std::string& path);
        static void                      Delete(const std::string& path);
        static void                      Copy(const std::string& src, const std::string& dst, bool overwrite = false);
        static void                      Move(const std::string& src, const std::string& dst);

        [[nodiscard]] static std::string         ReadAllText(const std::string& path);
        static void                              WriteAllText(const std::string& path, const std::string& contents);
        [[nodiscard]] static std::vector<std::string> ReadAllLines(const std::string& path);
        static void                              WriteAllLines(const std::string& path,
                                                               const std::vector<std::string>& lines);

        [[nodiscard]] static std::vector<SharpRuntime::bytecs> ReadAllBytes(const std::string& path);
        static void WriteAllBytes(const std::string& path,
                                  const std::vector<SharpRuntime::bytecs>& bytes);

        static void AppendAllText(const std::string& path, const std::string& contents);
    };

} // namespace System::IO
