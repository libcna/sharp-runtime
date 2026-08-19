// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/SocketAddress.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include <cctype>

namespace System::Net {

    using System::Net::Sockets::AddressFamily;

    void IPEndPoint::validatePort(intcs port) {
        System::ArgumentOutOfRangeException::ThrowIfLessThan(port, MinPort, "port");
        System::ArgumentOutOfRangeException::ThrowIfGreaterThan(port, MaxPort, "port");
    }

    namespace {
        // Verified against IPAddress.cs's IPAddress(long) constructor (which IPEndPoint(long,
        // int) delegates to in real .NET): throws ArgumentOutOfRangeException if the value
        // doesn't fit in 32 bits, rather than silently truncating. Comparing as uint64_t (not
        // int64_t) means a negative address is correctly rejected too, matching .NET's own
        // (ulong)newAddress cast before the range check.
        uint32_t validatedNarrow(longcs address) {
            System::ArgumentOutOfRangeException::ThrowIfGreaterThan(
                static_cast<uint64_t>(address), static_cast<uint64_t>(0xFFFFFFFFULL), "address");
            return static_cast<uint32_t>(address);
        }
    }

    IPEndPoint::IPEndPoint(const IPAddress& address, intcs port) : address_(address), port_(port) {
        validatePort(port);
    }

    IPEndPoint::IPEndPoint(uint32_t address, intcs port) : address_(address), port_(port) {
        validatePort(port);
    }

    IPEndPoint::IPEndPoint(longcs address, intcs port) : address_(validatedNarrow(address)), port_(port) {
        validatePort(port);
    }

    void IPEndPoint::setPortProperty(intcs p) {
        validatePort(p);
        port_ = p;
    }

    std::string IPEndPoint::ToString() const {
        if (address_.getIsIPv6Property()) {
            return "[" + address_.ToString() + "]:" + std::to_string(port_);
        }
        return address_.ToString() + ":" + std::to_string(port_);
    }

    SocketAddress IPEndPoint::Serialize() const {
        return SocketAddress(address_, port_);
    }

    std::shared_ptr<EndPoint> IPEndPoint::Create(const SocketAddress& socketAddress) const {
        AddressFamily family = socketAddress.getFamilyProperty();
        if (family != AddressFamily::InterNetwork && family != AddressFamily::InterNetworkV6) {
            throw System::ArgumentException("Unable to use socket address with this end point.", "socketAddress");
        }

        intcs minSize = family == AddressFamily::InterNetworkV6 ? SocketAddress::IPv6AddressSize : SocketAddress::IPv4AddressSize;
        if (socketAddress.getSizeProperty() < minSize) {
            throw System::ArgumentException("Socket address size is too small.", "socketAddress");
        }

        return std::make_shared<IPEndPoint>(socketAddress.GetIPEndPoint());
    }

    IPEndPoint IPEndPoint::Parse(const std::string& s) {
        IPEndPoint result;
        if (!TryParse(s, result)) {
            throw System::FormatException("An invalid IPEndPoint was specified.");
        }
        return result;
    }

    bool IPEndPoint::TryParse(const std::string& s, IPEndPoint& result) {
        std::string addressPart;
        std::string portPart;
        // #2045. "no colon at all" and "a colon with nothing after it" both leave portPart
        // empty, and they are NOT the same input: the first is a bare address and means port 0,
        // the second asserts that a port follows and none does. Without this flag the second
        // silently became port 0 -- a value the caller never wrote.
        //
        // .NET distinguishes them structurally rather than with a flag: `addressLength ==
        // s.Length` means no port field was found, and otherwise it runs `uint.TryParse` over
        // `s.Slice(addressLength + 1)`, which REJECTS an empty span under NumberStyles.None
        // (IPEndPoint.cs:120-148). A flag is the same distinction in this parser's shape.
        bool hasPortField = false;

        if (!s.empty() && s.front() == '[') {
            // "[ipv6]" or "[ipv6]:port" form.
            size_t closeBracket = s.find(']');
            if (closeBracket == std::string::npos) return false;
            addressPart = s.substr(1, closeBracket - 1);

            // This branch used to search for the next ':' ANYWHERE after the closing bracket and
            // silently drop everything in between, so text the parser did not understand simply
            // evaporated: "[::1]ignored:80" parsed as [::1]:80, "[::1]ignored" and "[::1]x" as
            // [::1]:0, and even "[::1] :80" -- with a space -- as [::1]:80, while the
            // unbracketed branch below correctly REJECTED the equivalent "1.2.3.4 :80". The two
            // branches of one function disagreed about the same input shape; the repair is
            // transcribed from the branch that was already right, not from any external
            // reference (ticket #2037, SR-AUD-302).
            //
            // What may follow the closing bracket is exactly nothing, or ':' and the port.
            const size_t afterBracket = closeBracket + 1;
            if (afterBracket < s.size()) {
                if (s[afterBracket] != ':') return false;
                portPart     = s.substr(afterBracket + 1);
                hasPortField = true;
            }
        } else {
            size_t lastColon = s.rfind(':');
            if (lastColon == std::string::npos) {
                addressPart = s;
            } else {
                // If there's more than one colon and no brackets, it's a bare IPv6 address with no port.
                size_t firstColon = s.find(':');
                if (firstColon != lastColon) {
                    addressPart = s;
                } else {
                    addressPart  = s.substr(0, lastColon);
                    portPart     = s.substr(lastColon + 1);
                    hasPortField = true;
                }
            }
        }

        IPAddress address;
        if (!IPAddress::TryParse(addressPart, address)) return false;

        intcs port = 0;
        if (hasPortField) {
            // An empty port field is a rejection, not a zero. This is the whole of #2045, and it
            // is why the guard is `hasPortField` rather than `!portPart.empty()`.
            //
            // HONEST NOTE: a mutation deleting THIS line is not caught, and it is a proven
            // equivalence rather than a gap -- the digit loop below runs zero times and
            // `std::stoul("")` throws `std::invalid_argument`, which the `catch (...)` already
            // turns into `return false` (verified by probe). It is kept because .NET's
            // `uint.TryParse` RETURNS false rather than throwing, so an explicit rejection is
            // the shape of the reference, and because resting a parser's correctness on an
            // exception thrown by a conversion function is a subtler contract than a test.
            if (portPart.empty()) return false;
            for (char c : portPart) {
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            }
            try {
                unsigned long value = std::stoul(portPart);
                if (value > static_cast<unsigned long>(MaxPort)) return false;
                port = static_cast<intcs>(value);
            } catch (...) {
                return false;
            }
        }

        result = IPEndPoint(address, port);
        return true;
    }

} // namespace System::Net
