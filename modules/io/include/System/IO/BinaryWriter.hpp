// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "System/Decimal.hpp"
#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;
    using SharpRuntime::charcs;

    /**
     * @brief Writes primitive types to a stream in binary, little-endian format.
     *
     * Partial C++ counterpart of .NET System.IO.BinaryWriter.
     *
     * @note Status: Implemented
     */
    class BinaryWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;
        bool    disposed_ = false;

        void ThrowIfDisposed() const;
        void WriteBytes(const bytecs* buf, intcs count);

    public:
        /**
         * @brief Constructs a BinaryWriter that writes to the given stream.
         * @throws System::ArgumentNullException if @p stream is null.
         * @throws System::ArgumentException if @p stream does not support writing.
         */
        explicit BinaryWriter(Stream* stream, bool leaveOpen = false);
        /** Destroys the BinaryWriter and closes the stream if not leaveOpen. */
        virtual ~BinaryWriter();

        /** Returns the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /** Writes an unsigned byte to the stream. */
        virtual void Write(bytecs value);
        /** Writes a signed byte to the stream. */
        virtual void Write(int8_t value);
        /** Writes a 16-bit signed integer to the stream. */
        virtual void Write(shortcs value);
        /** Writes a 16-bit unsigned integer to the stream. */
        virtual void Write(ushortcs value);
        /** Writes a 32-bit signed integer to the stream. */
        virtual void Write(intcs value);
        /** Writes a 32-bit unsigned integer to the stream. */
        virtual void Write(uint32_t value);
        /** Writes a 64-bit signed integer to the stream. */
        virtual void Write(longcs value);
        /** Writes a 64-bit unsigned integer to the stream. */
        virtual void Write(uint64_t value);
        /** Writes a 4-byte floating-point value to the stream. */
        virtual void Write(Single value);
        /** Writes an 8-byte floating-point value to the stream. */
        virtual void Write(double value);
        /** Writes a one-byte boolean value to the stream. */
        virtual void Write(bool value);
        /** Writes a length-prefixed string to the stream in UTF-8. */
        virtual void Write(const std::string& value);

        /** Writes a region of a byte array to the stream. */
        virtual void Write(const bytecs* buffer, intcs offset, intcs count);

        // The overloads below are not virtual: they add no vtable slot. Without them a C++ caller's
        // char16_t, string literal or array argument silently converted to Write(intcs) or
        // Write(bool) and wrote the wrong bytes.

        /**
         * @brief Writes a Unicode character to the stream, UTF-8 encoded.
         *
         * C++ counterpart of .NET BinaryWriter.Write(char): one character in its UTF-8 bytes.
         * @param ch The character; a lone surrogate cannot be encoded on its own.
         * @throws System::ArgumentException if @p ch is a surrogate (.NET
         *         Arg_SurrogatesNotAllowedAsSingleChar).
         */
        void Write(charcs ch);
        /**
         * @brief Writes a narrow character as the Unicode character of the same value.
         *
         * The C++ spelling of a C# character literal: `Write('A')` writes the one byte 0x41.
         * @param ch The character, read as an unsigned code point (U+0000..U+00FF).
         */
        void Write(char ch);
        /**
         * @brief Writes a length-prefixed string -- the C++ spelling of a C# string literal.
         * @param value A null-terminated UTF-8 string.
         * @throws System::ArgumentNullException if @p value is null.
         */
        void Write(const char* value);
        /**
         * @brief Writes a whole byte array to the stream. C++ counterpart of .NET Write(byte[]).
         * @param buffer The bytes to write.
         */
        void Write(const std::vector<bytecs>& buffer);
        /**
         * @brief Writes a whole byte array to the stream. C++ counterpart of .NET Write(byte[]).
         * @param buffer The bytes to write.
         */
        template<std::size_t N>
        void Write(const bytecs (&buffer)[N]) { Write(buffer, 0, static_cast<intcs>(N)); }
        /**
         * @brief Writes a character array to the stream, UTF-8 encoded.
         *
         * C++ counterpart of .NET BinaryWriter.Write(char[]). A surrogate pair is one character.
         * @param chars The characters to write.
         * @throws System::ArgumentException for a lone surrogate (.NET EncoderFallbackException).
         */
        void Write(const std::vector<charcs>& chars);
        /**
         * @brief Writes a region of a character array to the stream, UTF-8 encoded.
         *
         * C++ counterpart of .NET BinaryWriter.Write(char[], int, int).
         * @param chars The characters.
         * @param index The first character to write.
         * @param count How many characters to write.
         * @throws System::ArgumentNullException if @p chars is null.
         * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
         * @throws System::ArgumentException for a lone surrogate.
         */
        void Write(const charcs* chars, intcs index, intcs count);
        /**
         * @brief Writes a whole character array to the stream, UTF-8 encoded.
         * @param chars The characters to write.
         */
        template<std::size_t N>
        void Write(const charcs (&chars)[N]) { Write(chars, 0, static_cast<intcs>(N)); }
        /**
         * @brief Writes a decimal value as its 16 bytes: lo, mid, hi and flags, little-endian.
         *
         * C++ counterpart of .NET BinaryWriter.Write(decimal) (Decimal.GetBytes).
         * @param value The value to write.
         */
        void Write(const System::Decimal& value);

        /**
         * @brief Writes a 32-bit integer encoded 7 bits at a time (high bit = continuation).
         *
         * C++ counterpart of .NET BinaryWriter.Write7BitEncodedInt(int).
         */
        void Write7BitEncodedInt(intcs value);

        /** Clears all buffers and causes any buffered data to be written. */
        virtual void Flush();

        /**
         * @brief Closes the writer and, unless leaveOpen was set, the underlying stream.
         *
         * C++ counterpart of .NET BinaryWriter.Close(). Safe to call multiple times.
         */
        virtual void Close();
    };

} // namespace System::IO
