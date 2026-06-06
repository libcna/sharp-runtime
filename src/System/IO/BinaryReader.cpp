#include "System/IO/BinaryReader.hpp"

#include <cstring>
#include <stdexcept>

namespace System::IO
{
    BinaryReader::BinaryReader(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen)
    {
        if (!stream_)
            throw std::invalid_argument("stream must not be null.");
    }

    BinaryReader::~BinaryReader()
    {
        if (!leaveOpen_ && stream_)
            stream_->Close();
    }

    void BinaryReader::ReadBytes(bytecs* buf, intcs count)
    {
        intcs total = 0;
        while (total < count)
        {
            intcs n = stream_->Read(buf, total, count - total);
            if (n == 0) throw std::runtime_error("Unexpected end of stream.");
            total += n;
        }
    }

    bytecs BinaryReader::ReadByte()
    {
        bytecs b;
        ReadBytes(&b, 1);
        return b;
    }

    int8_t BinaryReader::ReadSByte()
    {
        return static_cast<int8_t>(ReadByte());
    }

    shortcs BinaryReader::ReadInt16()
    {
        bytecs buf[2];
        ReadBytes(buf, 2);
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
        ReadBytes(buf, 4);
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

    std::string BinaryReader::ReadString()
    {
        // .NET BinaryWriter writes strings with a 7-bit-encoded length prefix.
        int length = 0;
        int shift = 0;
        while (true)
        {
            bytecs b = ReadByte();
            length |= (static_cast<int>(b & 0x7F)) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }
        if (length == 0) return {};
        std::string result(static_cast<std::size_t>(length), '\0');
        ReadBytes(reinterpret_cast<bytecs*>(result.data()), length);
        return result;
    }

    intcs BinaryReader::Read(bytecs buffer[], intcs offset, intcs count)
    {
        return stream_->Read(buffer, offset, count);
    }

    void BinaryReader::Close()
    {
        if (!leaveOpen_ && stream_)
            stream_->Close();
    }
}
