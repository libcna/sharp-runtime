// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Stream.hpp"

namespace System::Net::Sockets {

    using System::IO::bytecs;
    using System::IO::intcs;

    /**
     * @brief Provides the underlying stream of data for network access.
     *
     * Wraps a socket file descriptor and exposes it as a System::IO::Stream.
     * Obtained via TcpClient::GetStream(). Uses Winsock2 on Windows,
     * POSIX sockets on Linux/macOS; throws on Emscripten.
     *
     * @par The descriptor is validated at construction (#2136, SR-AUD-265)
     * Real .NET's constructor takes a `Socket` and rejects four states before it will wrap it:
     * a null socket, a non-blocking socket, an unconnected socket, and a socket whose
     * `SocketType` is not `Stream`. This port's constructor takes a **raw descriptor**, so it
     * must first establish something .NET gets for free — that the handle is a socket at all —
     * and then applies the same four rules. Until #2136 it applied **none** of them:
     * `NetworkStream(-1)` constructed happily and then reported end-of-stream from every `Read`
     * and *silently succeeded* from every `Write`, and a pipe descriptor — not a socket in any
     * sense — was accepted just as readily.
     *
     * @par A closed stream throws rather than lying (#2136, cause NS-B)
     * `Close()` has always recorded the closed state, and `getCanReadProperty()` /
     * `getCanWriteProperty()` have always reported it correctly. Neither `Read` nor `Write`
     * consulted it: `Read` returned `0` (indistinguishable from a peer that closed cleanly) and
     * `Write` returned **normally, having written nothing**, which every caller of a `void
     * Write` reads as "the bytes went out". Both now throw
     * `System::ObjectDisposedException`, matching this repository's established closed-stream
     * contract (`MemoryStream`, `UnmanagedMemoryStream`, `BufferedStream`, `FileStream`).
     * The argument checks stay **in front of** the closed check, so a caller who passes a
     * negative `offset` to a closed stream still learns about the argument first.
     *
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class NetworkStream : public System::IO::Stream {
        int fd_ = -1;

        /** @brief Throws System::ObjectDisposedException if Close() has already run. */
        void ThrowIfClosed() const;

    public:
        /**
         * @brief Takes ownership of @p fd (will close on destruction/Close).
         *
         * @param fd An **open, blocking, connected, stream-oriented socket** descriptor.
         * @throws System::ArgumentException if @p fd is negative, is not an open handle, or is
         *         an open handle that is not a socket (a pipe or a file, for example). The
         *         descriptor is **not closed** when construction is rejected — the caller still
         *         owns it.
         * @throws System::IO::IOException if @p fd is a socket but is non-blocking, is not
         *         connected, or is not stream-oriented, mirroring .NET's three messages.
         *
         * @note On Windows the non-blocking check is **absent**: Winsock's `FIONBIO` is
         *       write-only, so the state cannot be read back from the handle (real .NET tracks
         *       it in managed state instead). On Emscripten no check runs at all — every
         *       operation on this type throws `PlatformNotSupportedException` there anyway.
         */
        explicit NetworkStream(int fd);
        ~NetworkStream();

        // Not copyable: fd_ is a raw owned socket handle closed by the destructor -- an implicit
        // shallow copy (previously allowed) lets two instances' destructors both close the same
        // fd, either failing silently or, if the fd was reused in between, closing a handle this
        // instance doesn't own. Matches Socket's own established copy-deletion.
        NetworkStream(const NetworkStream&) = delete;
        NetworkStream& operator=(const NetworkStream&) = delete;

        /**
         * @brief Reads up to @p count bytes from the socket.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         * @throws System::ObjectDisposedException if the stream has been closed (#2136).
         * @throws System::IO::IOException if the underlying socket read fails.
         */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;

        /**
         * @brief Writes @p count bytes to the socket.
         * @throws System::ArgumentOutOfRangeException if @p offset or @p count is negative.
         * @throws System::ObjectDisposedException if the stream has been closed (#2136).
         * @throws System::IO::IOException if the underlying socket write fails.
         */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;

        /** @brief Closes the socket. Idempotent — a second Close() is a no-op. */
        void  Close() override;

        /** @brief Not supported — TCP streams are not seekable. */
        [[nodiscard]] intcs getLengthProperty() const override;

        [[nodiscard]] bool getCanReadProperty()  const override { return fd_ >= 0; }
        [[nodiscard]] bool getCanWriteProperty() const override { return fd_ >= 0; }

        [[nodiscard]] int getFd() const { return fd_; }
    };

} // namespace System::Net::Sockets
