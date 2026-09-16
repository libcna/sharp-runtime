// SPDX-License-Identifier: MIT
#pragma once

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/**
 * @file Utf8Path.hpp
 * @brief Implementation-private conversion between a path string and a native filesystem path.
 *
 * .NET's `FileStream`, `File` and `Directory` take a UTF-16 `String`. Their narrow `std::string`
 * counterparts here mean **UTF-8**, and this is the one place that is turned into a
 * `std::filesystem::path`.
 *
 * It matters because `std::filesystem::path`'s narrow constructor, `std::fstream`'s
 * `const std::string&` overload and every `std::filesystem` function that takes one convert
 * through the process **ANSI code page** on Windows. A path holding any character that code page
 * cannot spell then names a different file, or no file at all. On POSIX the same call is a byte
 * copy, which is why this has never shown up on a Linux build.
 *
 * Not a public header: it lives under `src/`, not `include/`, because it is not part of the
 * `System.IO` surface.
 */
namespace System::IO::Detail
{
    /**
     * @brief Converts UTF-8 path text to a native filesystem path.
     *
     * @param value Path text encoded as UTF-8.
     * @return The native path.
     * @throws std::filesystem::filesystem_error On Windows, when @p value is not valid UTF-8.
     */
    [[nodiscard]] inline std::filesystem::path NativePath(std::string_view value)
    {
        return std::filesystem::path(
            std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
    }

    /**
     * @brief Converts UTF-8 path text to a native filesystem path without throwing.
     *
     * For callers that must answer rather than propagate — a `bool`-returning existence check, or
     * a query whose contract is a value rather than an exception.
     *
     * @param value Path text encoded as UTF-8.
     * @return The native path, or an empty optional when the text cannot name one here.
     */
    [[nodiscard]] inline std::optional<std::filesystem::path> TryNativePath(std::string_view value)
    {
        try
        {
            return NativePath(value);
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }

    /**
     * @brief Converts a native filesystem path to UTF-8 text.
     *
     * @param path The native path.
     * @return The path as UTF-8.
     */
    [[nodiscard]] inline std::string Utf8Of(const std::filesystem::path& path)
    {
        const std::u8string value = path.u8string();
        return {reinterpret_cast<const char*>(value.data()), value.size()};
    }
}
