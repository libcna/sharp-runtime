// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <stdexcept>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO
{
    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

    /**
     * @brief Reads primitive data types from a Stream in binary, little-endian format.
     *
     * C++ counterpart of the .NET System.IO.BinaryReader class.
     */
    class BinaryReader
    {
    private:
        Stream* stream_;
        bool leaveOpen_;

        void ReadBytes(bytecs* buf, intcs count);

    public:
        explicit BinaryReader(Stream* stream, bool leaveOpen = false);
        virtual ~BinaryReader();

        /// Gets the underlying stream.
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /// Reads one byte and advances the stream position.
        [[nodiscard]] virtual bytecs ReadByte();

        /// Reads a signed byte.
        [[nodiscard]] virtual int8_t ReadSByte();

        /// Reads a 2-byte signed integer (little-endian).
        [[nodiscard]] virtual shortcs ReadInt16();

        /// Reads a 2-byte unsigned integer (little-endian).
        [[nodiscard]] virtual ushortcs ReadUInt16();

        /// Reads a 4-byte signed integer (little-endian).
        [[nodiscard]] virtual intcs ReadInt32();

        /// Reads a 4-byte unsigned integer (little-endian).
        [[nodiscard]] virtual uint32_t ReadUInt32();

        /// Reads an 8-byte signed integer (little-endian).
        [[nodiscard]] virtual longcs ReadInt64();

        /// Reads an 8-byte unsigned integer (little-endian).
        [[nodiscard]] virtual uint64_t ReadUInt64();

        /// Reads a 4-byte IEEE 754 float (little-endian).
        [[nodiscard]] virtual Single ReadSingle();

        /// Reads a 4-byte IEEE 754 double (little-endian).
        [[nodiscard]] virtual double ReadDouble();

        /// Reads a boolean (one byte; non-zero = true).
        [[nodiscard]] virtual bool ReadBoolean();

        /// Reads a UTF-8 string prefixed with its 7-bit encoded length.
        [[nodiscard]] virtual std::string ReadString();

        /// Reads exactly count bytes into the caller-supplied buffer.
        virtual intcs Read(bytecs buffer[], intcs offset, intcs count);

        /// Closes the reader and, unless leaveOpen was set, the underlying stream.
        virtual void Close();
    };
}
