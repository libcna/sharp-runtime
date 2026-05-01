#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text
{
    /**
     * @brief Represents a character encoding.
     *
     * Minimal implementation supporting UTF-8 encoding.
     *
     * @note Status: PARTIAL
     */
    class Encoding
    {
    public:
        /**
         * @brief Gets UTF-8 encoding instance.
         *
         * @return Shared pointer to UTF-8 Encoding.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF8();

        /**
         * @brief Converts string to byte array (UTF-8).
         *
         * @param str Input string.
         * @return Byte vector.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const;

        /**
         * @brief Converts byte array to string (UTF-8).
         *
         * @param data Byte buffer.
         * @param index Start index.
         * @param count Number of bytes.
         * @return Decoded string.
         *
         * @note Status: IMPLEMENTED
         */
        [[nodiscard]] std::string GetString(
            const SharpRuntime::bytecs* data,
            SharpRuntime::intcs index,
            SharpRuntime::intcs count) const;

    private:
        Encoding() = default;
    };
}