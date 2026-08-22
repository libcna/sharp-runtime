// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Net/EndPoint.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/Sockets/LingerOption.hpp"
#include "System/Net/Sockets/ProtocolType.hpp"
#include "System/Net/Sockets/SelectMode.hpp"
#include "System/Net/Sockets/SocketFlags.hpp"
#include "System/Net/Sockets/SocketOptionLevel.hpp"
#include "System/Net/Sockets/SocketOptionName.hpp"
#include "System/Net/Sockets/SocketReceiveFromResult.hpp"
#include "System/Net/Sockets/SocketShutdown.hpp"
#include "System/Net/Sockets/SocketType.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Threading/Tasks/Task.hpp"

namespace System::Net::Sockets {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

}

namespace SharpRuntime::Testing {
    template <typename TOwner>
    struct SocketAsyncStartAccess;
}

namespace System::Net::Sockets {

    /**
     * @brief Implements the Berkeley sockets interface.
     *
     * C++ counterpart of .NET System.Net.Sockets.Socket. Uses Winsock2 on Windows, POSIX
     * sockets on Linux/macOS, and throws PlatformNotSupportedException on Emscripten — the same
     * three-way platform split already established by TcpClient/TcpListener/UdpClient.
     *
     * @note Reduced scope, documented deviations from .NET's full surface:
     * - No `SendFile`/`SendPacketsAsync` (Windows `TransmitFile`/`TRANSMIT_PACKETS`-specific
     *   scatter/gather sends) and no `DuplicateAndClose` (process-handle-inheritance concept
     *   that doesn't apply here).
     * - `ReceiveMessageFrom`/`SendMessageFrom` (with real ancillary/control-message data via
     *   `IPPacketInformation`) are not implemented; use `ReceiveFrom`/`SendTo` instead.
     * - Async methods return `System::Threading::Tasks::TaskT<T>` (this runtime's `std::async`
     *   based Task type) instead of .NET's `SocketAsyncEventArgs`-based APM/EAP pattern or
     *   `ValueTask`, matching the `*Async` convention already used by `HttpClient`/`Ping`.
     * - `GetSocketOption`/`SetSocketOption` support the common scalar (int/bool), `LingerOption`,
     *   and raw-byte-buffer overloads; the object-returning `GetSocketOption(level, name)`
     *   overload that mirrors .NET's reflection-free-but-boxing-based API is simplified to
     *   distinct typed overloads instead.
     */
    class Socket {
        friend struct SharpRuntime::Testing::SocketAsyncStartAccess<Socket>;

        intcs fd_ = -1;
        System::Net::Sockets::AddressFamily addressFamily_;
        System::Net::Sockets::SocketType socketType_;
        System::Net::Sockets::ProtocolType protocolType_;
        bool connected_ = false;
        bool bound_ = false;
        bool blocking_ = true;

        /**
         * The liveness boundary for the four `*Async` members (ticket #2134, CCF-019).
         *
         * Each of them returns a `TaskT` whose body runs on a `std::async` thread and calls back
         * into this object. Without a boundary, destroying or move-assigning the `Socket` while
         * one of those bodies is running reads freed storage: `Socket` is move-assignable and
         * destructible with no `shared_from_this` and no retention of any kind.
         *
         * .NET does not need one because the GC keeps the object alive for as long as the
         * captured delegate can reach it. C++ has no such mechanism, and this port's answer is
         * the RAII one: destruction and **both sides of a move** wait before replacing storage --
         * the same shape `FileSystemWatcher` took in #2347. `Close()` cannot simply run first,
         * because a worker blocked in `recv()`/`accept()` would then be operating on a descriptor
         * number the process may already have reused. Move-assignment reopens its drained guard
         * so new work on the destination is accepted after the transfer (#2417).
         *
         * Defined in the `.cpp`; never null on a live `Socket`.
         */
        struct AsyncOperations;
        /**
         * Owns one in-flight count from before Task construction until the async body finishes.
         * The caller first owns the shared registration, then the task callable moves its capture
         * into a body-local owner on entry. That move matters: `std::async` may retain the callable
         * until its future is destroyed, whereas the boundary must release when work finishes.
         * If callable allocation or task launch throws first, ordinary shared ownership rolls the
         * caller-side registration back instead of leaking a count forever (#2417).
         */
        struct AsyncOperationRegistration;
        std::shared_ptr<AsyncOperations> asyncOps_;
        using BeforeAsyncTaskTestHook = void (*)();
        static std::atomic<BeforeAsyncTaskTestHook> beforeAsyncTaskTestHook_;

        /**
         * Registers one in-flight async operation, and returns the guard the operation's body
         * holds for its lifetime. Called on the CALLER's thread, before the task is constructed:
         * doing it inside the task body would leave a window in which the socket is destroyed
         * before the body starts, the wait sees no operation, and the boundary does nothing.
         */
        [[nodiscard]] std::shared_ptr<AsyncOperationRegistration>
        beginAsyncOperation(bool interruptWithShutdown);
        [[nodiscard]] int asyncOperationCountForTesting() const;

        /**
         * Stops AcceptAsync and waits until every in-flight async operation has finished.
         * When @p discardDescriptor is true, also shuts down blocking Connect/Send/Receive work;
         * callers using that mode must close the descriptor immediately afterwards. A source-side
         * move passes false and may therefore wait for blocking I/O to complete naturally, because
         * shutdown is irreversible and would transfer a damaged resource (#2417).
         * Marks this particular guard terminal even when it is idle; move-assignment explicitly
         * reopens the drained destination guard before returning.
         */
        void waitForAsyncOperations(bool discardDescriptor) noexcept;

        Socket(intcs fd, System::Net::Sockets::AddressFamily family, System::Net::Sockets::SocketType type,
               System::Net::Sockets::ProtocolType protocol);

    public:
        /** @brief Creates a new socket of the given address family, type, and protocol. */
        Socket(System::Net::Sockets::AddressFamily addressFamily, System::Net::Sockets::SocketType socketType,
               System::Net::Sockets::ProtocolType protocolType);

        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        /** @return The address family of this socket. */
        [[nodiscard]] System::Net::Sockets::AddressFamily getAddressFamilyProperty() const { return addressFamily_; }
        /** @return The socket type. */
        [[nodiscard]] System::Net::Sockets::SocketType getSocketTypeProperty() const { return socketType_; }
        /** @return The protocol type. */
        [[nodiscard]] System::Net::Sockets::ProtocolType getProtocolTypeProperty() const { return protocolType_; }
        /** @return true if Connect() has succeeded and the socket has not since been closed/shutdown. */
        [[nodiscard]] bool getConnectedProperty() const { return connected_; }
        /** @return true if Bind() has been called on this socket. */
        [[nodiscard]] bool getIsBoundProperty() const { return bound_; }
        /** @return The underlying native socket descriptor/handle. */
        [[nodiscard]] intcs getHandleProperty() const { return fd_; }

        /** @return The local endpoint, or nullptr if the socket is not bound. */
        [[nodiscard]] std::shared_ptr<System::Net::EndPoint> getLocalEndPointProperty() const;
        /** @return The remote endpoint, or nullptr if the socket is not connected. */
        [[nodiscard]] std::shared_ptr<System::Net::EndPoint> getRemoteEndPointProperty() const;
        /** @return The number of bytes available to read without blocking. */
        [[nodiscard]] intcs getAvailableProperty() const;

        /** @return true if the socket is in blocking mode (the default). */
        [[nodiscard]] bool getBlockingProperty() const { return blocking_; }
        /** @brief Sets whether the socket operates in blocking mode. */
        void setBlockingProperty(bool value);

        /** @return The receive timeout in milliseconds (0 = infinite). */
        [[nodiscard]] intcs getReceiveTimeoutProperty() const;
        /** @brief Sets the receive timeout in milliseconds (0 = infinite). */
        void setReceiveTimeoutProperty(intcs value);
        /** @return The send timeout in milliseconds (0 = infinite). */
        [[nodiscard]] intcs getSendTimeoutProperty() const;
        /** @brief Sets the send timeout in milliseconds (0 = infinite). */
        void setSendTimeoutProperty(intcs value);
        /** @return The receive buffer size in bytes. */
        [[nodiscard]] intcs getReceiveBufferSizeProperty() const;
        /** @brief Sets the receive buffer size in bytes. */
        void setReceiveBufferSizeProperty(intcs value);
        /** @return The send buffer size in bytes. */
        [[nodiscard]] intcs getSendBufferSizeProperty() const;
        /** @brief Sets the send buffer size in bytes. */
        void setSendBufferSizeProperty(intcs value);
        /** @return true if SO_REUSEADDR is disabled (exclusive bind). */
        [[nodiscard]] bool getExclusiveAddressUseProperty() const;
        /** @brief Enables/disables exclusive use of the bound address. Must be set before Bind(). */
        void setExclusiveAddressUseProperty(bool value);
        /** @return true if the Nagle algorithm is disabled (Tcp sockets only). */
        [[nodiscard]] bool getNoDelayProperty() const;
        /** @brief Enables/disables the Nagle algorithm (Tcp sockets only). */
        void setNoDelayProperty(bool value);
        /** @return The IP time-to-live value. */
        [[nodiscard]] intcs getTtlProperty() const;
        /** @brief Sets the IP time-to-live value. */
        void setTtlProperty(intcs value);
        /** @return true if outgoing IP packets have the "don't fragment" bit set. */
        [[nodiscard]] bool getDontFragmentProperty() const;
        /** @brief Sets whether outgoing IP packets have the "don't fragment" bit set. */
        void setDontFragmentProperty(bool value);
        /** @return true if outgoing multicast packets are looped back to the sender. */
        [[nodiscard]] bool getMulticastLoopbackProperty() const;
        /** @brief Sets whether outgoing multicast packets are looped back to the sender. */
        void setMulticastLoopbackProperty(bool value);
        /** @return true if sending broadcast datagrams is allowed (Udp sockets). */
        [[nodiscard]] bool getEnableBroadcastProperty() const;
        /** @brief Enables/disables sending broadcast datagrams (Udp sockets). */
        void setEnableBroadcastProperty(bool value);
        /** @return The current SO_LINGER setting. */
        [[nodiscard]] LingerOption getLingerStateProperty() const;
        /** @brief Sets the SO_LINGER setting. */
        void setLingerStateProperty(const LingerOption& value);

        /** @brief Associates this socket with a local endpoint. */
        void Bind(const System::Net::EndPoint& localEP);

        /** @brief Establishes a connection to a remote endpoint. */
        void Connect(const System::Net::EndPoint& remoteEP);
        /** @brief Establishes a connection to the given address and port. */
        void Connect(const System::Net::IPAddress& address, intcs port);
        /** @brief Resolves @p host and establishes a connection on @p port. */
        void Connect(const std::string& host, intcs port);

        /** @brief Places this socket into a listening state. */
        void Listen(intcs backlog);
        /** @brief Places this socket into a listening state, using the OS's default backlog. */
        void Listen();

        /** @brief Blocks until an incoming connection is accepted, returning the new socket. */
        [[nodiscard]] std::shared_ptr<Socket> Accept();

        /** @brief Sends bytes @p offset..@p offset+@p count from @p buffer. @return Bytes sent. */
        intcs Send(const std::vector<bytecs>& buffer, intcs offset, intcs count, SocketFlags flags);
        /** @brief Sends the entire buffer. @return Bytes sent. */
        intcs Send(const std::vector<bytecs>& buffer, SocketFlags flags = SocketFlags::None);

        /** @brief Receives into @p buffer at @p offset, up to @p count bytes. @return Bytes received. */
        intcs Receive(std::vector<bytecs>& buffer, intcs offset, intcs count, SocketFlags flags);
        /** @brief Receives into the entire buffer. @return Bytes received. */
        intcs Receive(std::vector<bytecs>& buffer, SocketFlags flags = SocketFlags::None);

        /** @brief Sends a datagram to @p remoteEP. @return Bytes sent. */
        intcs SendTo(const std::vector<bytecs>& buffer, intcs offset, intcs count, SocketFlags flags,
                     const System::Net::EndPoint& remoteEP);
        /** @brief Sends a datagram (entire buffer) to @p remoteEP. @return Bytes sent. */
        intcs SendTo(const std::vector<bytecs>& buffer, const System::Net::EndPoint& remoteEP);

        /**
         * @brief Receives a datagram, reporting the sender's endpoint.
         * @param remoteEPTemplate An endpoint of the expected remote family (e.g. an IPEndPoint),
         * used only to determine how to decode the sender's address.
         */
        SocketReceiveFromResult ReceiveFrom(std::vector<bytecs>& buffer, intcs offset, intcs count, SocketFlags flags,
                                             const System::Net::EndPoint& remoteEPTemplate);
        /** @brief As above, receiving into the entire buffer with no flags. */
        SocketReceiveFromResult ReceiveFrom(std::vector<bytecs>& buffer, const System::Net::EndPoint& remoteEPTemplate);

        /** @brief Disables send and/or receive on this socket. */
        void Shutdown(SocketShutdown how);

        /**
         * @brief Wakes and drains registered asynchronous work, then closes the socket.
         * @note May block until an already-started async body has crossed its teardown boundary.
         */
        void Close();
        /** @brief Equivalent to Close(). */
        void Dispose() { Close(); }

        /** @brief Polls the socket for the given condition, waiting up to @p microSeconds. */
        [[nodiscard]] bool Poll(longcs microSeconds, SelectMode mode) const;

        /** @brief Sets an integer-valued socket option. */
        void SetSocketOption(SocketOptionLevel level, SocketOptionName name, intcs value);
        /** @brief Sets a boolean-valued socket option. */
        void SetSocketOption(SocketOptionLevel level, SocketOptionName name, bool value);
        /** @brief Sets the SO_LINGER socket option. */
        void SetSocketOption(SocketOptionLevel level, SocketOptionName name, const LingerOption& value);
        /** @return An integer-valued socket option. */
        [[nodiscard]] intcs GetSocketOption(SocketOptionLevel level, SocketOptionName name) const;

        /**
         * @brief Asynchronous counterpart of Connect(remoteEP).
         * @note The operation registers with the socket's in-flight-work boundary before the
         * task is created. Destruction, move construction, and both sides of move-assignment wake
         * blocked I/O and wait for all registered work, so the raw `this` capture cannot outlive
         * or race replacement of the object (tickets #2134/#2417).
         */
        [[nodiscard]] System::Threading::Tasks::TaskT<bool> ConnectAsync(const System::Net::EndPoint& remoteEP);
        /** @brief Asynchronous counterpart of Accept(). Uses the same lifetime boundary as ConnectAsync. */
        [[nodiscard]] System::Threading::Tasks::TaskT<std::shared_ptr<Socket>> AcceptAsync();
        /** @brief Asynchronous counterpart of Send(buffer, flags). Uses the same lifetime boundary as ConnectAsync. */
        [[nodiscard]] System::Threading::Tasks::TaskT<intcs> SendAsync(std::vector<bytecs> buffer,
                                                                        SocketFlags flags = SocketFlags::None);
        /**
         * @brief Asynchronous counterpart of Receive(*buffer, flags). @p buffer is shared so the
         * caller can observe the bytes written once the task completes (there is no Memory<T>
         * idiom here to express an in-place async fill more directly).
         * Uses the same lifetime boundary as ConnectAsync.
         */
        [[nodiscard]] System::Threading::Tasks::TaskT<intcs>
        ReceiveAsync(std::shared_ptr<std::vector<bytecs>> buffer, SocketFlags flags = SocketFlags::None);

        /** @return true if this platform supports IPv4 sockets. */
        [[nodiscard]] static bool getOSSupportsIPv4Property();
        /** @return true if this platform supports IPv6 sockets. */
        [[nodiscard]] static bool getOSSupportsIPv6Property();
        /** @return true if this platform supports Unix domain sockets. */
        [[nodiscard]] static bool getOSSupportsUnixDomainSocketsProperty();
    };

} // namespace System::Net::Sockets
