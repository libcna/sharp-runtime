// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;
using SharpRuntime::longcs;

/**
 * <summary>Provides static methods for reading and writing files at specific positions without
 * seeking.</summary>
 *
 * <remarks>
 * <para><b>Argument domain (ticket #2100, SR-AUD-340).</b> Every member below rejects an
 * out-of-domain argument with an <c>ArgumentOutOfRangeException</c> or
 * <c>ArgumentNullException</c> that names the offending parameter, and it does so
 * <em>before</em> touching the descriptor, so a rejected call transfers no bytes and changes no
 * file. Until #2100 a negative <c>count</c> was accepted silently by <c>Write</c>, and every
 * other rejection surfaced as a bare <c>IOException("RandomAccess::&lt;member&gt; failed")</c>
 * that discarded both the parameter name and the native reason.</para>
 *
 * <para>The exception types and parameter names are <b>this port's choice</b>, recorded as such
 * in <c>docs/SystemIONamespaceReviewPlan.md</c> §16 — the .NET reference tree is absent, so no
 * evidence pins what .NET raises at these doors.</para>
 *
 * <para><b>Descriptor ownership.</b> <c>RandomAccess</c> takes a raw <c>int fd</c> and owns
 * nothing: it never opens, closes, duplicates or validates the descriptor's lifetime. A caller
 * that passes a descriptor it has already closed gets whatever the operating system reports for
 * that number. Widening this into a descriptor-ownership design is explicitly out of scope for
 * #2100 (plan §7.5).</para>
 * </remarks>
 */
class RandomAccess {
public:
    RandomAccess() = delete;

    /**
     * <summary>Gets the length of the file in bytes (platform-specific fd).</summary>
     * <exception cref="IOException">The descriptor is invalid, or it is not seekable — a pipe,
     * for instance. Before #2100 both cases <em>returned</em> the sentinel <c>-1</c> on POSIX
     * instead of throwing; the Windows branch already threw.</exception>
     */
    static longcs GetLength(int fd);

    /**
     * <summary>Sets the length of the file.</summary>
     * <exception cref="ArgumentOutOfRangeException"><paramref name="length"/> is negative.</exception>
     * <exception cref="IOException">The underlying truncation failed; the message carries the
     * native reason.</exception>
     */
    static void SetLength(int fd, longcs length);

    /**
     * <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
     * <exception cref="ArgumentOutOfRangeException"><paramref name="fileOffset"/> is negative, or
     * the buffer is longer than a single operation can transfer.</exception>
     */
    static intcs Read(int fd, std::vector<bytecs>& buffer, longcs fileOffset);

    /**
     * <summary>Reads bytes from the file at the specified offset into the buffer.</summary>
     * <returns>The number of bytes read, which may be fewer than <paramref name="count"/> and is
     * zero at end of file.</returns>
     * <exception cref="ArgumentOutOfRangeException"><paramref name="count"/> or
     * <paramref name="fileOffset"/> is negative.</exception>
     * <exception cref="ArgumentNullException"><paramref name="buffer"/> is null and
     * <paramref name="count"/> is greater than zero. A null buffer with a count of zero is
     * accepted and transfers nothing.</exception>
     */
    static intcs Read(int fd, bytecs* buffer, intcs count, longcs fileOffset);

    /**
     * <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
     * <exception cref="ArgumentOutOfRangeException"><paramref name="fileOffset"/> is negative, or
     * the buffer is longer than a single operation can transfer.</exception>
     */
    static void Write(int fd, const std::vector<bytecs>& buffer, longcs fileOffset);

    /**
     * <summary>Writes bytes from the buffer to the file at the specified offset.</summary>
     * <remarks>The whole buffer is written: a short write is retried from where it stopped.</remarks>
     * <exception cref="ArgumentOutOfRangeException"><paramref name="count"/> or
     * <paramref name="fileOffset"/> is negative.</exception>
     * <exception cref="ArgumentNullException"><paramref name="buffer"/> is null and
     * <paramref name="count"/> is greater than zero.</exception>
     */
    static void Write(int fd, const bytecs* buffer, intcs count, longcs fileOffset);

    /**
     * <summary>Flushes OS write buffers to disk for the given file descriptor.</summary>
     * <exception cref="IOException">The flush failed; the message carries the native reason.</exception>
     */
    static void FlushToDisk(int fd);
};

} // namespace System::IO
