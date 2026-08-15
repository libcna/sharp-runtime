// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/BinaryReader.hpp"

#include <cstring>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/IO/EndOfStreamException.hpp"
#include "System/IO/IOException.hpp"

namespace System::IO
{
    BinaryReader::BinaryReader(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen)
    {
        if (!stream_)
            throw System::ArgumentNullException("stream");
        if (!stream_->getCanReadProperty())
            throw System::ArgumentException("Stream was not readable.");
    }

    BinaryReader::~BinaryReader()
    {
        if (!disposed_ && !leaveOpen_ && stream_)
            stream_->Close();
    }

    void BinaryReader::ThrowIfDisposed() const
    {
        if (disposed_)
            throw System::ObjectDisposedException("BinaryReader", "Cannot access a closed file.");
    }

    void BinaryReader::ReadBytesExact(bytecs* buf, intcs count)
    {
        ThrowIfDisposed();
        intcs total = 0;
        while (total < count)
        {
            intcs n = stream_->Read(buf, total, count - total);
            if (n == 0) throw System::IO::EndOfStreamException();
            total += n;
        }
    }

    bool BinaryReader::TryReadChar(charcs& character)
    {
        ThrowIfDisposed();

        if (hasPendingCharacter_)
        {
            character = pendingCharacter_;
            hasPendingCharacter_ = false;
            return true;
        }

        const intcs rawLead = stream_->ReadByte();
        if (rawLead < 0)
            return false;

        const bytecs lead = static_cast<bytecs>(rawLead);
        uint32_t codepoint;
        uint32_t minimumCodepoint;
        int continuationBytes;
        if ((lead & 0x80u) == 0x00u)
        {
            codepoint = lead;
            minimumCodepoint = 0;
            continuationBytes = 0;
        }
        else if (lead >= 0xC2u && lead <= 0xDFu)
        {
            codepoint = lead & 0x1Fu;
            minimumCodepoint = 0x80u;
            continuationBytes = 1;
        }
        else if (lead >= 0xE0u && lead <= 0xEFu)
        {
            codepoint = lead & 0x0Fu;
            minimumCodepoint = 0x800u;
            continuationBytes = 2;
        }
        else if (lead >= 0xF0u && lead <= 0xF4u)
        {
            codepoint = lead & 0x07u;
            minimumCodepoint = 0x10000u;
            continuationBytes = 3;
        }
        else
        {
            throw System::FormatException("Invalid UTF-8 lead byte while reading a char.");
        }

        for (int i = 0; i < continuationBytes; ++i)
        {
            const intcs rawContinuation = stream_->ReadByte();
            if (rawContinuation < 0)
                throw System::IO::EndOfStreamException();

            const bytecs continuation = static_cast<bytecs>(rawContinuation);
            if ((continuation & 0xC0u) != 0x80u)
                throw System::FormatException("Invalid UTF-8 continuation byte while reading a char.");

            codepoint = (codepoint << 6) | (continuation & 0x3Fu);
        }

        if (codepoint < minimumCodepoint || codepoint > 0x10FFFFu ||
            (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
        {
            throw System::FormatException("Invalid UTF-8 scalar value while reading a char.");
        }

        if (codepoint <= 0xFFFFu)
        {
            character = static_cast<charcs>(codepoint);
            return true;
        }

        const uint32_t scalarOffset = codepoint - 0x10000u;
        character = static_cast<charcs>(0xD800u + (scalarOffset >> 10));
        pendingCharacter_ = static_cast<charcs>(0xDC00u + (scalarOffset & 0x3FFu));
        hasPendingCharacter_ = true;
        return true;
    }

    bytecs BinaryReader::ReadByte()
    {
        bytecs b;
        ReadBytesExact(&b, 1);
        return b;
    }

    int8_t BinaryReader::ReadSByte()
    {
        return static_cast<int8_t>(ReadByte());
    }

    shortcs BinaryReader::ReadInt16()
    {
        bytecs buf[2];
        ReadBytesExact(buf, 2);
        return static_cast<shortcs>(
            static_cast<uint16_t>(buf[0]) |
            (static_cast<uint16_t>(buf[1]) << 8));
    }

    ushortcs BinaryReader::ReadUInt16()
    {
        return static_cast<ushortcs>(ReadInt16());
    }

    intcs BinaryReader::ReadInt32()
    {
        bytecs buf[4];
        ReadBytesExact(buf, 4);
        return static_cast<intcs>(
            static_cast<uint32_t>(buf[0])        |
            (static_cast<uint32_t>(buf[1]) << 8) |
            (static_cast<uint32_t>(buf[2]) << 16)|
            (static_cast<uint32_t>(buf[3]) << 24));
    }

    uint32_t BinaryReader::ReadUInt32()
    {
        return static_cast<uint32_t>(ReadInt32());
    }

    longcs BinaryReader::ReadInt64()
    {
        uint64_t lo = ReadUInt32();
        uint64_t hi = ReadUInt32();
        return static_cast<longcs>((hi << 32) | lo);
    }

    uint64_t BinaryReader::ReadUInt64()
    {
        return static_cast<uint64_t>(ReadInt64());
    }

    Single BinaryReader::ReadSingle()
    {
        uint32_t raw = ReadUInt32();
        Single result;
        std::memcpy(&result, &raw, sizeof(Single));
        return result;
    }

    double BinaryReader::ReadDouble()
    {
        uint64_t raw = ReadUInt64();
        double result;
        std::memcpy(&result, &raw, sizeof(double));
        return result;
    }

    bool BinaryReader::ReadBoolean()
    {
        return ReadByte() != 0;
    }

    intcs BinaryReader::PeekChar()
    {
        ThrowIfDisposed();
        if (!stream_->getCanSeekProperty())
            throw System::NotSupportedException(
                "BinaryReader::PeekChar requires a seekable stream in this implementation.");

        const intcs originalPosition = stream_->getPositionProperty();
        const bool hadPendingCharacter = hasPendingCharacter_;
        const charcs originalPendingCharacter = pendingCharacter_;
        if (hadPendingCharacter)
            return static_cast<intcs>(originalPendingCharacter);
        if (originalPosition >= stream_->getLengthProperty())
            return -1;

        try
        {
            const charcs character = ReadChar();
            stream_->setPositionProperty(originalPosition);
            hasPendingCharacter_ = hadPendingCharacter;
            pendingCharacter_ = originalPendingCharacter;
            return static_cast<intcs>(character);
        }
        catch (...)
        {
            stream_->setPositionProperty(originalPosition);
            hasPendingCharacter_ = hadPendingCharacter;
            pendingCharacter_ = originalPendingCharacter;
            throw;
        }
    }

    charcs BinaryReader::ReadChar()
    {
        charcs character;
        if (!TryReadChar(character))
            throw System::IO::EndOfStreamException();
        return character;
    }

    std::vector<charcs> BinaryReader::ReadChars(intcs count)
    {
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        ThrowIfDisposed();

        std::vector<charcs> result;
        if (count == 0)
            return result;

        // Grow incrementally rather than trusting a caller-controlled count for one large
        // allocation. This preserves the requested partial-read behavior on non-seekable streams.
        result.reserve(static_cast<std::size_t>(count < 256 ? count : 256));
        while (static_cast<intcs>(result.size()) < count)
        {
            charcs character;
            if (!TryReadChar(character))
                break;
            result.push_back(character);
        }
        return result;
    }

    intcs BinaryReader::Read(charcs buffer[], intcs offset, intcs count)
    {
        if (buffer == nullptr)
            throw System::ArgumentNullException("buffer");
        System::ArgumentOutOfRangeException::ThrowIfNegative(offset, "offset");
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        ThrowIfDisposed();

        intcs total = 0;
        while (total < count)
        {
            charcs character;
            if (!TryReadChar(character))
                break;
            buffer[offset + total] = character;
            ++total;
        }
        return total;
    }

    intcs BinaryReader::Read7BitEncodedInt()
    {
        // Unlike writing, we can't delegate to the 64-bit read: we want to stop
        // consuming bytes as soon as an overflow is detected, matching .NET exactly.
        uint32_t result = 0;
        bytecs byteReadJustNow;

        constexpr int MaxBytesWithoutOverflow = 4;
        for (int shift = 0; shift < MaxBytesWithoutOverflow * 7; shift += 7)
        {
            byteReadJustNow = ReadByte();
            result |= (static_cast<uint32_t>(byteReadJustNow) & 0x7Fu) << shift;

            if (byteReadJustNow <= 0x7Fu)
                return static_cast<intcs>(result);
        }

        // The 5th byte: we've already read 28 bits, so this byte must fit in 4 bits
        // and must not have the continuation bit set.
        byteReadJustNow = ReadByte();
        if (byteReadJustNow > 0b1111u)
            throw System::FormatException("Too many bytes in what should have been a 7-bit encoded integer.");

        result |= static_cast<uint32_t>(byteReadJustNow) << (MaxBytesWithoutOverflow * 7);
        return static_cast<intcs>(result);
    }

    std::string BinaryReader::ReadString()
    {
        // .NET BinaryWriter writes strings with a 7-bit-encoded length prefix.
        intcs length = Read7BitEncodedInt();
        if (length < 0)
            throw System::IO::IOException("Invalid string length.");
        if (length == 0) return {};

        if (stream_->getCanSeekProperty())
        {
            const intcs remaining = stream_->getLengthProperty() - stream_->getPositionProperty();
            if (remaining >= 0 && length > remaining)
                throw System::IO::EndOfStreamException(
                    "BinaryReader::ReadString(): declared string length exceeds the remaining stream length.");
        }

        std::string result(static_cast<std::size_t>(length), '\0');
        ReadBytesExact(reinterpret_cast<bytecs*>(result.data()), length);
        return result;
    }

    intcs BinaryReader::Read(bytecs buffer[], intcs offset, intcs count)
    {
        ThrowIfDisposed();
        System::ArgumentOutOfRangeException::ThrowIfNegative(offset, "offset");
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        return stream_->Read(buffer, offset, count);
    }

    std::vector<bytecs> BinaryReader::ReadBytes(intcs count)
    {
        ThrowIfDisposed();
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        if (count == 0) return {};

        // A seekable stream can never deliver more bytes than its own remaining length -- clamp
        // the eager allocation to that bound instead of the raw (possibly adversarial) `count`,
        // so a corrupt/attacker-controlled count can't force a huge allocation attempt for bytes
        // that could never be read anyway. The read loop below and the final trim still produce
        // the exact same result this would have without the clamp.
        intcs allocateCount = count;
        if (stream_->getCanSeekProperty())
        {
            const intcs remaining = stream_->getLengthProperty() - stream_->getPositionProperty();
            if (remaining >= 0 && remaining < allocateCount)
                allocateCount = remaining;
        }

        std::vector<bytecs> result(static_cast<std::size_t>(allocateCount));
        intcs total = 0;
        while (total < allocateCount)
        {
            intcs n = stream_->Read(result.data(), total, allocateCount - total);
            if (n == 0) break; // .NET trims the result instead of throwing here.
            total += n;
        }
        if (total != count) result.resize(static_cast<std::size_t>(total));
        return result;
    }

    void BinaryReader::Close()
    {
        if (!disposed_)
        {
            if (!leaveOpen_ && stream_)
                stream_->Close();
            disposed_ = true;
        }
    }

#if SHARP_RUNTIME_HAS_NATIVE_INT128
    System::Decimal BinaryReader::ReadDecimal()
    {
        const int32_t lo = ReadInt32();
        const int32_t mid = ReadInt32();
        const int32_t hi = ReadInt32();
        const int32_t flags = ReadInt32();

        const bool isNegative = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        const auto scale = static_cast<bytecs>((static_cast<uint32_t>(flags) >> 16) & 0xFFu);

        return System::Decimal(lo, mid, hi, isNegative, scale);
    }
#endif // SHARP_RUNTIME_HAS_NATIVE_INT128
}
