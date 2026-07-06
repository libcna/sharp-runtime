// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/IOException.hpp"
#include "System/NotSupportedException.hpp"
#include <zlib.h>
#include <string>

namespace System::IO::Compression {

// ---------------------------------------------------------------------------
// Opaque zlib state (never exposed in the header)
// ---------------------------------------------------------------------------

struct ZlibZLibState {
    z_stream zs{};
    bool     initialized = false;
    bool     finished    = false;
    static constexpr int BUFSIZE = 65536;
    uint8_t  inbuf[BUFSIZE];
    uint8_t  outbuf[BUFSIZE];
};

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

ZLibStream::ZLibStream(Stream* stream, CompressionMode mode, bool leaveOpen)
    : inner_(stream), mode_(mode), leaveOpen_(leaveOpen),
      state_(std::make_unique<ZlibZLibState>())
{
    if (mode_ == CompressionMode::Decompress) {
        // Positive MAX_WBITS: zlib format, with 2-byte header + Adler-32 trailer.
        if (inflateInit2(&state_->zs, MAX_WBITS) != Z_OK)
            throw IOException("ZLibStream: inflateInit2 failed");
    } else {
        if (deflateInit2(&state_->zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            throw IOException("ZLibStream: deflateInit2 failed");
    }
    state_->initialized = true;
}

ZLibStream::~ZLibStream() { Close(); }

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

bool ZLibStream::getCanReadProperty()  const { return mode_ == CompressionMode::Decompress; }
bool ZLibStream::getCanWriteProperty() const { return mode_ == CompressionMode::Compress;   }

SharpRuntime::intcs ZLibStream::getLengthProperty() const {
    throw System::NotSupportedException("This operation is not supported.");
}

// ---------------------------------------------------------------------------
// Read (decompress)
// ---------------------------------------------------------------------------

SharpRuntime::intcs ZLibStream::Read(SharpRuntime::bytecs* buffer,
                                     SharpRuntime::intcs   offset,
                                     SharpRuntime::intcs   count)
{
    if (!state_ || !state_->initialized || state_->finished || count <= 0) return 0;

    auto& s = *state_;
    s.zs.next_out  = reinterpret_cast<Bytef*>(buffer + offset);
    s.zs.avail_out = static_cast<uInt>(count);

    while (s.zs.avail_out > 0 && !s.finished) {
        if (s.zs.avail_in == 0) {
            SharpRuntime::intcs n = inner_->Read(s.inbuf, 0, ZlibZLibState::BUFSIZE);
            if (n <= 0) break;
            s.zs.next_in  = s.inbuf;
            s.zs.avail_in = static_cast<uInt>(n);
        }
        int ret = inflate(&s.zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) { s.finished = true; break; }
        if (ret != Z_OK && ret != Z_BUF_ERROR)
            throw IOException("ZLibStream: inflate error " + std::to_string(ret));
    }
    return count - static_cast<SharpRuntime::intcs>(s.zs.avail_out);
}

// ---------------------------------------------------------------------------
// Write (compress)
// ---------------------------------------------------------------------------

void ZLibStream::Write(const SharpRuntime::bytecs* buffer,
                       SharpRuntime::intcs          offset,
                       SharpRuntime::intcs          count)
{
    if (!state_ || !state_->initialized || count <= 0) return;

    auto& s = *state_;
    s.zs.next_in  = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(buffer + offset));
    s.zs.avail_in = static_cast<uInt>(count);

    while (s.zs.avail_in > 0) {
        s.zs.next_out  = s.outbuf;
        s.zs.avail_out = ZlibZLibState::BUFSIZE;
        int ret = deflate(&s.zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_BUF_ERROR)
            throw IOException("ZLibStream: deflate error " + std::to_string(ret));
        SharpRuntime::intcs produced =
            ZlibZLibState::BUFSIZE - static_cast<SharpRuntime::intcs>(s.zs.avail_out);
        if (produced > 0)
            inner_->Write(s.outbuf, 0, produced);
    }
}

// ---------------------------------------------------------------------------
// Flush
// ---------------------------------------------------------------------------

void ZLibStream::Flush() {
    if (!state_ || !state_->initialized || mode_ != CompressionMode::Compress) return;

    auto& s = *state_;
    s.zs.next_in  = nullptr;
    s.zs.avail_in = 0;
    int ret;
    do {
        s.zs.next_out  = s.outbuf;
        s.zs.avail_out = ZlibZLibState::BUFSIZE;
        ret = deflate(&s.zs, Z_SYNC_FLUSH);
        SharpRuntime::intcs produced =
            ZlibZLibState::BUFSIZE - static_cast<SharpRuntime::intcs>(s.zs.avail_out);
        if (produced > 0 && inner_)
            inner_->Write(s.outbuf, 0, produced);
    } while (ret == Z_OK && s.zs.avail_out == 0);
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void ZLibStream::Close() {
    if (!state_ || !state_->initialized) {
        if (!leaveOpen_ && inner_) { inner_->Close(); inner_ = nullptr; }
        return;
    }
    auto& s = *state_;
    if (mode_ == CompressionMode::Compress) {
        s.zs.next_in  = nullptr;
        s.zs.avail_in = 0;
        int ret;
        do {
            s.zs.next_out  = s.outbuf;
            s.zs.avail_out = ZlibZLibState::BUFSIZE;
            ret = deflate(&s.zs, Z_FINISH);
            SharpRuntime::intcs produced =
                ZlibZLibState::BUFSIZE - static_cast<SharpRuntime::intcs>(s.zs.avail_out);
            if (produced > 0 && inner_)
                inner_->Write(s.outbuf, 0, produced);
        } while (ret == Z_OK);
        deflateEnd(&s.zs);
    } else {
        inflateEnd(&s.zs);
    }
    s.initialized = false;
    if (!leaveOpen_ && inner_) { inner_->Close(); inner_ = nullptr; }
}

} // namespace System::IO::Compression
