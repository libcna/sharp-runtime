// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// BinaryReader decodes as .NET Framework 4.0 (mscorlib 4.0.30319.1, SHA256
// 5634668d4775b0113f08ea31093b281fea69bfc4e99227f5ca761b4ed98acc63), the runtime Microsoft XNA 4.0
// content is read by. Every expectation below is derived by hand from that assembly's IL:
// BinaryReader.Read7BitEncodedInt IL_0000-003a; ReadString IL_0000-010d; InternalReadOneChar
// IL_0000-00fe; InternalReadChars IL_0000-012d; PeekChar IL_0000-003c; and
// UTF8Encoding.GetChars(byte*, int, char*, int, DecoderNLS) IL_0000-05ea with
// FallbackInvalidByteSequence, DecoderReplacementFallbackBuffer.Fallback (one U+FFFD per
// invalid sequence) and Encoding.ThrowCharsOverflow. See docs/Migration-BinaryReaderNet4Decoding.md.

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/EndOfStreamException.hpp"
#include "System/IO/MemoryStream.hpp"

using System::IO::BinaryReader;
using System::IO::MemoryStream;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    const std::string Fffd = "\xEF\xBF\xBD";

    std::string Repeat(const std::string& text, const int count)
    {
        std::string result;
        for (int i = 0; i < count; ++i) result += text;
        return result;
    }

    /// One length-prefixed string per element of @p strings, each spelled byte by byte.
    Bytes Strings(const std::vector<Bytes>& strings)
    {
        Bytes stream;
        for (const Bytes& bytes : strings)
        {
            std::uint32_t length = static_cast<std::uint32_t>(bytes.size());
            while (length >= 0x80u)
            {
                stream.push_back(static_cast<std::uint8_t>(length | 0x80u));
                length >>= 7;
            }
            stream.push_back(static_cast<std::uint8_t>(length));
            stream.insert(stream.end(), bytes.begin(), bytes.end());
        }
        return stream;
    }

    std::string DecodeOneString(const Bytes& bytes)
    {
        const Bytes stream = Strings({bytes});
        MemoryStream ms(stream.data(), static_cast<SharpRuntime::intcs>(stream.size()));
        BinaryReader reader(&ms, true);
        return reader.ReadString();
    }

    std::int32_t Decode7Bit(const Bytes& bytes)
    {
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        return reader.Read7BitEncodedInt();
    }
}

TEST(BinaryReaderNet4DecodingTest, AFifthSevenBitByteKeepsOnlyItsLowFourBits)
{
    // IL_0020-002b: (b & 0x7F) << (shift & 31) into an Int32, so bits shifted past 31 vanish.
    EXPECT_EQ(Decode7Bit({0x81, 0x80, 0x80, 0x80, 0x70}), 1);
    EXPECT_EQ(Decode7Bit({0xFF, 0xFF, 0xFF, 0xFF, 0x7F}), -1);
    EXPECT_EQ(Decode7Bit({0x80, 0x80, 0x80, 0x80, 0x1F}), static_cast<std::int32_t>(0xF0000000u));
    EXPECT_EQ(Decode7Bit({0x80, 0x80, 0x80, 0x80, 0x10}), 0);
    // The four-bit encodings that were already accepted are unchanged.
    EXPECT_EQ(Decode7Bit({0xFF, 0xFF, 0xFF, 0xFF, 0x0F}), -1);
    EXPECT_EQ(Decode7Bit({0x80, 0x80, 0x80, 0x80, 0x08}), static_cast<std::int32_t>(0x80000000u));
}

TEST(BinaryReaderNet4DecodingTest, AFifthByteWithTheContinuationBitIsRefusedBeforeASixthIsRead)
{
    // IL_0004-0018: the shift reaches 35 only after a fifth continuation byte.
    const Bytes bytes{0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
    BinaryReader reader(&ms, true);
    EXPECT_THROW((void)reader.Read7BitEncodedInt(), System::FormatException);
    EXPECT_EQ(ms.getPositionProperty(), 5);
}

TEST(BinaryReaderNet4DecodingTest, AStringLengthMayUseTheTruncatedFifthByte)
{
    Bytes stream{0x81, 0x80, 0x80, 0x80, 0x70, 'Z'};
    MemoryStream ms(stream.data(), static_cast<SharpRuntime::intcs>(stream.size()));
    BinaryReader reader(&ms, true);
    EXPECT_EQ(reader.ReadString(), "Z");
}

TEST(BinaryReaderNet4DecodingTest, EachInvalidSequenceBecomesOneReplacementCharacterAsNet4GroupsIt)
{
    // Lead bytes that can never start a sequence: one U+FFFD each (IL_016b-0170, IL_0180-0196,
    // IL_01bc-01d2).
    EXPECT_EQ(DecodeOneString({'A', 0xFF, 'B'}), "A" + Fffd + "B");
    EXPECT_EQ(DecodeOneString({0x80}), Fffd);
    EXPECT_EQ(DecodeOneString({0xC0, 0x80}), Fffd + Fffd);
    EXPECT_EQ(DecodeOneString({0xF5, 0x80, 0x80, 0x80}), Repeat(Fffd, 4));
    // A second byte that makes the sequence non-shortest, a surrogate or out of range is consumed
    // into the one U+FFFD (IL_0079-00b7); the bytes after it are then lone continuations. Modern
    // .NET's maximal-subpart rule gives three U+FFFD for the first two of these.
    EXPECT_EQ(DecodeOneString({0xE0, 0x80, 0x80}), Repeat(Fffd, 2));
    EXPECT_EQ(DecodeOneString({0xED, 0xA0, 0x80}), Repeat(Fffd, 2));
    EXPECT_EQ(DecodeOneString({0xF0, 0x80, 0x80, 0x80}), Repeat(Fffd, 3));
    EXPECT_EQ(DecodeOneString({0xF4, 0x90, 0x80, 0x80}), Repeat(Fffd, 3));
    // A byte that is not a continuation ends the pending sequence as one U+FFFD and is read again
    // (IL_004f-0054).
    EXPECT_EQ(DecodeOneString({0xC3, 'A'}), Fffd + "A");
    EXPECT_EQ(DecodeOneString({0xE2, 0x82, 'A'}), Fffd + "A");
    EXPECT_EQ(DecodeOneString({0xF0, 0x9F, 0x98, 'A'}), Fffd + "A");
    EXPECT_EQ(DecodeOneString({0xC3, 0xC3, 0xA9}), Fffd + "\xC3\xA9");
}

TEST(BinaryReaderNet4DecodingTest, WellFormedUtf8IsByteIdentical)
{
    const Bytes text{'a', 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80, 0xF4, 0x8F, 0xBF, 0xBF};
    EXPECT_EQ(DecodeOneString(text), std::string(text.begin(), text.end()));
}

TEST(BinaryReaderNet4DecodingTest, AnUnfinishedSequenceWaitsInTheDecoderForTheNextString)
{
    // GetChars is called with flush = false (DecoderNLS.GetChars IL_0054-005e), so the pending
    // bits survive the string (IL_05bf-05d0).
    const Bytes stream = Strings({{'A', 0xC3}, {0xA9}, {0xE2}, {'B'}});
    MemoryStream ms(stream.data(), static_cast<SharpRuntime::intcs>(stream.size()));
    BinaryReader reader(&ms, true);
    EXPECT_EQ(reader.ReadString(), "A");
    EXPECT_EQ(reader.ReadString(), "\xC3\xA9");
    EXPECT_EQ(reader.ReadString(), "");
    EXPECT_EQ(reader.ReadString(), Fffd + "B");
}

TEST(BinaryReaderNet4DecodingTest, ASequenceAcrossTheHundredTwentyEightByteChunkDecodesWhole)
{
    Bytes text(127, 'a');
    text.insert(text.end(), {0xF0, 0x9F, 0x98, 0x80, 0xFF});
    EXPECT_EQ(DecodeOneString(text), std::string(127, 'a') + "\xF0\x9F\x98\x80" + Fffd);
}

TEST(BinaryReaderNet4DecodingTest, ReadCharReplacesAnInvalidLeadByte)
{
    const Bytes bytes{0xFF, 0xE0, 0x80, 'Z'};
    MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
    BinaryReader reader(&ms, true);
    EXPECT_EQ(reader.ReadChar(), static_cast<char16_t>(0xFFFD));
    EXPECT_EQ(ms.getPositionProperty(), 1);
    // E0 80 is non-shortest at its second byte: one U+FFFD, both bytes consumed.
    EXPECT_EQ(reader.ReadChar(), static_cast<char16_t>(0xFFFD));
    EXPECT_EQ(ms.getPositionProperty(), 3);
    EXPECT_EQ(reader.ReadChar(), u'Z');
}

TEST(BinaryReaderNet4DecodingTest, ReadCharKeepsALeadByteThatEndsAnInvalidSequence)
{
    // C3 C3 A9: the second C3 ends the first as U+FFFD and starts the next character.
    const Bytes bytes{0xC3, 0xC3, 0xA9};
    MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
    BinaryReader reader(&ms, true);
    EXPECT_EQ(reader.ReadChar(), static_cast<char16_t>(0xFFFD));
    EXPECT_EQ(reader.ReadChar(), static_cast<char16_t>(0x00E9));
    EXPECT_EQ(ms.getPositionProperty(), 3);
}

TEST(BinaryReaderNet4DecodingTest, ReadCharRefusesTwoCodeUnitsAndSeeksBack)
{
    // A one-char buffer (IL_0045-004c): U+FFFD followed by 'A', or a surrogate pair, overflows
    // (UTF8Encoding.GetChars IL_01e6-0245) and InternalReadOneChar seeks back (IL_00c1-00e9).
    for (const Bytes& bytes : {Bytes{0xC3, 'A'}, Bytes{0xF0, 0x9F, 0x98, 0x80}})
    {
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_THROW((void)reader.ReadChar(), System::ArgumentException);
        EXPECT_EQ(ms.getPositionProperty(), 0);
    }
}

TEST(BinaryReaderNet4DecodingTest, ReadCharsDecodesInvalidInputAndTrimsAnUnfinishedEnd)
{
    {
        const Bytes bytes{'A', 0xFF, 'B'};
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_EQ(reader.ReadChars(4), (std::vector<char16_t>{u'A', 0xFFFD, u'B'}));
    }
    {
        // InternalReadChars returns what it has once Read returns nothing (IL_00b5-00bb).
        const Bytes bytes{'A', 0xC3};
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_EQ(reader.ReadChars(2), (std::vector<char16_t>{u'A'}));
        EXPECT_EQ(ms.getPositionProperty(), 2);
    }
    {
        // Two bytes for two chars, U+FFFD and 'A'; 'B' is left unread.
        const Bytes bytes{0xC3, 'A', 'B'};
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_EQ(reader.ReadChars(2), (std::vector<char16_t>{0xFFFD, u'A'}));
        EXPECT_EQ(ms.getPositionProperty(), 2);
    }
    {
        // One requested char cannot hold a surrogate pair.
        const Bytes bytes{0xF0, 0x9F, 0x98, 0x80};
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_THROW((void)reader.ReadChars(1), System::ArgumentException);
    }
}

TEST(BinaryReaderNet4DecodingTest, ReadCharsLeavesRoomForACarriedInSequence)
{
    // InternalReadChars IL_0028-0046: with a sequence pending, one byte fewer than the chars still
    // wanted is read, because that sequence may end as a U+FFFD of its own. Reading three bytes
    // here would decode four chars into three.
    const Bytes bytes{0xE2, 0x82, 0xAC, 0xE2, 'A', 'B', 'C'};
    MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
    BinaryReader reader(&ms, true);
    EXPECT_EQ(reader.ReadChars(4), (std::vector<char16_t>{0x20AC, 0xFFFD, u'A', u'B'}));
    EXPECT_EQ(ms.getPositionProperty(), 6);
}

TEST(BinaryReaderNet4DecodingTest, AStringLongerThanTheStreamReadsWhatIsThereThenEnds)
{
    // ReadString IL_007b-00a7: the bytes that are there are read and decoded before the empty
    // Read throws; nothing is refused up front.
    const Bytes stream{0x05, 'a', 'b'};
    MemoryStream ms(stream.data(), static_cast<SharpRuntime::intcs>(stream.size()));
    BinaryReader reader(&ms, true);
    EXPECT_THROW((void)reader.ReadString(), System::IO::EndOfStreamException);
    EXPECT_EQ(ms.getPositionProperty(), 3);
}

TEST(BinaryReaderNet4DecodingTest, PeekCharRestoresThePositionButNotTheDecoder)
{
    {
        const Bytes bytes{0xFF};
        MemoryStream ms(bytes.data(), static_cast<SharpRuntime::intcs>(bytes.size()));
        BinaryReader reader(&ms, true);
        EXPECT_EQ(reader.PeekChar(), 0xFFFD);
        EXPECT_EQ(ms.getPositionProperty(), 0);
    }
    {
        // C3 at the end: no character, -1, and the C3 stays in the decoder, so the C3 read again
        // after the restored position is a non-continuation that ends it as U+FFFD.
        const Bytes bytes{0xC3, 0xA9};
        MemoryStream ms(bytes.data(), 1);
        BinaryReader reader(&ms, true);
        EXPECT_EQ(reader.PeekChar(), -1);
        EXPECT_EQ(ms.getPositionProperty(), 0);
        EXPECT_EQ(reader.ReadChar(), static_cast<char16_t>(0xFFFD));
    }
}

TEST(BinaryReaderNet4DecodingTest, TheDecoderStateKeepsTheReaderLayout)
{
    // The pending-character pair became one Int32 of decoder bits, in the same slot.
    struct ShadowBefore
    {
        virtual ~ShadowBefore() = default;
        void* stream;
        bool leaveOpen;
        bool disposed;
        bool hasPending;
        char16_t pending;
    };
    struct ShadowAfter
    {
        virtual ~ShadowAfter() = default;
        void* stream;
        bool leaveOpen;
        bool disposed;
        std::int32_t decoderBits;
    };
    EXPECT_EQ(sizeof(BinaryReader), sizeof(ShadowAfter));
    EXPECT_EQ(alignof(BinaryReader), alignof(ShadowAfter));
    EXPECT_EQ(sizeof(ShadowAfter), sizeof(ShadowBefore));
}
