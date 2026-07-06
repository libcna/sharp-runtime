// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Memory.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net {

    using SharpRuntime::intcs;
    using SharpRuntime::bytecs;

    /**
     * @brief Indicates how to format the memory buffer that a platform uses for a network
     * address, for use when subclassing EndPoint.
     *
     * C++ counterpart of .NET System.Net.SocketAddress.
     *
     * @note This runtime has no native Socket/OS interop yet (see NEXT.md — Socket/TcpListener
     * are unimplemented), so the buffer layout here is this runtime's own simplified encoding
     * (2-byte little-endian address family at offset 0), not the platform sockaddr ABI that
     * .NET's SocketAddressPal produces. It exists to satisfy the EndPoint::Serialize()/Create()
     * API surface and give ported game code somewhere to store raw address bytes.
     */
    class SocketAddress {
        std::vector<bytecs> buffer_;
        intcs size_ = 0;

        static constexpr intcs MinSize = 2;
        static constexpr intcs DataOffset = 2;

    public:
        static constexpr intcs IPv4AddressSize = 16;
        static constexpr intcs IPv6AddressSize = 28;
        static constexpr intcs UdsAddressSize = 110;
        static constexpr intcs MaxAddressSize = 128;

        /** @return The maximum address buffer size typically needed for @p addressFamily. */
        [[nodiscard]] static intcs GetMaximumAddressSize(System::Net::Sockets::AddressFamily addressFamily);

        /** Constructs a SocketAddress sized to the maximum buffer size for @p family. */
        explicit SocketAddress(System::Net::Sockets::AddressFamily family);

        /** Constructs a SocketAddress of exactly @p size bytes for @p family. */
        SocketAddress(System::Net::Sockets::AddressFamily family, intcs size);

        /** @return The address family encoded in this buffer. */
        [[nodiscard]] System::Net::Sockets::AddressFamily getFamilyProperty() const;

        /** @return The number of bytes currently in use in the buffer. */
        [[nodiscard]] intcs getSizeProperty() const { return size_; }
        /** Sets the number of bytes currently in use in the buffer. */
        void setSizeProperty(intcs value);

        /** Indexer: accesses a byte of the address data (offset 0 is the first byte after the family header). */
        [[nodiscard]] bytecs operator[](intcs offset) const;
        /** Indexer: mutates a byte of the address data. */
        bytecs& operator[](intcs offset);

        /** @return The underlying buffer as a Memory view. */
        [[nodiscard]] System::Memory<bytecs> getBufferProperty();

        /** Equality comparison: true if both buffers have identical contents. */
        [[nodiscard]] bool Equals(const SocketAddress& other) const;
        bool operator==(const SocketAddress& other) const { return Equals(other); }
        bool operator!=(const SocketAddress& other) const { return !Equals(other); }

        /** @return A human-readable representation, e.g. "InterNetwork:16:{...}". */
        [[nodiscard]] std::string ToString() const;
    };

} // namespace System::Net
