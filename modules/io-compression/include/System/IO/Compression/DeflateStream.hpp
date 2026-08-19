// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Stream.hpp"
#include "System/IO/Compression/CompressionMode.hpp"
#include "System/IO/Compression/ZLibCompressionOptions.hpp"
#include <memory>

// Note: .NET DeflateStream uses raw DEFLATE (no gzip wrapper), unlike GZipStream.
// XNB content files compressed with older XNA profiles use raw DEFLATE.

namespace System::IO::Compression {

    struct ZlibDeflateState; ///< Opaque zlib state; defined in DeflateStream.cpp.

    /**
     * @brief Provides methods for compressing and decompressing streams using raw DEFLATE format.
     *
     * Unlike GZipStream there is no gzip header/trailer — the byte stream is pure
     * DEFLATE as specified in RFC 1951.  This is what .NET DeflateStream produces and
     * what older XNB content files use.
     *
     * Partial C++ counterpart of .NET System.IO.Compression.DeflateStream.
     *
     * @note Status: IMPLEMENTED — requires system zlib (linked via ZLIB::ZLIB CMake target).
     */
    class DeflateStream : public Stream {
        Stream*                           inner_;
        CompressionMode                   mode_;
        bool                              leaveOpen_;
        std::unique_ptr<ZlibDeflateState> state_;

    public:
        /**
         * @brief Constructs a DeflateStream wrapping @p stream.
         *
         * A null @p stream is rejected here rather than carried, matching .NET,
         * whose every Stream-taking constructor of this type begins with
         * `ArgumentNullException.ThrowIfNull(stream)`. It is checked before zlib
         * is initialised, so a rejected call allocates no compressor state.
         *
         * @param stream    The underlying stream for compressed data. Must not be null.
         * @param mode      @c CompressionMode::Compress or @c ::Decompress. A value outside
         *                  those two members is rejected here (ticket #2148); it used to
         *                  construct a deflater that `Close()` then released with
         *                  `inflateEnd`, leaking the whole zlib state.
         * @param leaveOpen When @c true the inner stream is not closed on destruction.
         *
         * @throws System::ArgumentNullException if @p stream is null.
         * @throws System::ArgumentException if @p mode is neither @c Compress nor @c Decompress.
         * @throws System::IO::IOException if zlib initialisation fails.
         */
        DeflateStream(Stream* stream, CompressionMode mode, bool leaveOpen = false);

        /**
         * @brief Constructs a compressing DeflateStream using the given compression options.
         *
         * C++ counterpart of .NET's `DeflateStream(Stream, ZLibCompressionOptions, bool)`. **This
         * constructor implies `CompressionMode::Compress`** — .NET's own comment above the
         * `CompressionLevel` overloads says *"Implies mode = Compress"*, and the options
         * overload has no mode parameter for the same reason: every option it carries describes
         * compression.
         *
         * The options are honoured in full: `CompressionLevel` becomes zlib's level,
         * `CompressionStrategy` its strategy, and `WindowLog` its window size, resolved for
         * raw deflate (no header or trailer) by `Detail::ResolveWindowBits`. `memLevel` follows .NET's rule — 7 at
         * quality 0, otherwise 8.
         *
         * @param stream    The stream to which compressed data is written.
         * @param options   The compression options. Validated by `ZLibCompressionOptions`'
         *                  own setters, so an out-of-range value cannot reach here.
         * @param leaveOpen When @c true the inner stream is not closed on destruction.
         *
         * @throws System::ArgumentNullException if @p stream is null.
         * @throws System::IO::IOException if zlib initialisation fails.
         *
         * @note Ticket **#2150**. Adding this overload cannot change the meaning of any existing
         * call: `CompressionMode` is a scoped enumeration and `ZLibCompressionOptions` has no
         * converting constructor, so no argument can bind to both this and the
         * `(Stream*, CompressionMode, bool)` overload. That is asserted, not assumed, by
         * `CompressionOptionsConstructorTests.Decl2150_TheAdditionCannotRebindAnExistingCall`.
         */
        DeflateStream(Stream* stream, const ZLibCompressionOptions& options, bool leaveOpen = false);

        ~DeflateStream() override;

        /** @brief Returns @c true when mode is Decompress. */
        [[nodiscard]] bool getCanReadProperty()  const override;

        /** @brief Returns @c true when mode is Compress. */
        [[nodiscard]] bool getCanWriteProperty() const override;

        /** @brief Not supported — always throws NotSupportedException. */
        [[nodiscard]] SharpRuntime::intcs getLengthProperty() const override;

        /**
         * @brief Decompresses bytes from the inner stream into @p buffer.
         *
         * Only valid when mode is @c CompressionMode::Decompress.
         *
         * @param buffer Destination array.
         * @param offset Starting index in @p buffer.
         * @param count  Maximum number of bytes to read.
         * @return Number of bytes written; 0 when the compressed stream is exhausted.
         * @throws System::ArgumentNullException if @p buffer is null.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         * @throws System::ObjectDisposedException if @c Close() has already run. The buffer
         *         arguments are validated first, matching .NET's
         *         `ValidateBufferArguments`-then-`EnsureNotDisposed` order.
         */
        SharpRuntime::intcs Read(SharpRuntime::bytecs* buffer,
                                 SharpRuntime::intcs   offset,
                                 SharpRuntime::intcs   count) override;

        /**
         * @brief Compresses @p count bytes from @p buffer and writes to the inner stream.
         *
         * Only valid when mode is @c CompressionMode::Compress.
         *
         * @param buffer Source array.
         * @param offset Starting index in @p buffer.
         * @param count  Number of bytes to compress.
         * @throws System::ArgumentNullException if buffer is null.
         * @throws System::ArgumentOutOfRangeException if offset or count is negative.
         * @throws System::ObjectDisposedException if @c Close() has already run. Before ticket
         *         #2148 this returned silently and the caller's bytes were discarded.
         */
        void Write(const SharpRuntime::bytecs* buffer,
                   SharpRuntime::intcs          offset,
                   SharpRuntime::intcs          count) override;

        /**
         * @brief Flushes any pending compressed data using @c Z_SYNC_FLUSH.
         *
         * No-op in Decompress mode.
         *
         * @throws System::ObjectDisposedException if @c Close() has already run.
         */
        void Flush() override;

        /**
         * @brief Finalises compression (@c Z_FINISH), writes remaining output, then
         *        optionally closes the inner stream.
         *
         * Safe to call multiple times.
         */
        void Close() override;
    };

} // namespace System::IO::Compression
