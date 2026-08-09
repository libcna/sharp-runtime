// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Compression/DeflateDecoder.hpp"

namespace System::IO::Compression {

    /**
     * @brief Provides methods to decode data compressed in the ZLib data format in a
     * streamless, non-allocating, and performant manner.
     *
     * C++ counterpart of .NET System.IO.Compression.ZLibDecoder. Internally composes a
     * DeflateDecoder configured for ZLib framing (zlib header/trailer).
     *
     * @note Status: IMPLEMENTED — requires system zlib (linked via ZLIB::ZLIB CMake target).
     */
    class ZLibDecoder {
    private:
        DeflateDecoder deflateDecoder_;
        bool disposed_ = false;

    public:
        /** Initializes a new ZLibDecoder. */
        ZLibDecoder();

        /** Frees and disposes unmanaged (zlib) resources. */
        void Dispose();

        /**
         * @brief Decompresses @p source into @p destination.
         * @return The status with which the operation finished.
         * @throws System::ObjectDisposedException if this decoder has been disposed.
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
        OperationStatus Decompress(const bytecs* source, intcs sourceLength,
                                   bytecs* destination, intcs destinationLength,
                                   intcs& bytesConsumed, intcs& bytesWritten);

        /**
         * The `Try*` overloads construct a temporary codec and funnel through the member above, so
         * they carry its raw-pointer contract unchanged: an invalid length or buffer THROWS rather
         * than returning `false` (ticket #2146; documented here by #2151).
         */
        /**
         * @brief Tries to decompress @p source into @p destination in one call using a
         * temporary ZLibDecoder.
         * @return true if decompression consumed all of @p source and completed successfully.
         */
        static bool TryDecompress(const bytecs* source, intcs sourceLength,
                                   bytecs* destination, intcs destinationLength,
                                   intcs& bytesWritten);
    };

} // namespace System::IO::Compression
