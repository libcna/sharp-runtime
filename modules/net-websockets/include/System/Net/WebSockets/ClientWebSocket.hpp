// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/WebSockets/ClientWebSocketOptions.hpp"
#include "System/Net/WebSockets/WebSocket.hpp"
#include "System/Net/WebSockets/WebSocketMessageType.hpp"
#include "System/Uri.hpp"

namespace System::Net::WebSockets {

    /**
     * @brief Provides a client for connecting to WebSocket services.
     *
     * C++ counterpart of .NET System.Net.WebSockets.ClientWebSocket.
     *
     * @note Real RFC 6455 implementation over a raw `System::Net::Sockets::Socket` TCP
     * connection: performs the HTTP/1.1 Upgrade handshake (including the
     * `Sec-WebSocket-Accept` SHA-1 digest check) and does real client-masked frame
     * send/receive, including transparent ping/pong and the close handshake.
     * @note `ws://` only — `wss://` throws `System::PlatformNotSupportedException`, matching
     * this runtime's `HttpClient`, which also has no TLS support.
     * @note No permessage-deflate compression (`ClientWebSocketOptions::DangerousDeflateOptions`
     * is not reproduced — see that class's doc comment). `HttpStatusCode`/`HttpResponseHeaders`
     * (gated on `CollectHttpResponseDetails` in .NET) and the `HttpMessageInvoker`-based
     * `ConnectAsync` overload are not reproduced — there is no `HttpMessageInvoker` concept in
     * this runtime's simplified `HttpClient`.
     */
    class ClientWebSocket : public WebSocket {
        ClientWebSocketOptions options_;

        /**
         * Ticket #2096. This was a `std::unique_ptr`, and `Dispose()`/`Abort()` reset it while a
         * task thread was inside `socket_->Send` or `readExact(*socket_, …)` — an unguarded
         * **null dereference**, with the `ThrowOnInvalidState` check in `SendAsync`/`ReceiveAsync`
         * a TOCTOU against it. Shared ownership is the repair: an operation takes a strong
         * reference under @ref stateMutex_ before any I/O, so the socket cannot be destroyed
         * underneath it, and `Dispose()` hands its own reference over instead of deleting.
         */
        std::shared_ptr<System::Net::Sockets::Socket> socket_;

        /**
         * Guards @ref socket_, @ref state_, @ref closeStatus_, @ref closeStatusDescription_ and
         * @ref subProtocol_ — every member a task thread writes and a public getter reads.
         *
         * Ticket #2096. The finding named `state_`; measured, **four** members had the same
         * unsynchronised write-from-a-task-thread / read-from-a-public-getter shape. .NET makes
         * its own state atomic for exactly this reason — `Interlocked.CompareExchange(ref _state,
         * …)` in `ClientWebSocket.cs:104,132`.
         *
         * **This mutex is never held across socket I/O.** It is taken to read or publish a field,
         * or to take a strong reference to the socket, and released before anything can block —
         * otherwise `Dispose()` would deadlock behind a `ReceiveAsync` waiting for a frame the
         * peer may never send.
         */
        mutable std::mutex stateMutex_;

        WebSocketState state_ = WebSocketState::None;
        bool connectStarted_ = false;
        std::optional<std::string> subProtocol_;
        std::optional<WebSocketCloseStatus> closeStatus_;
        std::optional<std::string> closeStatusDescription_;
        bool sendContinuation_ = false;
        WebSocketMessageType fragmentType_ = WebSocketMessageType::Binary;
        std::vector<SharpRuntime::bytecs> recvLeftover_;
        size_t recvLeftoverPos_ = 0;
        WebSocketMessageType recvLeftoverType_ = WebSocketMessageType::Binary;
        std::mutex sendMutex_;

        void performHandshake(const System::Uri& uri);
        void sendFrame(SharpRuntime::bytecs opcode, const SharpRuntime::bytecs* data, size_t len, bool fin);
        struct RawFrame {
            SharpRuntime::bytecs opcode = 0;
            bool fin = false;
            std::vector<SharpRuntime::bytecs> payload;
        };
        RawFrame readFrame();
        void sendCloseFrame(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription);

        /**
         * @brief A strong reference to the live socket, or a throw if there is none.
         *
         * Ticket #2096. Every I/O path calls this instead of touching @ref socket_ directly, so
         * the window between "the state said Open" and "the socket is used" cannot end in a null
         * dereference. It raises the **same** `WebSocketException(InvalidState)` the state check
         * would have raised a microsecond earlier, deliberately: a caller racing `Dispose()`
         * against `SendAsync` cannot tell which side it landed on, and should not have to.
         *
         * .NET raises `ObjectDisposedException` here (`ClientWebSocket.cs:166`), but it raises it
         * for the *non-racy* path too, where this port has always raised
         * `WebSocketException(InvalidState)`. Matching .NET on one side of the race and not the
         * other would be worse than either; the exception-type question is ticket **#2357**.
         */
        [[nodiscard]] std::shared_ptr<System::Net::Sockets::Socket> socketForIo() const;

        /** @brief Reads @ref state_ under @ref stateMutex_. */
        [[nodiscard]] WebSocketState loadState() const;
        /** @brief Writes @ref state_ under @ref stateMutex_. */
        void storeState(WebSocketState next);

    public:
        ClientWebSocket() = default;
        /**
         * @brief Destroys the socket, waiting for any in-flight `*Async` body to finish.
         *
         * Ticket #2088 / SR-AUD-247 (CCF-019). Every `*Async` member returns a task whose body
         * captures raw `this` and runs on a `std::async` thread; nothing kept this object alive
         * until it ran, and the audit confirmed the use-after-free under ASan.
         *
         * .NET needs no boundary because the GC keeps the socket alive for as long as the
         * captured delegate can reach it. C++ has no such mechanism, so this port takes the RAII
         * answer -- the same shape `Socket` (#2134) and `FileSystemWatcher` (#2347) took.
         * `Dispose()` still runs afterwards, so the close handshake is unchanged.
         */
        ~ClientWebSocket() override;

        /** @return The options used to configure the connection. Must be set before ConnectAsync(). */
        [[nodiscard]] ClientWebSocketOptions& getOptionsProperty() { return options_; }

    private:
        /// The liveness boundary for the five `*Async` members (#2088). Defined in the `.cpp`.
        /// See `Socket`'s equivalent for why the decrement must be body-local rather than
        /// captured by the lambda.
        struct AsyncOperations;
        struct AsyncOperationScope;
        std::shared_ptr<AsyncOperations> asyncOps_ = nullptr;

        [[nodiscard]] std::shared_ptr<AsyncOperations> beginAsyncOperation();
        void waitForAsyncOperations() noexcept;

    public:

        // Ticket #2096: all four take stateMutex_. Each is written by a task thread and read here
        // from whichever thread the caller happens to be on, which was an unsynchronised race on
        // a public property -- undefined behaviour, not merely a stale read. Defined in the .cpp
        // so the mutex is not part of the header's inline surface.
        [[nodiscard]] std::optional<WebSocketCloseStatus> getCloseStatusProperty() const override;
        [[nodiscard]] std::optional<std::string> getCloseStatusDescriptionProperty() const override;
        [[nodiscard]] std::optional<std::string> getSubProtocolProperty() const override;
        [[nodiscard]] WebSocketState getStateProperty() const override;

        /** @brief Connects to the WebSocket server at @p uri (scheme must be "ws"; "wss" is unsupported — no TLS). */
        System::Threading::Tasks::Task
        ConnectAsync(const System::Uri& uri,
                     System::Threading::CancellationToken cancellationToken = System::Threading::CancellationToken::None());

        void Abort() override;
        System::Threading::Tasks::Task
        CloseAsync(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
                   System::Threading::CancellationToken cancellationToken = System::Threading::CancellationToken::None()) override;
        System::Threading::Tasks::Task CloseOutputAsync(
            WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
            System::Threading::CancellationToken cancellationToken = System::Threading::CancellationToken::None()) override;
        void Dispose() override;

        // Un-hide WebSocket's non-virtual whole-buffer convenience overloads — declaring the
        // full-signature overrides below would otherwise hide them (any derived-class overload
        // of a name hides all base-class overloads of that name).
        using WebSocket::ReceiveAsync;
        using WebSocket::SendAsync;

        System::Threading::Tasks::TaskT<WebSocketReceiveResult>
        ReceiveAsync(std::vector<SharpRuntime::bytecs>& buffer, SharpRuntime::intcs offset, SharpRuntime::intcs count,
                     System::Threading::CancellationToken cancellationToken = System::Threading::CancellationToken::None()) override;
        System::Threading::Tasks::Task
        SendAsync(const std::vector<SharpRuntime::bytecs>& buffer, SharpRuntime::intcs offset, SharpRuntime::intcs count,
                  WebSocketMessageType messageType, bool endOfMessage,
                  System::Threading::CancellationToken cancellationToken = System::Threading::CancellationToken::None()) override;
    };

} // namespace System::Net::WebSockets
