// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <vector>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

// System::Decimal requires unsigned __int128 (GCC/Clang only) and #error's out on MSVC -- guard
// ReadDecimal() the same way, rather than making the whole of BinaryReader MSVC-unsupported over
// one method. Matches sharp-runtime's own existing Decimal/Int128/UInt128 MSVC-unsupported policy.
#if !defined(_MSC_VER)
#include "System/Decimal.hpp"
#endif

namespace System::IO
{
    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::longcs;
    using SharpRuntime::charcs;
    using SharpRuntime::Single;

    /**
     * @brief Reads primitive data types from a Stream in binary, little-endian format.
     *
     * C++ counterpart of the .NET System.IO.BinaryReader class. This is a byte-oriented
     * subset: real .NET's BinaryReader accepts an arbitrary, pluggable Encoding; this port
     * always decodes UTF-8 (matching .NET's own default `BinaryReader(Stream)` constructor,
     * which uses UTF8Encoding — the only encoding real-world .NET/XNA content ever uses).
     * `PeekChar`, `ReadChars`, and `Read(char[])` are not implemented, since this codebase's
     * Stream has no general character-buffer decoding layer — a deliberate simplification,
     * not a silent gap.
     */
    class BinaryReader
    {
    private:
        Stream* stream_;
        bool leaveOpen_;
        bool disposed_ = false;

        void ThrowIfDisposed() const;
        void ReadBytesExact(bytecs* buf, intcs count);

    public:
        /** Initializes a new BinaryReader over @p stream, optionally leaving it open on Dispose. */
        explicit BinaryReader(Stream* stream, bool leaveOpen = false);
        virtual ~BinaryReader();

        /** Gets the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        /** Reads one byte and advances the stream position. @throws System::IO::EndOfStreamException at end of stream. */
        [[nodiscard]] virtual bytecs ReadByte();

        /** Reads a signed byte. */
        [[nodiscard]] virtual int8_t ReadSByte();

        /** Reads a 2-byte signed integer (little-endian). */
        [[nodiscard]] virtual shortcs ReadInt16();

        /** Reads a 2-byte unsigned integer (little-endian). */
        [[nodiscard]] virtual ushortcs ReadUInt16();

        /** Reads a 4-byte signed integer (little-endian). */
        [[nodiscard]] virtual intcs ReadInt32();

        /** Reads a 4-byte unsigned integer (little-endian). */
        [[nodiscard]] virtual uint32_t ReadUInt32();

        /** Reads an 8-byte signed integer (little-endian). */
        [[nodiscard]] virtual longcs ReadInt64();

        /** Reads an 8-byte unsigned integer (little-endian). */
        [[nodiscard]] virtual uint64_t ReadUInt64();

        /** Reads a 4-byte IEEE 754 float (little-endian). */
        [[nodiscard]] virtual Single ReadSingle();

        /** Reads a 4-byte IEEE 754 double (little-endian). */
        [[nodiscard]] virtual double ReadDouble();

        /** Reads a boolean (one byte; non-zero = true). */
        [[nodiscard]] virtual bool ReadBoolean();

        /**
         * @brief Reads one character, decoded from UTF-8, advancing the stream by however
         *        many bytes that character's encoding occupies.
         *
         * C++ counterpart of .NET `BinaryReader.ReadChar()` under its default UTF8Encoding.
         * A `System::Char`/`charcs` is a single UTF-16 code unit, so a codepoint above the
         * Basic Multilingual Plane (encoded as 4 UTF-8 bytes, requiring a surrogate pair)
         * cannot be returned by this method — matching real .NET, whose own `ReadChar()`
         * throws in that same case (decoding such a character produces two `char`s, which
         * does not fit the single-`char` output buffer `ReadChar()` decodes into).
         * @return The decoded character.
         * @throws System::IO::EndOfStreamException if the stream ends before a full character is read.
         * @throws System::FormatException if the bytes are not valid UTF-8, or encode a codepoint
         *         above U+FFFF.
         */
        [[nodiscard]] virtual charcs ReadChar();

        /**
         * @brief Reads a UTF-8 string prefixed with its 7-bit encoded length.
         *
         * For a seekable @ref stream_ (`getCanSeekProperty()` true), a declared length exceeding
         * the stream's own remaining bytes is rejected before the string buffer is allocated --
         * a corrupt or adversarial length prefix (up to ~2GB from a single 5-byte 7-bit-encoded
         * value) can otherwise trigger a huge allocation attempt before the truncation would
         * naturally be detected. Non-seekable streams (`Length`/`Position` not meaningful, e.g.
         * `NetworkStream`) are unaffected -- same behavior as before.
         *
         * @return The decoded string.
         * @throws System::IO::IOException if the decoded length is negative.
         * @throws System::IO::EndOfStreamException if the declared length exceeds the seekable
         *         stream's remaining length, or the stream ends before the full string is read.
         */
        [[nodiscard]] virtual std::string ReadString();

#if !defined(_MSC_VER)
        /**
         * @brief Reads a 16-byte .NET `System.Decimal` value.
         *
         * C++ counterpart of .NET `BinaryReader.ReadDecimal()`. Matches real .NET's actual
         * on-disk field order -- `lo`, `mid`, `hi`, `flags` (four little-endian `int32`s, per
         * `decimal.ToDecimal(ReadOnlySpan<byte>)` in the .NET reference source) -- which is
         * *not* the same order as `Decimal`'s own in-memory struct layout (`flags`, `hi`,
         * `lo64`). `flags`' bit 31 is the sign; bits 16-23 are the scale (0-28).
         *
         * @return The decoded Decimal value.
         * @note MSVC-unsupported: guarded out because `System::Decimal` itself requires
         *       `unsigned __int128` (GCC/Clang only) -- see this project's MSVC-unsupported policy.
         */
        [[nodiscard]] virtual System::Decimal ReadDecimal();
#endif

        /** Reads exactly count bytes into the caller-supplied buffer. */
        virtual intcs Read(bytecs buffer[], intcs offset, intcs count);

        /**
         * @brief Reads the specified number of bytes into a new array.
         *
         * C++ counterpart of .NET BinaryReader.ReadBytes(int).
         * If the end of the stream is reached before @p count bytes are read, the
         * returned vector is trimmed to the number of bytes actually read (no exception).
         *
         * For a seekable @ref stream_, the initial allocation is clamped to the stream's own
         * remaining length when that is smaller than @p count, since a seekable stream can never
         * actually deliver more bytes than that -- avoids attempting a huge allocation for a
         * corrupt or adversarial @p count (e.g. a `.xnb` reader that trusts an attacker-controlled
         * byte count) that could never be more than partially satisfied anyway. The final
         * (possibly trimmed) result is identical to the pre-existing behavior either way.
         *
         * @param count The number of bytes to read.
         * @return A vector containing the bytes read (may be shorter than @p count at EOF).
         * @throws System::ArgumentOutOfRangeException if @p count is negative.
         */
        [[nodiscard]] virtual std::vector<bytecs> ReadBytes(intcs count);

        /**
         * @brief Reads a 32-bit integer encoded 7 bits at a time (little-endian-first, high bit = continuation).
         *
         * C++ counterpart of .NET BinaryReader.Read7BitEncodedInt().
         * @return The decoded 32-bit integer.
         * @throws System::FormatException if the encoding uses more bytes than a 32-bit value requires.
         */
        intcs Read7BitEncodedInt();

        /** Closes the reader and, unless leaveOpen was set, the underlying stream. */
        virtual void Close();
    };
}
