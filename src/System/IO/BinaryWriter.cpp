// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/BinaryWriter.hpp"

#include <cstring>

#include "System/ArgumentNullException.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::IO {

    BinaryWriter::BinaryWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen) {
        if (!stream_)
            throw System::ArgumentNullException("stream");
    }

    BinaryWriter::~BinaryWriter() {
        if (!disposed_ && !leaveOpen_ && stream_) stream_->Close();
    }

    void BinaryWriter::ThrowIfDisposed() const {
        if (disposed_)
            throw System::ObjectDisposedException("BinaryWriter", "Cannot access a closed file.");
    }

    void BinaryWriter::WriteBytes(const bytecs* buf, intcs count) {
        ThrowIfDisposed();
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
        Write7BitEncodedInt(static_cast<intcs>(value.size()));
        WriteBytes(reinterpret_cast<const bytecs*>(value.data()),
                   static_cast<intcs>(value.size()));
    }

    void BinaryWriter::Write7BitEncodedInt(intcs value) {
        uint32_t uValue = static_cast<uint32_t>(value);
        while (uValue > 0x7Fu) {
            Write(static_cast<bytecs>(uValue | ~0x7Fu));
            uValue >>= 7;
        }
        Write(static_cast<bytecs>(uValue));
    }

    void BinaryWriter::Write(const bytecs* buffer, intcs offset, intcs count) {
        ThrowIfDisposed();
        stream_->Write(buffer, offset, count);
    }

    void BinaryWriter::Flush() {
        ThrowIfDisposed();
        stream_->Flush();
    }

    void BinaryWriter::Close() {
        if (!disposed_) {
            if (!leaveOpen_ && stream_) stream_->Close();
            disposed_ = true;
        }
    }

} // namespace System::IO
