// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/SocketAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include <array>

namespace System::Net {

    using System::Net::Sockets::AddressFamily;
    using SharpRuntime::uintcs;
    using SharpRuntime::longcs;

    namespace {
        std::string addressFamilyToString(AddressFamily family) {
            switch (family) {
                case AddressFamily::Unknown:         return "Unknown";
                case AddressFamily::Unspecified:      return "Unspecified";
                case AddressFamily::Unix:             return "Unix";
                case AddressFamily::InterNetwork:     return "InterNetwork";
                case AddressFamily::InterNetworkV6:   return "InterNetworkV6";
                case AddressFamily::Max:              return "Max";
                default:                              return std::to_string(static_cast<int>(family));
            }
        }

        // The number of bytes GetIPEndPoint's layout needs for a family, or 0 when the family
        // has no IP layout at all. Deliberately the same two constants IPEndPoint::Create
        // already checks against, so the safe path and this public shortcut cannot disagree.
        // File-local, not a member: this ticket must not touch SocketAddress.hpp, because a
        // header change would turn a relink-only repair into a consumer recompile.
        intcs requiredIPEndPointSize(AddressFamily family) {
            switch (family) {
                case AddressFamily::InterNetwork:   return SocketAddress::IPv4AddressSize;
                case AddressFamily::InterNetworkV6: return SocketAddress::IPv6AddressSize;
                default:                            return 0;
            }
        }
    }

    intcs SocketAddress::GetMaximumAddressSize(AddressFamily addressFamily) {
        switch (addressFamily) {
            case AddressFamily::InterNetwork:   return IPv4AddressSize;
            case AddressFamily::InterNetworkV6: return IPv6AddressSize;
            case AddressFamily::Unix:           return UdsAddressSize;
            default:                            return MaxAddressSize;
        }
    }

    SocketAddress::SocketAddress(AddressFamily family)
        : SocketAddress(family, GetMaximumAddressSize(family)) {
    }

    SocketAddress::SocketAddress(AddressFamily family, intcs size) {
        System::ArgumentOutOfRangeException::ThrowIfLessThan(size, MinSize, "size");

        size_ = size;
        buffer_.assign(static_cast<size_t>(size), bytecs{0});
        buffer_[0] = static_cast<bytecs>(static_cast<uint16_t>(family) & 0xFF);
        buffer_[1] = static_cast<bytecs>((static_cast<uint16_t>(static_cast<intcs>(family)) >> 8) & 0xFF);
    }

    AddressFamily SocketAddress::getFamilyProperty() const {
        uint16_t raw = static_cast<uint16_t>(buffer_[0]) | (static_cast<uint16_t>(buffer_[1]) << 8);
        return static_cast<AddressFamily>(static_cast<int16_t>(raw));
    }

    void SocketAddress::setSizeProperty(intcs value) {
        System::ArgumentOutOfRangeException::ThrowIfGreaterThan(value, static_cast<intcs>(buffer_.size()), "value");
        System::ArgumentOutOfRangeException::ThrowIfLessThan(value, 0, "value");
        size_ = value;
    }

    bytecs SocketAddress::operator[](intcs offset) const {
        if (static_cast<uintcs>(offset) >= static_cast<uintcs>(size_)) {
            throw System::IndexOutOfRangeException();
        }
        return buffer_[static_cast<size_t>(offset)];
    }

    bytecs& SocketAddress::operator[](intcs offset) {
        if (static_cast<uintcs>(offset) >= static_cast<uintcs>(size_)) {
            throw System::IndexOutOfRangeException();
        }
        return buffer_[static_cast<size_t>(offset)];
    }

    System::Memory<bytecs> SocketAddress::getBufferProperty() {
        return System::Memory<bytecs>(buffer_);
    }

    bool SocketAddress::Equals(const SocketAddress& other) const {
        return buffer_ == other.buffer_;
    }

    SocketAddress::SocketAddress(const IPAddress& address, intcs port)
        : SocketAddress(address.getAddressFamilyProperty(),
                        address.getIsIPv6Property() ? IPv6AddressSize : IPv4AddressSize) {
        // The two bytes below hold a 16-bit port, so every intcs outside [MinPort, MaxPort] used
        // to be silently truncated into that field rather than rejected: measured before this
        // check existed (build-probe/2035_probe1_before.log), port 70000 encoded as 4464, -1 as
        // 65535, INTCS_MAX as 65535 and INTCS_MIN as 0. The domain and the exception identity are
        // IPEndPoint::validatePort's, which this constructor's own result is handed to.
        System::ArgumentOutOfRangeException::ThrowIfLessThan(port, IPEndPoint::MinPort, "port");
        System::ArgumentOutOfRangeException::ThrowIfGreaterThan(port, IPEndPoint::MaxPort, "port");

        buffer_[2] = static_cast<bytecs>((static_cast<uint16_t>(port) >> 8) & 0xFF);
        buffer_[3] = static_cast<bytecs>(static_cast<uint16_t>(port) & 0xFF);

        auto addressBytes = address.GetAddressBytes();
        if (address.getIsIPv6Property()) {
            for (size_t i = 0; i < 16; ++i) buffer_[8 + i] = addressBytes[i];
            uint32_t scopeId = static_cast<uint32_t>(address.getScopeIdProperty());
            buffer_[24] = static_cast<bytecs>((scopeId >> 24) & 0xFF);
            buffer_[25] = static_cast<bytecs>((scopeId >> 16) & 0xFF);
            buffer_[26] = static_cast<bytecs>((scopeId >> 8) & 0xFF);
            buffer_[27] = static_cast<bytecs>(scopeId & 0xFF);
        } else {
            for (size_t i = 0; i < 4; ++i) buffer_[4 + i] = addressBytes[i];
        }
    }

    IPEndPoint SocketAddress::GetIPEndPoint() const {
        AddressFamily family = getFamilyProperty();

        // Neither of the two checks below existed. GetIPEndPoint read fixed offsets 2..7 (IPv4)
        // or 2..27 (IPv6) out of a buffer whose size and family the caller chose, so:
        //
        //   * every family that is not an IP family -- Unix, Unspecified, Unknown, Max and any
        //     unnamed value -- fell into the IPv4 branch and decoded as an IPv4 endpoint;
        //   * an undersized buffer was read past its end. ASan reported a heap-buffer-overflow
        //     READ at this function's own lines for both branches (SocketAddress.cpp:106 for a
        //     2-byte InterNetwork buffer, :110 for an 8-byte InterNetworkV6 one), and the values
        //     produced were whatever the heap happened to hold -- "238.85.0.0:52122" in one run;
        //   * a buffer whose DECLARED size had been shrunk with setSizeProperty() below the
        //     layout minimum still decoded, reading bytes the object itself reports as unused.
        //     That case is memory-safe (the allocation is still there) so no sanitizer sees it,
        //     but it contradicts operator[], which throws IndexOutOfRangeException for exactly
        //     the same offsets. Validating the DECLARED size closes both: size_ is never greater
        //     than buffer_.size(), so requiring it also bounds the allocation.
        //
        // The required sizes are IPEndPoint::Create's, which is the safe path this public
        // shortcut bypasses, and are exactly what SocketAddress(const IPAddress&, intcs)
        // produces -- so no buffer this class builds is affected.
        const intcs required = requiredIPEndPointSize(family);
        if (required == 0) {
            throw System::ArgumentException(
                "The socket address family '" + addressFamilyToString(family) +
                "' cannot be decoded as an IPEndPoint. Only InterNetwork and InterNetworkV6 can.");
        }
        if (size_ < required) {
            throw System::ArgumentException(
                "Socket address size is too small: " + std::to_string(size_) +
                " byte(s), but address family '" + addressFamilyToString(family) +
                "' requires " + std::to_string(required) + ".");
        }

        intcs port = (static_cast<intcs>(buffer_[2]) << 8) | static_cast<intcs>(buffer_[3]);

        if (family == AddressFamily::InterNetworkV6) {
            std::array<bytecs, 16> addrBytes{};
            for (size_t i = 0; i < 16; ++i) addrBytes[i] = buffer_[8 + i];
            uint32_t scopeId = (static_cast<uint32_t>(buffer_[24]) << 24) |
                                (static_cast<uint32_t>(buffer_[25]) << 16) |
                                (static_cast<uint32_t>(buffer_[26]) << 8) |
                                static_cast<uint32_t>(buffer_[27]);
            return IPEndPoint(IPAddress(addrBytes, static_cast<longcs>(scopeId)), port);
        }

        std::vector<bytecs> addrBytes(buffer_.begin() + 4, buffer_.begin() + 8);
        return IPEndPoint(IPAddress(addrBytes), port);
    }

    std::string SocketAddress::ToString() const {
        std::string result = addressFamilyToString(getFamilyProperty());
        result += ':';
        result += std::to_string(size_);
        result += ":{";
        for (intcs i = DataOffset; i < size_; ++i) {
            if (i > DataOffset) result += ',';
            result += std::to_string(static_cast<int>(buffer_[static_cast<size_t>(i)]));
        }
        result += '}';
        return result;
    }

} // namespace System::Net
