// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

// Keep BinaryReader available when its one native-128-dependent method is unavailable.
#if SHARP_RUNTIME_HAS_NATIVE_INT128
#  include "System/Decimal.hpp"
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
     * always decodes UTF-8, exactly as .NET Framework 4.0's default `BinaryReader(Stream)`
     * does with `new UTF8Encoding()` -- the runtime Microsoft XNA 4.0 content is read by.
     * Every character API shares one stateful decoder that never flushes: an invalid sequence
     * becomes one U+FFFD, grouped as .NET Framework 4.0's UTF8Encoding groups it, and an
     * unfinished sequence waits for the next read. See `docs/Migration-BinaryReaderNet4Decoding.md`.
     */
    class BinaryReader
    {
    private:
        Stream* stream_;
        bool leaveOpen_;
        bool disposed_ = false;
        // .NET 4 UTF8Decoder.bits: the unfinished sequence, with its marker bits, or 0.
        std::int32_t decoderBits_ = 0;

        void ThrowIfDisposed() const;
        void ReadBytesExact(bytecs* buf, intcs count);
        [[nodiscard]] intcs DecodeUtf8(const bytecs* bytes, intcs byteCount, charcs* chars,
                                       intcs charCount);
        [[nodiscard]] intcs InternalReadOneChar();
        intcs InternalReadChars(charcs* buffer, intcs index, intcs count,
                                std::vector<charcs>* grow);

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
         * @brief Returns the next character without advancing a seekable stream.
         *
         * C++ counterpart of .NET Framework 4.0 `BinaryReader.PeekChar()`: it reads one
         * character as ReadChar() does and restores the stream position, but not the decoder,
         * whose state the read may have changed, exactly as .NET 4's does not. A stream whose
         * getCanSeekProperty() is false is not read at all, and the answer is -1.
         * @return The next UTF-16 character as an intcs; -1 at the end of the stream, including
         *         the end of an unfinished UTF-8 sequence, and -1 for a stream that cannot seek.
         * @throws System::ArgumentException if the character needs two UTF-16 code units, or an
         *         invalid sequence's U+FFFD and the next character both complete in one byte, as
         *         ReadChar(). The stream position is restored before throwing.
         */
        [[nodiscard]] virtual intcs PeekChar();

        /**
         * @brief Reads one character, decoded from UTF-8, one byte at a time until a character
         *        comes out.
         *
         * C++ counterpart of .NET Framework 4.0 `BinaryReader.ReadChar()` (InternalReadOneChar):
         * an invalid sequence reads as U+FFFD; the output is a single UTF-16 code unit, so a
         * scalar above the Basic Multilingual Plane, or a U+FFFD completed together with the
         * character of the byte that ended its sequence, does not fit and throws, after the
         * stream is sought back to where the call began (seekable streams only). The decoder's
         * state is what .NET 4 leaves in it.
         * @return The next decoded UTF-16 code unit.
         * @throws System::IO::EndOfStreamException if the stream ends before a character is
         *         complete; an unfinished sequence stays in the decoder.
         * @throws System::ArgumentException ("The output char buffer is too small...", parameter
         *         `chars`) when the decoded output needs two code units.
         */
        [[nodiscard]] virtual charcs ReadChar();

        /**
         * @brief Reads the next character, or answers -1 at the end of the stream.
         *
         * C++ counterpart of .NET Framework 4.0 `BinaryReader.Read()` (InternalReadOneChar), the
         * non-throwing sibling of ReadChar(). Not virtual: it adds no vtable slot.
         * @return The character's value, or -1 when no character is left.
         */
        [[nodiscard]] intcs Read();

        /**
         * @brief Reads up to @p count UTF-16 code units decoded from UTF-8 into a new array.
         *
         * C++ counterpart of .NET Framework 4.0 `BinaryReader.ReadChars` (InternalReadChars):
         * the vector is shortened at the end of the stream, an unfinished sequence there stays
         * in the decoder, and an invalid sequence reads as U+FFFD. The result grows with the
         * characters read rather than being allocated for @p count up front.
         *
         * @throws System::ArgumentOutOfRangeException if @p count is negative.
         * @throws System::ArgumentException when the last requested position would need two code
         *         units, as .NET 4's decoder refuses there; the bytes read stay consumed.
         */
        [[nodiscard]] virtual std::vector<charcs> ReadChars(intcs count);

        /**
         * @brief Reads up to @p count UTF-16 code units decoded from UTF-8 into @p buffer.
         *
         * Decodes as ReadChars(intcs) does. Returns a shorter count at the end of the stream.
         * The caller must provide storage for at least `offset + count` `charcs` elements; raw
         * C++ arrays do not carry their length.
         *
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         * @throws System::ArgumentException when the last requested position would need two code
         *         units, as ReadChars(intcs).
         */
        virtual intcs Read(charcs buffer[], intcs offset, intcs count);

        /**
         * @brief Reads a string prefixed with its 7-bit encoded byte length.
         *
         * C++ counterpart of .NET Framework 4.0 `BinaryReader.ReadString()`: the bytes are
         * decoded 128 at a time through the reader's decoder, so an invalid sequence becomes
         * U+FFFD (returned here as its UTF-8 encoding, EF BF BD) and an unfinished sequence at
         * the end stays in the decoder for the next read. Well-formed UTF-8 comes back
         * byte-identical. The result grows with the bytes actually read, never with the declared
         * length, so a corrupt length prefix cannot request a large allocation.
         *
         * @return The decoded string, as UTF-8.
         * @throws System::IO::IOException if the decoded length is negative.
         * @throws System::IO::EndOfStreamException if the stream ends before the declared
         *         length is read; the bytes read before that are consumed.
         */
        [[nodiscard]] virtual std::string ReadString();

#if SHARP_RUNTIME_HAS_NATIVE_INT128
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
         * @note Available when @c SHARP_RUNTIME_HAS_NATIVE_INT128 is 1.
         */
        [[nodiscard]] virtual System::Decimal ReadDecimal();
#endif // SHARP_RUNTIME_HAS_NATIVE_INT128

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
         * C++ counterpart of .NET Framework 4.0 BinaryReader.Read7BitEncodedInt(): a fifth byte
         * contributes only its low four bits; the rest are dropped without an error.
         * @return The decoded 32-bit integer.
         * @throws System::FormatException if the fifth byte still sets the continuation bit; no
         *         sixth byte is read.
         */
        intcs Read7BitEncodedInt();

        /** Closes the reader and, unless leaveOpen was set, the underlying stream. */
        virtual void Close();
    };
}
