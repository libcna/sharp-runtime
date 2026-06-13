// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Net/IPAddress.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net {

    using SharpRuntime::intcs;

    /// Represents a network endpoint as an IP address and a port number.
    ///
    /// Partial C++ counterpart of .NET System.Net.IPEndPoint.
    ///
    /// @note Status: Partial
    class IPEndPoint {
        IPAddress address_;
        intcs     port_ = 0;
    public:
        /// Default constructor — 0.0.0.0:0.
        IPEndPoint() = default;

        /// Constructs an endpoint from an IPAddress and port.
        IPEndPoint(const IPAddress& address, intcs port) : address_(address), port_(port) {}

        /// Constructs an endpoint from a raw 32-bit address and port.
        IPEndPoint(uint32_t address, intcs port) : address_(address), port_(port) {}

        /// @return The IP address component.
        [[nodiscard]] const IPAddress& getAddressProperty() const { return address_; }

        /// @return The port number.
        [[nodiscard]] intcs            getPortProperty()    const { return port_; }

        /// Sets the IP address component.
        void setAddressProperty(const IPAddress& a) { address_ = a; }

        /// Sets the port number.
        void setPortProperty(intcs p)               { port_ = p; }

        /// @return The endpoint as "address:port".
        [[nodiscard]] std::string ToString() const {
            return address_.ToString() + ':' + std::to_string(port_);
        }

        /// Equality operator.
        bool operator==(const IPEndPoint& o) const { return address_ == o.address_ && port_ == o.port_; }
        /// Inequality operator.
        bool operator!=(const IPEndPoint& o) const { return !(*this == o); }

        static const intcs MinPort = 0;     ///< Minimum valid port number.
        static const intcs MaxPort = 65535; ///< Maximum valid port number.
    };

} // namespace System::Net
