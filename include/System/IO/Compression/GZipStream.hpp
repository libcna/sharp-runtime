// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Stream.hpp"
#include "System/NotImplementedException.hpp"
#include "System/IO/Compression/CompressionMode.hpp"

// NOTE: Full implementation requires a zlib-compatible library (zlib, miniz, or zlib-ng).
// To implement:
//   1. Add zlib or miniz as a dependency.
//      - zlib:  find_package(ZLIB REQUIRED) + target_link_libraries(SHARP_RUNTIME ZLIB::ZLIB)
//      - miniz: drop miniz.h into the project (single-header, public domain / MIT)
//   2. Replace the throw statements in Read() and Write() with calls to:
//      - Decompress: inflateInit2(..., 16+MAX_WBITS) / inflate() / inflateEnd()  [gzip header]
//      - Compress:   deflateInit2(..., ..., 16+MAX_WBITS, ...) / deflate() / deflateEnd()
//   3. Add #include <zlib.h> (or "miniz.h") and keep state in z_stream members.

namespace System::IO::Compression {

    /**
     * @brief Provides methods for compressing and decompressing streams using the GZip format.
     *
     * Partial C++ counterpart of .NET System.IO.Compression.GZipStream.
     *
     * @note Status: Stub — Read/Write throw NotImplementedException.
     *   See the comment at the top of this file for what is needed to implement.
     */
    class GZipStream : public Stream {
        Stream*         inner_;
        CompressionMode mode_;
        bool            leaveOpen_;
    public:
        /**
         * @param stream    The stream to wrap for compress/decompress.
         * @param mode      CompressionMode::Compress or ::Decompress.
         * @param leaveOpen When true, inner stream is not closed on destruction.
         */
        GZipStream(Stream* stream, CompressionMode mode, bool leaveOpen = false)
            : inner_(stream), mode_(mode), leaveOpen_(leaveOpen) {}

        ~GZipStream() override {
            if (!leaveOpen_ && inner_) inner_->Close();
        }

        [[nodiscard]] bool getCanReadProperty()  const override { return mode_ == CompressionMode::Decompress; }
        [[nodiscard]] bool getCanWriteProperty() const override { return mode_ == CompressionMode::Compress; }

        SharpRuntime::intcs Read(SharpRuntime::bytecs* /*buffer*/,
                                 SharpRuntime::intcs  /*offset*/,
                                 SharpRuntime::intcs  /*count*/) override {
            throw NotImplementedException(
                "GZipStream::Read is not implemented. "
                "Requires zlib (inflate with 16+MAX_WBITS) or miniz. "
                "See include/System/IO/Compression/GZipStream.hpp for integration notes.");
        }

        void Write(const SharpRuntime::bytecs* /*buffer*/,
                   SharpRuntime::intcs         /*offset*/,
                   SharpRuntime::intcs         /*count*/) override {
            throw NotImplementedException(
                "GZipStream::Write is not implemented. "
                "Requires zlib (deflate with 16+MAX_WBITS) or miniz. "
                "See include/System/IO/Compression/GZipStream.hpp for integration notes.");
        }

        [[nodiscard]] SharpRuntime::intcs getLengthProperty() const override {
            throw NotImplementedException("GZipStream does not support seeking.");
        }

        void Flush() override {}

        void Close() override {
            if (!leaveOpen_ && inner_) { inner_->Close(); inner_ = nullptr; }
        }
    };

} // namespace System::IO::Compression
