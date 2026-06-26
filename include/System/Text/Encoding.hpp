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
     * <summary>
     * Represents a character encoding.
     * 
     * Abstract base class; use UTF8(), ASCII(), or Unicode() factory methods
     * to obtain a concrete instance.
     * </summary>
     */
    class Encoding
    {
    public:
        virtual ~Encoding() = default;

        /** Returns a shared UTF-8 encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> UTF8();

        /** Returns a shared ASCII encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> ASCII();

        /** Returns a shared UTF-16 LE (Unicode) encoding instance. */
        [[nodiscard]] static std::shared_ptr<Encoding> Unicode();

        /**
         * Encodes a string to a byte vector using this encoding.
         * @param str The string to encode.
         * @return Encoded bytes.
         */
        [[nodiscard]] virtual std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const;

        /**
         * Decodes a range of bytes to a string using this encoding.
         * @param data  Pointer to the byte buffer.
         * @param index Start index within @p data.
         * @param count Number of bytes to decode.
         * @return Decoded string.
         */
        [[nodiscard]] virtual std::string GetString(
            const SharpRuntime::bytecs* data,
            SharpRuntime::intcs index,
            SharpRuntime::intcs count) const;

        /** Gets the IANA name of this encoding (e.g. "utf-8", "us-ascii"). */
        [[nodiscard]] virtual std::string getEncodingNameProperty() const { return "utf-8"; }

        /** Gets the code page identifier for this encoding (e.g. 65001 for UTF-8). */
        [[nodiscard]] virtual int getCodePageProperty() const { return 65001; }

    protected:
        Encoding() = default;
    };
}
