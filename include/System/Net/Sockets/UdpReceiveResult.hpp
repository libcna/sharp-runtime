// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Net/IPEndPoint.hpp"

namespace System::Net::Sockets {

    /**
     * @brief Presents UDP receive result information from UdpClient::ReceiveAsync().
     *
     * C++ counterpart of .NET System.Net.Sockets.UdpReceiveResult.
     */
    class UdpReceiveResult {
        std::vector<SharpRuntime::bytecs> buffer_;
        System::Net::IPEndPoint remoteEndPoint_;

    public:
        UdpReceiveResult() = default;

        UdpReceiveResult(std::vector<SharpRuntime::bytecs> buffer, System::Net::IPEndPoint remoteEndPoint)
            : buffer_(std::move(buffer)), remoteEndPoint_(std::move(remoteEndPoint)) {}

        /** @return The buffer with the data received in the UDP packet. */
        [[nodiscard]] const std::vector<SharpRuntime::bytecs>& getBufferProperty() const { return buffer_; }

        /** @return The remote endpoint from which the UDP packet was received. */
        [[nodiscard]] System::Net::IPEndPoint getRemoteEndPointProperty() const { return remoteEndPoint_; }

        [[nodiscard]] bool Equals(const UdpReceiveResult& other) const {
            return buffer_ == other.buffer_ && remoteEndPoint_ == other.remoteEndPoint_;
        }
        bool operator==(const UdpReceiveResult& o) const { return Equals(o); }
        bool operator!=(const UdpReceiveResult& o) const { return !Equals(o); }
    };

} // namespace System::Net::Sockets
