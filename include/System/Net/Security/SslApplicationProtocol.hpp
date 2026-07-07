// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"

namespace System::Net::Security {

    /**
     * @brief Represents an Application-Layer Protocol Negotiation (ALPN) TLS extension value.
     *
     * C++ counterpart of .NET System.Net.Security.SslApplicationProtocol (readonly struct).
     */
    class SslApplicationProtocol {
        std::vector<SharpRuntime::bytecs> protocol_;

    public:
        SslApplicationProtocol() = default;

        /** @brief Constructs from raw ALPN protocol-id bytes (RFC 7301: 1-255 bytes). */
        explicit SslApplicationProtocol(const std::vector<SharpRuntime::bytecs>& protocol) : protocol_(protocol) {
            if (protocol.empty() || protocol.size() > 255) {
                throw System::ArgumentException("Invalid SSL application protocol.", "protocol");
            }
        }

        /** @brief Constructs from a UTF-8-encoded protocol name (e.g. "h2"). */
        explicit SslApplicationProtocol(const std::string& protocol)
            : SslApplicationProtocol(std::vector<SharpRuntime::bytecs>(protocol.begin(), protocol.end())) {}

        /** @return The raw ALPN protocol-id bytes. */
        [[nodiscard]] const std::vector<SharpRuntime::bytecs>& getProtocolProperty() const { return protocol_; }

        [[nodiscard]] bool Equals(const SslApplicationProtocol& other) const { return protocol_ == other.protocol_; }
        bool operator==(const SslApplicationProtocol& o) const { return Equals(o); }
        bool operator!=(const SslApplicationProtocol& o) const { return !Equals(o); }

        [[nodiscard]] SharpRuntime::intcs GetHashCode() const {
            SharpRuntime::intcs hash = 0;
            for (auto b : protocol_) {
                hash = ((hash << 5) + hash) ^ static_cast<SharpRuntime::intcs>(b);
            }
            return hash;
        }

        /** @return The protocol name decoded as UTF-8, or a hex dump if it isn't valid UTF-8. */
        [[nodiscard]] std::string ToString() const {
            return std::string(protocol_.begin(), protocol_.end());
        }

        /** @brief Defines the HTTP/3.0 ALPN protocol id ("h3"). */
        static const SslApplicationProtocol Http3;
        /** @brief Defines the HTTP/2.0 ALPN protocol id ("h2"). */
        static const SslApplicationProtocol Http2;
        /** @brief Defines the HTTP/1.1 ALPN protocol id ("http/1.1"). */
        static const SslApplicationProtocol Http11;
    };

    inline const SslApplicationProtocol SslApplicationProtocol::Http3{std::string("h3")};
    inline const SslApplicationProtocol SslApplicationProtocol::Http2{std::string("h2")};
    inline const SslApplicationProtocol SslApplicationProtocol::Http11{std::string("http/1.1")};

} // namespace System::Net::Security
