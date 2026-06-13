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
     * @note Status: Implemented — Windows (Winsock2) and POSIX (Linux/macOS).
     */
    class NetworkStream : public System::IO::Stream {
        int fd_ = -1;

    public:
        /// @brief Takes ownership of @p fd (will close on destruction/Close).
        explicit NetworkStream(int fd);
        ~NetworkStream();

        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        void  Close() override;

        /// @brief Not supported — TCP streams are not seekable.
        [[nodiscard]] intcs getLengthProperty() const override;

        [[nodiscard]] bool getCanReadProperty()  const override { return fd_ >= 0; }
        [[nodiscard]] bool getCanWriteProperty() const override { return fd_ >= 0; }

        [[nodiscard]] int getFd() const { return fd_; }
    };

} // namespace System::Net::Sockets
