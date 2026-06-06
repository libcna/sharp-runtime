// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/BinaryWriter.hpp"

#include <cstring>

namespace System::IO {

    BinaryWriter::BinaryWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen) {}

    BinaryWriter::~BinaryWriter() {
        if (!leaveOpen_ && stream_) stream_->Close();
    }

    void BinaryWriter::WriteBytes(const bytecs* buf, intcs count) {
        stream_->Write(buf, 0, count);
    }

    void BinaryWriter::Write(bytecs value)   { WriteBytes(&value, 1); }
    void BinaryWriter::Write(int8_t value)   { WriteBytes(reinterpret_cast<const bytecs*>(&value), 1); }

    void BinaryWriter::Write(shortcs value) {
        bytecs buf[2];
        buf[0] = static_cast<bytecs>(value & 0xFF);
        buf[1] = static_cast<bytecs>((value >> 8) & 0xFF);
        WriteBytes(buf, 2);
    }
    void BinaryWriter::Write(ushortcs value) { Write(static_cast<shortcs>(value)); }

    void BinaryWriter::Write(intcs value) {
        bytecs buf[4];
        buf[0] = static_cast<bytecs>( value        & 0xFF);
        buf[1] = static_cast<bytecs>((value >>  8) & 0xFF);
        buf[2] = static_cast<bytecs>((value >> 16) & 0xFF);
        buf[3] = static_cast<bytecs>((value >> 24) & 0xFF);
        WriteBytes(buf, 4);
    }
    void BinaryWriter::Write(uint32_t value) { Write(static_cast<intcs>(value)); }

    void BinaryWriter::Write(longcs value) {
        bytecs buf[8];
        for (int i = 0; i < 8; ++i)
            buf[i] = static_cast<bytecs>((value >> (i * 8)) & 0xFF);
        WriteBytes(buf, 8);
    }
    void BinaryWriter::Write(uint64_t value) { Write(static_cast<longcs>(value)); }

    void BinaryWriter::Write(Single value) {
        bytecs buf[4];
        std::memcpy(buf, &value, 4);
        WriteBytes(buf, 4);
    }
    void BinaryWriter::Write(double value) {
        bytecs buf[8];
        std::memcpy(buf, &value, 8);
        WriteBytes(buf, 8);
    }

    void BinaryWriter::Write(bool value) {
        bytecs b = value ? 1 : 0;
        WriteBytes(&b, 1);
    }

    void BinaryWriter::Write(const std::string& value) {
        // 7-bit encoded length (matching BinaryReader::ReadString)
        uint32_t len = static_cast<uint32_t>(value.size());
        while (len >= 0x80) {
            bytecs b = static_cast<bytecs>((len & 0x7F) | 0x80);
            WriteBytes(&b, 1);
            len >>= 7;
        }
        bytecs b = static_cast<bytecs>(len);
        WriteBytes(&b, 1);
        WriteBytes(reinterpret_cast<const bytecs*>(value.data()),
                   static_cast<intcs>(value.size()));
    }

    void BinaryWriter::Write(const bytecs* buffer, intcs offset, intcs count) {
        stream_->Write(buffer, offset, count);
    }

    void BinaryWriter::Flush() { stream_->Flush(); }
    void BinaryWriter::Close() { stream_->Close(); }

} // namespace System::IO
