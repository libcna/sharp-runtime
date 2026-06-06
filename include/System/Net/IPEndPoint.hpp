// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Net/IPAddress.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a network endpoint as an IP address and a port number.
     *
     * Partial C++ counterpart of .NET System.Net.IPEndPoint.
     *
     * @note Status: Partial
     */
    class IPEndPoint {
        IPAddress address_;
        intcs     port_ = 0;
    public:
        IPEndPoint() = default;
        IPEndPoint(const IPAddress& address, intcs port) : address_(address), port_(port) {}
        IPEndPoint(uint32_t address, intcs port) : address_(address), port_(port) {}

        [[nodiscard]] const IPAddress& getAddressProperty() const { return address_; }
        [[nodiscard]] intcs            getPortProperty()    const { return port_; }

        void setAddressProperty(const IPAddress& a) { address_ = a; }
        void setPortProperty(intcs p)               { port_ = p; }

        [[nodiscard]] std::string ToString() const {
            return address_.ToString() + ':' + std::to_string(port_);
        }

        bool operator==(const IPEndPoint& o) const { return address_ == o.address_ && port_ == o.port_; }
        bool operator!=(const IPEndPoint& o) const { return !(*this == o); }

        static const intcs MinPort = 0;
        static const intcs MaxPort = 65535;
    };

} // namespace System::Net
