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
#include "System/Threading/CancellationTokenRegistration.hpp"
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
        /**
         * Whether the frame the leftover came from was final.
         *
         * Ticket #2095. Draining the last of a leftover used to report `endOfMessage = true`
         * from the buffer's own exhaustion, so the tail of a **non-final** frame claimed the
         * message had ended. .NET reports `header.EndOfMessage`, which is the frame's FIN.
         */
        bool recvLeftoverFinal_ = true;
        /**
         * .NET's `_lastReceiveHeader.Fin`, initialised true exactly as .NET initialises it
         * (`ManagedWebSocket.cs:98` — `{ Opcode = Text, Fin = true, Processed = true }`).
         *
         * Ticket #2095. Only **data** frames update it; control frames do not, matching .NET,
         * which assigns `_lastReceiveHeader` only after a continuation has been rewritten to its
         * parent opcode.
         */
        bool lastReceivedFrameWasFinal_ = true;
        std::mutex sendMutex_;

        /**
         * @brief The keep-alive heartbeat, or null when the options disable it.
         *
         * Ticket #2094. `KeepAliveInterval` and `KeepAliveTimeout` were validated, stored and
         * returned, and nothing read them. The ticket was blocked on the concurrency it would
         * need — "a BACKGROUND TIMER THREAD in a class that today has exactly one mutex
         * (`sendMutex_`) and already races on `state_` (#2096)". #2096 removed that race and
         * #2093 made `Abort()` safe to call from another thread, so the objection is answered
         * rather than ignored.
         *
         * Owned through a `shared_ptr` so the heartbeat body can hold its own state alive
         * independently of when the owner drops it. Defined in the `.cpp`.
         */
        struct KeepAlive;
        std::shared_ptr<KeepAlive> keepAlive_;

        void performHandshake(const System::Uri& uri);
        /** @brief Starts the heartbeat if the options ask for one. Called once, when Open. */
        void startKeepAlive();
        /** @brief Stops and joins the heartbeat. Never called from the heartbeat thread itself. */
        void stopKeepAlive() noexcept;
        /** @brief The heartbeat body — one tick of .NET's `HeartBeat()`. */
        void keepAliveHeartBeat(const std::shared_ptr<KeepAlive>& state);
        /** @brief Rethrows the keep-alive's own failure when it is the reason an operation failed. */
        void throwIfKeepAliveFaulted() const;
        /** @return A strong reference to the heartbeat state, or null when there is none. */
        [[nodiscard]] std::shared_ptr<KeepAlive> keepAliveState() const;
        /// #2357: .NET's outer `ConnectedWebSocket` gate -- ObjectDisposedException when this
        /// instance is disposed, InvalidOperationException when it was never connected or is
        /// still connecting. Adds no data member; see the definition for the state mapping.
        void throwIfNotConnected() const;

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

        /**
         * @brief Registers `Abort()` on @p token for the lifetime of one operation.
         *
         * Ticket #2093. All five `*Async` members took a `CancellationToken` and none consulted
         * it. The ticket was blocked on a design it expected to be transport-level — a socket
         * timeout, a poll-based non-blocking read loop, or a shutdown-based interrupt — and the
         * reference shows the answer is none of those:
         *
         * ```csharp
         * registration = cancellationToken.Register(static s => ((ManagedWebSocket)s!).Abort(), this);
         * ```
         *
         * `ManagedWebSocket.cs:608,789`. **Cancelling any WebSocket operation aborts the whole
         * WebSocket in .NET** — that is the documented contract, not an implementation shortcut,
         * and it is why no per-read polling is needed. This port's `Abort()` already shuts the
         * socket down under #2096's shared ownership, which is exactly what unblocks a worker
         * parked in `recv()`.
         *
         * The scope also gives the two ends of the operation: a token already cancelled on entry
         * means the body never runs, and a body that failed *because* the token fired reports
         * `TaskCanceledException` rather than the `WebSocketException` the abort produced.
         *
         * @note The registration is disposed by the scope's destructor, outside any lock. The
         *       shared `CancellationTokenRegistration::Dispose()` waits for an in-flight callback,
         *       and that callback takes @ref stateMutex_ — so unregistering while holding it
         *       would deadlock.
         */
        class CancellationScope {
            ClientWebSocket*                              owner_;
            System::Threading::CancellationToken          token_;
            System::Threading::CancellationTokenRegistration registration_;

        public:
            CancellationScope(ClientWebSocket* owner, System::Threading::CancellationToken token);
            ~CancellationScope();
            CancellationScope(const CancellationScope&) = delete;
            CancellationScope& operator=(const CancellationScope&) = delete;

            /** @brief Rethrows a failure as `TaskCanceledException` when the token is the cause. */
            [[noreturn]] void rethrowAsCancelled() const;
            /** @return true if the token has fired. */
            [[nodiscard]] bool cancelled() const;
        };

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
