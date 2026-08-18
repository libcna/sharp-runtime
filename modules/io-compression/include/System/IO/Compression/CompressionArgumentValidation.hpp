// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IO/Compression/CompressionMode.hpp"
#include "System/IO/Compression/ZLibCompressionStrategy.hpp"

namespace System::IO::Compression::Detail {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;

    /**
     * @brief The one place `System::IO::Compression` states the contract its raw-pointer surface
     * needs before a length reaches zlib.
     *
     * .NET's encoder/decoder surface is expressed in `ReadOnlySpan<byte>`/`Span<byte>`. This port
     * replaced every span with a raw pointer plus a signed `intcs` length, and every body then
     * handed that length to zlib as an **unsigned** `uInt`:
     *
     * @code
     * zs.avail_in  = static_cast<uInt>(sourceLength);      // -1 becomes 4,294,967,295
     * zs.avail_out = static_cast<uInt>(destinationLength);
     * @endcode
     *
     * A negative length therefore became an enormous byte count and zlib read or wrote far past
     * the caller's allocation. AddressSanitizer reported a 65,536-byte READ past a one-byte source
     * and a WRITE past a one-byte destination (`build-probe/2146_probe1_before.log`, 15 crashes in
     * 63 cases). This module is the one whose ordinary job is parsing **attacker-supplied** bytes,
     * so the state a span cannot represent must be rejected before zlib sees it.
     *
     * The rule, the exception types and the messages are deliberately **identical** to
     * `System::IO::Hashing::Detail` (ticket #2141/#2142, SR-AUD-260/261), which repaired the same
     * idiom in the sibling module. Two components solving one problem two ways is how the
     * repository ends up with two answers to "what does a negative length mean".
     *
     * Not part of the public API surface of the module's types; declared in a public header only
     * because every implementation file in the component needs it.
     */

    /** @brief Throws `ArgumentOutOfRangeException(paramName, "Non-negative number required.")`. */
    [[noreturn]] void ThrowNegativeLength(const char* paramName);

    /** @brief Throws `ArgumentNullException(paramName)`. */
    [[noreturn]] void ThrowNullBuffer(const char* paramName);

    /**
     * @brief Rejects a source buffer that cannot be read for @p length bytes.
     *
     * A **null pointer with a length of zero is accepted**, deliberately: `default(ReadOnlySpan<byte>)`
     * in .NET is an empty span whose reference is null, and compressing or decompressing nothing is
     * legal. Only a null pointer with a *positive* length is an error.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     * @throws System::ArgumentNullException if @p length is positive and @p source is null.
     */
    inline void ValidateSource(const bytecs* source, intcs length) {
        if (length < 0) {
            ThrowNegativeLength("sourceLength");
        }
        if (length > 0 && source == nullptr) {
            ThrowNullBuffer("source");
        }
    }

    /**
     * @brief Rejects a destination buffer that cannot be written for @p length bytes.
     *
     * Symmetrical with ValidateSource, and separate from it so the thrown `paramName` names the
     * argument the caller actually got wrong. A zero-length destination is legal — every codec in
     * this module reports `DestinationTooSmall` for it rather than failing.
     *
     * @throws System::ArgumentOutOfRangeException if @p length is negative.
     * @throws System::ArgumentNullException if @p length is positive and @p destination is null.
     */
    inline void ValidateDestination(const bytecs* destination, intcs length) {
        if (length < 0) {
            ThrowNegativeLength("destinationLength");
        }
        if (length > 0 && destination == nullptr) {
            ThrowNullBuffer("destination");
        }
    }

    // -----------------------------------------------------------------------------------------
    // Ticket #2148 (SR-AUD-258, cause C-B) — the stream half of the same "state the type cannot
    // represent must not reach zlib" rule. Kept here, beside the length contract, so the whole
    // component has exactly one definition of each rejection.
    // -----------------------------------------------------------------------------------------

    /**
     * @brief Throws `ArgumentException("Enum value was out of legal range.", paramName)`.
     *
     * The **base** `ArgumentException`, not the derived `ArgumentOutOfRangeException`. #2148 chose
     * this from the audit's own managed probe and recorded that `/rv` was absent to narrow it
     * further. **It is present now, and it confirms the choice exactly**: `DeflateStream.cs:99` is
     * `throw new ArgumentException(SR.ArgumentOutOfRange_Enum, nameof(mode))`, and
     * `ArgumentOutOfRange_Enum` is *"Enum value was out of legal range."* — the same type, the same
     * message and the same parameter name. Verified 2026-08-18 by ticket #2152.
     */
    [[noreturn]] void ThrowInvalidCompressionMode(const char* paramName);

    /** @brief Throws `ObjectDisposedException(typeName, "Cannot access a closed Stream.")`. */
    [[noreturn]] void ThrowStreamClosed(const char* typeName);

    /**
     * @brief Throws `InvalidOperationException("Reading from the compression stream is not supported.")`.
     *
     * .NET's `SR.CannotReadFromDeflateStream`, transcribed
     * (`System.IO.Compression/src/Resources/Strings.resx:122`). The message names *the compression
     * stream* rather than the concrete type, so all three wrappers share one string, exactly as
     * .NET's three do — `GZipStream` and `ZLibStream` delegate to `DeflateStream` there.
     */
    [[noreturn]] void ThrowCannotReadFromCompressionStream();

    /**
     * @brief Throws `InvalidOperationException("Writing to the compression stream is not supported.")`.
     *
     * .NET's `SR.CannotWriteToDeflateStream` (`Strings.resx:125`).
     */
    [[noreturn]] void ThrowCannotWriteToCompressionStream();

    /**
     * @brief Rejects a read on a stream that was opened to **compress**.
     *
     * Ticket #2152 (SR-AUD post-audit, measured under #2148 and deliberately left then). Before it,
     * a `Read` on an open Compress-mode stream returned **0** — a silent wrong answer a caller
     * cannot distinguish from end-of-stream — and a `Write` on an open Decompress-mode stream
     * surfaced zlib's `Z_STREAM_ERROR` as `IOException("DeflateStream: deflate error -2")`, naming
     * an internal error code for a caller mistake. .NET guards both
     * (`DeflateStream.cs:387-403`).
     *
     * **The position matters and is transcribed rather than chosen.** .NET runs
     * `ValidateBufferArguments` in `Read(byte[], int, int)`, then `ReadCore` opens with
     * `EnsureDecompressionMode(); EnsureNotDisposed();` — in that order (`DeflateStream.cs:284-309`).
     * So the mode check sits **between** the argument validation and the disposed check, and a
     * *disposed* Compress-mode stream reports the **mode** error, not `ObjectDisposedException`.
     *
     * `Flush()` deliberately gets **no** guard: .NET's opens with `EnsureNotDisposed()` alone and
     * then no-ops for a Decompress-mode stream (`DeflateStream.cs:210-215`).
     *
     * @throws System::InvalidOperationException when @p mode is not `Decompress`.
     */
    inline void EnsureDecompressionMode(CompressionMode mode) {
        if (mode != CompressionMode::Decompress) {
            ThrowCannotReadFromCompressionStream();
        }
    }

    /**
     * @brief Rejects a write on a stream that was opened to **decompress**.
     *
     * The mirror of EnsureDecompressionMode; see that function for the ordering rule and for what
     * each door did before ticket #2152.
     *
     * @throws System::InvalidOperationException when @p mode is not `Compress`.
     */
    inline void EnsureCompressionMode(CompressionMode mode) {
        if (mode != CompressionMode::Compress) {
            ThrowCannotWriteToCompressionStream();
        }
    }

    /**
     * @brief Rejects a `CompressionMode` outside the two members the enum declares.
     *
     * A cast such as `static_cast<CompressionMode>(42)` used to construct successfully in all
     * three stream types and take the **deflate** arm of the constructor while `Close()` takes the
     * **inflate** arm, so `inflateEnd` was called on a `deflateInit2`-initialised stream. zlib
     * rejects that with `Z_STREAM_ERROR` and frees nothing: LeakSanitizer reported a direct leak of
     * 5,952 bytes plus a 65,536-byte indirect leak **per constructed object**, in all three types
     * (`build-probe/2148_probe2_before.log`), while the caller's payload was silently discarded and
     * both capability properties reported `false`. Rejecting at construction is what closes it —
     * a check on the I/O path would still leave a constructed object whose two halves disagree.
     *
     * @throws System::ArgumentException if @p mode is neither `Decompress` nor `Compress`.
     */
    inline void ValidateCompressionMode(CompressionMode mode) {
        if (mode != CompressionMode::Decompress && mode != CompressionMode::Compress) {
            ThrowInvalidCompressionMode("mode");
        }
    }

    /**
     * @brief Rejects an operation on a stream whose `Close()` has already run.
     *
     * @param open     The wrapper's own open signal (`state_->initialized`).
     * @param typeName The wrapper's type name, used as the exception's object name.
     * @throws System::ObjectDisposedException when @p open is false.
     */
    inline void ThrowIfStreamClosed(bool open, const char* typeName) {
        if (!open) {
            ThrowStreamClosed(typeName);
        }
    }

    /**
     * @brief Maps a `ZLibCompressionStrategy` onto the zlib `Z_*_STRATEGY` constant it names.
     *
     * Ticket #2149 (SR-AUD-259, cause C-C). All three encoders' options constructors used to drop
     * `ZLibCompressionOptions::getCompressionStrategyProperty()` on the floor and pass
     * `Z_DEFAULT_STRATEGY` unconditionally, so all five strategy values produced byte-identical
     * output — measured over 3 encoders × 5 strategies × 3 payload shapes, 45 of 45 identical to
     * `Default`, while zlib given the same parameters produced different bytes in 24 of them
     * (`build-probe/2149_probe1_before.log`; after, the port matches zlib in all 45).
     *
     * Declared here, and defined once in `CompressionArgumentValidation.cpp`, so the three
     * encoders share one mapping: three private copies of a five-way switch is how the two halves
     * of a codec end up disagreeing. The return type is `intcs`, not a zlib type, so no zlib header
     * leaks into a public header.
     *
     * @return The zlib strategy constant (`Z_DEFAULT_STRATEGY`, `Z_FILTERED`, `Z_HUFFMAN_ONLY`,
     *         `Z_RLE` or `Z_FIXED`).
     * @throws System::ArgumentOutOfRangeException if @p strategy is outside the declared domain.
     *         Unreachable through `ZLibCompressionOptions`, whose setter validates, and kept so a
     *         future path that bypasses that setter fails loudly rather than silently compressing
     *         with the wrong strategy.
     */
    [[nodiscard]] intcs ResolveZLibStrategy(ZLibCompressionStrategy strategy);

} // namespace System::IO::Compression::Detail
