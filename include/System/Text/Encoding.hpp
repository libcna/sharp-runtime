// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
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
     * Abstract base class; use UTF8() or ASCII() factory methods to obtain
     * a concrete instance.
     *
     * @note Status: PARTIAL
     */
    class Encoding
    {
    public:
        virtual ~Encoding() = default;

        /** @brief Gets a UTF-8 encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF8();

        /** @brief Gets an ASCII encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> ASCII();

        /** @brief Encodes a string to bytes. */
        [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const;

        /**
         * @brief Decodes a range of bytes to a string.
         *
         * @param data  Byte buffer.
         * @param index Start index in data.
         * @param count Number of bytes to decode.
         */
        [[nodiscard]] virtual std::string GetString(
            const SharpRuntime::bytecs* data,
            SharpRuntime::intcs index,
            SharpRuntime::intcs count) const;

        /** @brief Gets the name of the encoding (e.g. "utf-8", "us-ascii"). */
        [[nodiscard]] virtual std::string getEncodingNameProperty() const { return "utf-8"; }

    protected:
        Encoding() = default;
    };
}