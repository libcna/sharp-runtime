// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Compression/DeflateEncoder.hpp"

namespace System::IO::Compression {

    /**
     * @brief Provides methods to encode data in a streamless, non-allocating, and performant
     * manner using the GZip data format specification.
     *
     * C++ counterpart of .NET System.IO.Compression.GZipEncoder. Internally composes a
     * DeflateEncoder configured for GZip framing (gzip header/trailer).
     *
     * @note Status: IMPLEMENTED — requires system zlib (linked via ZLIB::ZLIB CMake target).
     */
    class GZipEncoder {
    private:
        DeflateEncoder deflateEncoder_;
        bool disposed_ = false;

    public:
        /** Initializes a new GZipEncoder using the default quality. */
        GZipEncoder();
        /** Initializes a new GZipEncoder using the specified quality (0-9, or -1 for default). */
        explicit GZipEncoder(intcs quality);
        /** Initializes a new GZipEncoder using the specified quality and window size. */
        GZipEncoder(intcs quality, intcs windowLog);
        /** Initializes a new GZipEncoder using the specified options. */
        explicit GZipEncoder(const ZLibCompressionOptions& options);

        /** Frees and disposes unmanaged (zlib) resources. */
        void Dispose();

        /** Returns the maximum expected compressed length for the given input size. */
        static longcs GetMaxCompressedLength(longcs inputLength);

        /**
         * @brief Compresses @p source into @p destination.
         * @throws System::ObjectDisposedException if this encoder has been disposed.
         *
         * **Raw-pointer length and buffer contract** (ticket #2146, SR-AUD-256; ticket #2151
         * documents it here). .NET expresses this surface in `ReadOnlySpan<byte>`/`Span<byte>`;
         * this port replaced every span with a pointer plus a signed `intcs` length, so a length
         * a span could never hold is now expressible. Every raw-pointer door in this component
         * therefore validates before zlib sees anything:
         *
         * - a **negative** length is rejected -- it used to be cast to `uInt` and become an
         *   enormous byte count that ran off the caller's allocation;
         * - a **null** buffer with a **positive** length is rejected;
         * - a **null** buffer with a length of **zero** is ACCEPTED, deliberately, because
         *   `default(ReadOnlySpan<byte>)` is an empty span whose reference is null and
         *   compressing or decompressing nothing is legal;
         * - validation runs **before** the out-parameters are written, so a rejected call leaves
         *   `bytesConsumed`/`bytesWritten` exactly as the caller left them rather than looking
         *   like a successful empty operation.
         *
         * @throws System::ArgumentOutOfRangeException if a length is negative
         *         (`"sourceLength"` / `"destinationLength"`).
         * @throws System::ArgumentNullException if a buffer is null with a positive length
         *         (`"source"` / `"destination"`).
         */
        OperationStatus Compress(const bytecs* source, intcs sourceLength,
                                 bytecs* destination, intcs destinationLength,
                                 intcs& bytesConsumed, intcs& bytesWritten, bool isFinalBlock);

        /**
         * @brief Flushes any pending output, ensuring output is produced for all processed input so far.
         *
         * Subject to the same destination length and buffer contract as `Compress` above
         * (ticket #2146): a negative `destinationLength` throws
         * `ArgumentOutOfRangeException("destinationLength")`, and a null `destination` with a
         * positive length throws `ArgumentNullException("destination")`.
         */
        OperationStatus Flush(bytecs* destination, intcs destinationLength, intcs& bytesWritten);

        /**
         * The `Try*` overloads construct a temporary codec and funnel through the member above, so
         * they carry its raw-pointer contract unchanged: an invalid length or buffer THROWS rather
         * than returning `false` (ticket #2146; documented here by #2151).
         */
        /** Tries to compress @p source into @p destination using the default quality and window size. */
        static bool TryCompress(const bytecs* source, intcs sourceLength,
                                 bytecs* destination, intcs destinationLength, intcs& bytesWritten);
        /** Tries to compress @p source into @p destination using the specified quality. */
        static bool TryCompress(const bytecs* source, intcs sourceLength,
                                 bytecs* destination, intcs destinationLength, intcs& bytesWritten,
                                 intcs quality);
        /** Tries to compress @p source into @p destination using the specified quality and window size. */
        static bool TryCompress(const bytecs* source, intcs sourceLength,
                                 bytecs* destination, intcs destinationLength, intcs& bytesWritten,
                                 intcs quality, intcs windowLog);
    };

} // namespace System::IO::Compression
