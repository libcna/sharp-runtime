// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/BinaryWriter.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::IO {

    BinaryWriter::BinaryWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen) {
        if (!stream_)
            throw System::ArgumentNullException("stream");
        if (!stream_->getCanWriteProperty())
            throw System::ArgumentException("Stream was not writable.");
    }

    // Verified against BinaryWriter.cs's Dispose(bool): real .NET calls OutStream.Flush()
    // when leaveOpen is true, and OutStream.Close() otherwise -- leaveOpen=true does NOT mean
    // "do nothing," it means "flush what's pending, but don't take ownership of closing it."
    // This previously did nothing at all when leaveOpen_ was true, silently leaving any data
    // buffered by the underlying stream (e.g. a BufferedStream) unflushed.
    BinaryWriter::~BinaryWriter() {
        if (!disposed_ && stream_) {
            if (leaveOpen_) stream_->Flush();
            else stream_->Close();
        }
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
        // Extracts the raw IEEE-754 bit pattern into a same-width, same-endianness integer (a
        // pure same-host memcpy, not a wire-format concern), then serializes it via the already
        // explicit-little-endian Write(uint32_t) - symmetric with BinaryReader::ReadSingle(),
        // which reconstructs via ReadUInt32() before reinterpreting the bits as a float. Fixes an
        // asymmetry where this previously did a raw host-native-order memcpy straight to the wire
        // buffer: byte-identical to the fix on every little-endian host (x86/x64/ARM), but would
        // have written big-endian bytes on a big-endian host while ReadSingle() always assumes
        // little-endian on the way back in - a real, if currently unreachable, latent bug.
        uint32_t raw;
        std::memcpy(&raw, &value, sizeof(raw));
        Write(raw);
    }
    void BinaryWriter::Write(double value) {
        // Same reasoning as Write(Single) above, mirroring BinaryReader::ReadDouble().
        uint64_t raw;
        std::memcpy(&raw, &value, sizeof(raw));
        Write(raw);
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

    namespace {
        // Appends one Unicode scalar value's UTF-8 bytes.
        void AppendUtf8(std::vector<bytecs>& out, char32_t cp) {
            if (cp < 0x80) {
                out.push_back(static_cast<bytecs>(cp));
            } else if (cp < 0x800) {
                out.push_back(static_cast<bytecs>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<bytecs>(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                out.push_back(static_cast<bytecs>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<bytecs>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<bytecs>(0x80 | (cp & 0x3F)));
            } else {
                out.push_back(static_cast<bytecs>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<bytecs>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<bytecs>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<bytecs>(0x80 | (cp & 0x3F)));
            }
        }

        bool IsSurrogate(charcs ch) { return ch >= 0xD800 && ch <= 0xDFFF; }
    }

    void BinaryWriter::Write(charcs ch) {
        // .NET Framework 4 BinaryWriter.Write(char) refuses a surrogate on its own.
        if (IsSurrogate(ch))
            throw System::ArgumentException(
                "Unicode surrogate characters must be written out as pairs together in the same "
                "call, not individually. Consider passing in a character array instead.");
        std::vector<bytecs> bytes;
        AppendUtf8(bytes, ch);
        WriteBytes(bytes.data(), static_cast<intcs>(bytes.size()));
    }

    void BinaryWriter::Write(char ch) {
        Write(static_cast<charcs>(static_cast<unsigned char>(ch)));
    }

    void BinaryWriter::Write(const char* value) {
        if (value == nullptr)
            throw System::ArgumentNullException("value");
        Write(std::string(value));
    }

    void BinaryWriter::Write(const std::vector<bytecs>& buffer) {
        if (!buffer.empty())
            Write(buffer.data(), 0, static_cast<intcs>(buffer.size()));
        else
            ThrowIfDisposed();
    }

    void BinaryWriter::Write(const std::vector<charcs>& chars) {
        if (!chars.empty())
            Write(chars.data(), 0, static_cast<intcs>(chars.size()));
        else
            ThrowIfDisposed();
    }

    void BinaryWriter::Write(const charcs* chars, intcs index, intcs count) {
        if (chars == nullptr)
            throw System::ArgumentNullException("chars");
        System::ArgumentOutOfRangeException::ThrowIfNegative(index, "index");
        System::ArgumentOutOfRangeException::ThrowIfNegative(count, "count");
        // UTF8Encoding(false, true) -- .NET Framework 4 BinaryWriter(Stream)'s encoder -- throws for a
        // lone surrogate (EncoderFallbackException, an ArgumentException) before anything is written.
        std::vector<bytecs> bytes;
        bytes.reserve(static_cast<std::size_t>(count));
        for (intcs i = 0; i < count; ++i) {
            const charcs ch = chars[index + i];
            if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < count &&
                chars[index + i + 1] >= 0xDC00 && chars[index + i + 1] <= 0xDFFF) {
                const char32_t cp = 0x10000 + ((static_cast<char32_t>(ch) - 0xD800) << 10) +
                                    (static_cast<char32_t>(chars[index + i + 1]) - 0xDC00);
                AppendUtf8(bytes, cp);
                ++i;
                continue;
            }
            if (IsSurrogate(ch))
                throw System::ArgumentException(
                    "Unable to translate Unicode character at index " + std::to_string(i) +
                    " to specified code page.");
            AppendUtf8(bytes, ch);
        }
        WriteBytes(bytes.data(), static_cast<intcs>(bytes.size()));
    }

#if SHARP_RUNTIME_HAS_NATIVE_INT128
    void BinaryWriter::Write(const System::Decimal& value) {
        intcs lo = 0, mid = 0, hi = 0, flags = 0;
        System::Decimal::GetBits(value, lo, mid, hi, flags);
        Write(lo);
        Write(mid);
        Write(hi);
        Write(flags);
    }
#endif // SHARP_RUNTIME_HAS_NATIVE_INT128

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
            if (stream_) {
                if (leaveOpen_) stream_->Flush();
                else stream_->Close();
            }
            disposed_ = true;
        }
    }

} // namespace System::IO
