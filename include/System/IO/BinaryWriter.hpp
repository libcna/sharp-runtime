// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

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

        void WriteBytes(const bytecs* buf, intcs count);

    public:
        /// Constructs a BinaryWriter that writes to the given stream.
        explicit BinaryWriter(Stream* stream, bool leaveOpen = false);
        /// Destroys the BinaryWriter and closes the stream if not leaveOpen.
        virtual ~BinaryWriter();

        /// Returns the underlying stream.
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /// Writes an unsigned byte to the stream.
        virtual void Write(bytecs value);
        /// Writes a signed byte to the stream.
        virtual void Write(int8_t value);
        /// Writes a 16-bit signed integer to the stream.
        virtual void Write(shortcs value);
        /// Writes a 16-bit unsigned integer to the stream.
        virtual void Write(ushortcs value);
        /// Writes a 32-bit signed integer to the stream.
        virtual void Write(intcs value);
        /// Writes a 32-bit unsigned integer to the stream.
        virtual void Write(uint32_t value);
        /// Writes a 64-bit signed integer to the stream.
        virtual void Write(longcs value);
        /// Writes a 64-bit unsigned integer to the stream.
        virtual void Write(uint64_t value);
        /// Writes a 4-byte floating-point value to the stream.
        virtual void Write(Single value);
        /// Writes an 8-byte floating-point value to the stream.
        virtual void Write(double value);
        /// Writes a one-byte boolean value to the stream.
        virtual void Write(bool value);
        /// Writes a length-prefixed string to the stream in UTF-8.
        virtual void Write(const std::string& value);

        /// Writes a region of a byte array to the stream.
        virtual void Write(const bytecs* buffer, intcs offset, intcs count);

        /// Clears all buffers and causes any buffered data to be written.
        virtual void Flush();
        /// Closes the writer and the underlying stream.
        virtual void Close();
    };

} // namespace System::IO
