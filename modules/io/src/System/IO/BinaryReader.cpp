// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/BinaryReader.hpp"

#include <array>
#include <cstring>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/IO/EndOfStreamException.hpp"
#include "System/IO/IOException.hpp"

// Character and string decoding follow .NET Framework 4.0 (mscorlib 4.0.30319.1,
// SHA256 5634668d4775b0113f08ea31093b281fea69bfc4e99227f5ca761b4ed98acc63), the runtime
// Microsoft XNA 4.0 Refresh content is read by. The IL offsets below are that assembly's:
// BinaryReader(Stream) IL_0002 uses `new UTF8Encoding()`, whose SetDefaultFallbacks
// (IL_001f-003a) installs DecoderReplacementFallback("�"), and every BinaryReader read
// decodes through the same stateful UTF8Decoder with flush = false.
namespace System::IO
{
    namespace
    {
        // MaxCharBytesSize: the byte chunk ReadString and InternalReadChars decode at a time.
        constexpr intcs MaxCharBytesSize = 128;

        // UTF8Encoding.InRange.
        [[nodiscard]] constexpr bool InRange(const std::int32_t ch, const std::int32_t start,
                                             const std::int32_t end) noexcept
        {
            return static_cast<std::uint32_t>(ch - start) <= static_cast<std::uint32_t>(end - start);
        }

        // Encoding.ThrowCharsOverflow() IL_0000-0033, for UTF8Encoding's name and fallback type.
        [[noreturn]] void ThrowCharsOverflow()
        {
            throw System::ArgumentException(
                "The output char buffer is too small to contain the decoded characters, encoding "
                "'Unicode (UTF-8)' fallback 'System.Text.DecoderReplacementFallback'.",
                "chars");
        }

        // Appends UTF-16 code units, which a UTF8Decoder always produces well formed, as UTF-8.
        void AppendUtf8(std::string& target, const charcs* chars, const intcs count)
        {
            for (intcs i = 0; i < count; ++i)
            {
                std::uint32_t scalar = chars[i];
                if (scalar >= 0xD800u && scalar <= 0xDBFFu && i + 1 < count)
                {
                    scalar = 0x10000u + ((scalar - 0xD800u) << 10) +
                             (static_cast<std::uint32_t>(chars[++i]) - 0xDC00u);
                }
                if (scalar < 0x80u)
                {
                    target.push_back(static_cast<char>(scalar));
                }
                else if (scalar < 0x800u)
                {
                    target.push_back(static_cast<char>(0xC0u | (scalar >> 6)));
                    target.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
                }
                else if (scalar < 0x10000u)
                {
                    target.push_back(static_cast<char>(0xE0u | (scalar >> 12)));
                    target.push_back(static_cast<char>(0x80u | ((scalar >> 6) & 0x3Fu)));
                    target.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
                }
                else
                {
                    target.push_back(static_cast<char>(0xF0u | (scalar >> 18)));
                    target.push_back(static_cast<char>(0x80u | ((scalar >> 12) & 0x3Fu)));
                    target.push_back(static_cast<char>(0x80u | ((scalar >> 6) & 0x3Fu)));
                    target.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
                }
            }
        }
    }

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

    intcs BinaryReader::DecodeUtf8(const bytecs* bytes, const intcs byteCount, charcs* chars,
                                   const intcs charCount)
    {
        // UTF8Encoding.GetChars(byte*, int, char*, int, DecoderNLS) IL_0000-05ea, the slow loop
        // only: its fast paths (IL_025c-0558) copy well-formed input and hand anything else
        // back to the slow loop at the lead byte (IL_0552-055a), so they decide nothing. `ch`
        // is the decoder's `bits`: a sequence's payload plus marker bits shifted along with it.
        constexpr std::int32_t FinalByte = 0x20000000;       // the sequence is complete
        constexpr std::int32_t SupplimentarySeq = 0x10000000; // four-byte sequence
        constexpr std::int32_t ThirdOfFour = 0x00800000;      // third byte of four is next

        const bytecs* source = bytes;
        const bytecs* const sourceEnd = bytes + byteCount;
        charcs* target = chars;
        charcs* const targetEnd = chars + charCount;
        std::int32_t ch = decoderBits_;

        const auto fold = [](const std::int32_t bits, const std::int32_t continuation) {
            return static_cast<std::int32_t>((static_cast<std::uint32_t>(bits) << 6) |
                                             static_cast<std::uint32_t>(continuation & 0x3F));
        };

        while (source < sourceEnd)
        {
            bool invalid = false;
            bool encode = false;
            if (ch != 0)
            {
                // IL_003a-00fb: a continuation byte is expected.
                const std::int32_t continuation = *source++;
                if ((continuation & 0xC0) != 0x80)
                {
                    // IL_004f-0054: put it back; it may begin the next sequence.
                    --source;
                    invalid = true;
                }
                else
                {
                    ch = fold(ch, continuation);
                    if ((ch & FinalByte) == 0)
                    {
                        if ((ch & SupplimentarySeq) != 0)
                        {
                            // IL_0079-0099: the second byte of four checks the plane range.
                            if ((ch & ThirdOfFour) == 0 && !InRange(ch & 0x1F0, 0x10, 0x100))
                                invalid = true;
                        }
                        else if ((ch & 0x3E0) == 0 || (ch & 0x3E0) == 0x360)
                        {
                            // IL_009b-00b7: non-shortest form, or an encoded surrogate.
                            invalid = true;
                        }
                        if (!invalid)
                            continue;
                    }
                    else
                    {
                        // IL_00b9-00fb: a supplementary scalar writes its high surrogate here
                        // when there is room, and its low surrogate through EncodeChar.
                        if ((ch & 0x101F0000) > SupplimentarySeq && target < targetEnd)
                        {
                            *target++ = static_cast<charcs>(((ch >> 10) & 0x7FF) + 0xD7C0);
                            ch = (ch & 0x3FF) + 0xDC00;
                        }
                        encode = true;
                    }
                }
            }
            else
            {
                // IL_015c-01e1: a new character.
                ch = *source++;
                if (ch <= 0x7F)
                {
                    encode = true;
                }
                else if ((ch & 0x40) == 0)
                {
                    invalid = true; // a continuation byte where a lead byte belongs
                }
                else if ((ch & 0x20) != 0)
                {
                    if ((ch & 0x10) != 0)
                    {
                        ch &= 0x0F;
                        if (ch > 4)
                        {
                            ch |= 0xF0; // F5..FF
                            invalid = true;
                        }
                        else
                        {
                            ch |= 0x504D0C00;
                            continue;
                        }
                    }
                    else
                    {
                        ch = (ch & 0x0F) | 0x48228000;
                        continue;
                    }
                }
                else
                {
                    ch &= 0x1F;
                    if (ch <= 1)
                    {
                        ch |= 0xC0; // C0, C1
                        invalid = true;
                    }
                    else
                    {
                        ch |= 0x800000;
                        continue;
                    }
                }
            }

            if (invalid)
            {
                // IL_0100-0157: FallbackInvalidByteSequence hands the whole sequence to
                // DecoderReplacementFallbackBuffer.Fallback, which yields one U+FFFD however many
                // bytes it held. No room fails InternalFallback (IL_005d-0067) and the call
                // throws through ThrowCharsOverflow (IL_0139-0147), keeping the decoder's bits.
                if (target >= targetEnd)
                    ThrowCharsOverflow();
                *target++ = static_cast<charcs>(0xFFFD);
                ch = 0;
                continue;
            }
            if (encode)
            {
                // IL_01e6-024d: EncodeChar; with no room the call throws (m_throwOnOverflow is
                // set by DecoderNLS.GetChars IL_005c-005e), keeping the decoder's bits.
                if (target >= targetEnd)
                    ThrowCharsOverflow();
                *target++ = static_cast<charcs>(ch);
                ch = 0;
            }
        }

        // IL_055f-05bc: flush is false, so an unfinished sequence stays in the decoder for the
        // next read rather than becoming U+FFFD (IL_05bf-05d0 stores it).
        decoderBits_ = ch;
        return static_cast<intcs>(target - chars);
    }

    intcs BinaryReader::InternalReadOneChar()
    {
        // BinaryReader.InternalReadOneChar IL_0000-00fe: one byte at a time into a one-char
        // buffer until a character comes out.
        ThrowIfDisposed();
        intcs savedPosition = 0;
        if (stream_->getCanSeekProperty())
            savedPosition = stream_->getPositionProperty();

        charcs single = 0;
        intcs charsRead = 0;
        while (charsRead == 0)
        {
            const intcs raw = stream_->ReadByte();
            if (raw == -1)
                return -1; // IL_009f-00a3: the decoder keeps any unfinished sequence
            const bytecs byte = static_cast<bytecs>(raw);
            try
            {
                charsRead = DecodeUtf8(&byte, 1, &single, 1);
            }
            catch (...)
            {
                // IL_00c1-00e9: back to where this read began, then rethrow. A supplementary
                // scalar needs two chars and lands here, as does U+FFFD followed by the
                // character of the byte that ended the invalid sequence.
                if (stream_->getCanSeekProperty())
                    stream_->Seek(savedPosition - stream_->getPositionProperty(), SeekOrigin::Current);
                throw;
            }
        }
        return static_cast<intcs>(single);
    }

    intcs BinaryReader::InternalReadChars(charcs* buffer, intcs index, const intcs count,
                                          std::vector<charcs>* grow)
    {
        // BinaryReader.InternalReadChars IL_0000-012d. A caller-owned buffer receives the
        // characters in place; ReadChars collects them in @p grow instead of allocating the
        // requested count up front, which decodes identically because no read can yield more
        // than one char per byte plus one for an unfinished sequence carried in.
        std::array<bytecs, MaxCharBytesSize> bytes{};
        std::array<charcs, MaxCharBytesSize + 1> scratch{};
        intcs charsRemaining = count;
        while (charsRemaining > 0)
        {
            intcs numBytes = charsRemaining;
            // IL_0028-0046: UTF8Decoder.HasState -- leave room for the carried-in sequence.
            if (decoderBits_ != 0 && numBytes > 1)
                numBytes -= 1;
            if (numBytes > MaxCharBytesSize)
                numBytes = MaxCharBytesSize;

            numBytes = stream_->Read(bytes.data(), 0, numBytes);
            if (numBytes == 0)
                return count - charsRemaining;

            intcs charsRead;
            if (grow == nullptr)
            {
                charsRead = DecodeUtf8(bytes.data(), numBytes, buffer + index, charsRemaining);
            }
            else
            {
                const intcs capacity = charsRemaining < numBytes + 1 ? charsRemaining : numBytes + 1;
                charsRead = DecodeUtf8(bytes.data(), numBytes, scratch.data(), capacity);
                grow->insert(grow->end(), scratch.begin(), scratch.begin() + charsRead);
            }
            charsRemaining -= charsRead;
            index += charsRead;
        }
        return count - charsRemaining;
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

        // BinaryReader.PeekChar IL_001c-003c: read one character and restore the position. The
        // decoder state is not restored, exactly as .NET 4's is not; a failing read has already
        // sought back (InternalReadOneChar IL_00c1-00e9).
        const intcs originalPosition = stream_->getPositionProperty();
        const intcs character = InternalReadOneChar();
        stream_->setPositionProperty(originalPosition);
        return character;
    }

    charcs BinaryReader::ReadChar()
    {
        // BinaryReader.ReadChar IL_0000-0012.
        const intcs character = InternalReadOneChar();
        if (character == -1)
            throw System::IO::EndOfStreamException();
        return static_cast<charcs>(character);
    }

    std::vector<charcs> BinaryReader::ReadChars(intcs count)
    {
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        ThrowIfDisposed();

        std::vector<charcs> result;
        if (count == 0)
            return result;
        result.reserve(static_cast<std::size_t>(count < 256 ? count : 256));
        (void)InternalReadChars(nullptr, 0, count, &result);
        return result;
    }

    intcs BinaryReader::Read(charcs buffer[], intcs offset, intcs count)
    {
        if (buffer == nullptr)
            throw System::ArgumentNullException("buffer");
        System::ArgumentOutOfRangeException::ThrowIfNegative(offset, "offset");
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        ThrowIfDisposed();
        return InternalReadChars(buffer, offset, count, nullptr);
    }

    intcs BinaryReader::Read7BitEncodedInt()
    {
        // BinaryReader.Read7BitEncodedInt IL_0000-003a: the shift is masked to five bits and
        // the result is an Int32, so a fifth byte keeps only its low four bits; only a fifth
        // byte that still sets the continuation bit is refused, before a sixth is read.
        std::uint32_t count = 0;
        int shift = 0;
        bytecs byteReadJustNow;
        do
        {
            if (shift == 5 * 7)
                throw System::FormatException(
                    "Too many bytes in what should have been a 7 bit encoded Int32.");
            byteReadJustNow = ReadByte();
            count |= (static_cast<std::uint32_t>(byteReadJustNow) & 0x7Fu) << (shift & 31);
            shift += 7;
        } while ((byteReadJustNow & 0x80u) != 0);
        return static_cast<intcs>(count);
    }

    std::string BinaryReader::ReadString()
    {
        // BinaryReader.ReadString IL_0000-010d: the 7-bit length counts bytes, decoded 128 at a
        // time through the reader's decoder.
        ThrowIfDisposed();
        const intcs stringLength = Read7BitEncodedInt();
        if (stringLength < 0)
            throw System::IO::IOException(
                "BinaryReader encountered an invalid string length of " +
                std::to_string(stringLength) + " characters.");
        if (stringLength == 0)
            return {};

        // The result grows with the bytes actually read, never with the declared length, so a
        // corrupt prefix cannot request a large allocation.
        std::string result;
        std::array<bytecs, MaxCharBytesSize> bytes{};
        std::array<charcs, MaxCharBytesSize + 1> chars{};
        intcs currPos = 0;
        do
        {
            const intcs readLength =
                stringLength - currPos > MaxCharBytesSize ? MaxCharBytesSize : stringLength - currPos;
            const intcs n = stream_->Read(bytes.data(), 0, readLength);
            if (n == 0)
                throw System::IO::EndOfStreamException();
            // m_charBuffer holds GetMaxCharCount(128) = 129 chars, more than 128 bytes plus one
            // carried-in sequence can produce, so this decode never overflows.
            const intcs charsRead =
                DecodeUtf8(bytes.data(), n, chars.data(), static_cast<intcs>(chars.size()));
            AppendUtf8(result, chars.data(), charsRead);
            currPos += n;
        } while (currPos < stringLength);
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
