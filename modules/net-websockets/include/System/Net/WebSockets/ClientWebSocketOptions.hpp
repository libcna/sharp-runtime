// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include "System/String.hpp"
#include "System/StringComparison.hpp"
#include "System/TimeSpan.hpp"
#include "System/Threading/Timeout.hpp"

namespace System::Net::WebSockets {

    class ClientWebSocket;

    /**
     * @brief Options to control the behavior of a ClientWebSocket connection.
     *
     * C++ counterpart of .NET System.Net.WebSockets.ClientWebSocketOptions.
     *
     * @note Reduced scope: no `Credentials`/`Proxy`/`Cookies`/`ClientCertificates`/
     * `RemoteCertificateValidationCallback`/`UseDefaultCredentials`/`HttpVersion(Policy)` — this
     * runtime's `HttpClient` has no credential/proxy/cookie/certificate infrastructure to hook
     * into, and there is no TLS support (matching `HttpClient`'s own documented no-TLS
     * limitation). `DangerousDeflateOptions` (permessage-deflate) is also not reproduced —
     * `WebSocketDeflateOptions` is out of scope in this runtime.
     */
    class ClientWebSocketOptions {
        bool isReadOnly_ = false;
        System::TimeSpan keepAliveInterval_ = System::TimeSpan::FromSeconds(30);
        System::TimeSpan keepAliveTimeout_ = System::TimeSpan::FromTicks(System::Threading::Timeout::InfiniteTimeSpan);
        SharpRuntime::intcs receiveBufferSize_ = 0x1000;
        SharpRuntime::intcs sendBufferSize_ = 0x1000;
        bool collectHttpResponseDetails_ = false;
        std::map<std::string, std::string> requestHeaders_;
        std::vector<std::string> requestedSubProtocols_;

        void throwIfReadOnly() const {
            if (isReadOnly_) {
                throw System::InvalidOperationException("The WebSocket has already been started.");
            }
        }

        /**
         * @brief Rejects a subprotocol that is not a single RFC 7230 `token`.
         *
         * Ticket #2089 / SR-AUD-249. The rejection set used to be `c <= 0x20 || c == 0x7F`,
         * which catches a space, a control character and DEL but **not one** of the RFC 7230
         * separators. `chat,evil` and `a;b` were therefore accepted as a *single* subprotocol
         * and joined into `Sec-WebSocket-Protocol` with `", "`, so the caller advertised two
         * protocols where the API reported one — measured in
         * `build-probe/2089_probe1_before.log`.
         *
         * The finding's own premise needed correcting in the port's favour and against it at
         * once (plan §6.2): a validator **did** exist and already rejected the control range,
         * so this widens an existing check rather than adding a missing one. Everything the
         * old check rejected is still rejected — CR, LF and NUL are all `<= 0x20`, so a
         * subprotocol never was a `ContainsProtocolFieldTerminator` door.
         *
         * `tchar` (RFC 7230 §3.2.6) is every VCHAR except the separators below, so `-`, `.`,
         * `_`, `+`, `!`, `#`, `$`, `%`, `&`, `'`, `*`, `^`, backtick, `|`, `~`, digits and
         * letters all stay accepted.
         *
         * @throws System::ArgumentException if @p subProtocol is empty or is not a token.
         */
        static void validateSubprotocol(const std::string& subProtocol) {
            if (subProtocol.empty()) {
                throw System::ArgumentException("Empty string is not a valid subprotocol value.", "subProtocol");
            }
            for (unsigned char c : subProtocol) {
                if (c <= 0x20 || c >= 0x7F || isSeparator(static_cast<char>(c))) {
                    throw System::ArgumentException("The subprotocol contains invalid characters.", "subProtocol");
                }
            }
        }

        /// The RFC 7230 §3.2.6 separator set — every VCHAR that `tchar` excludes.
        static constexpr bool isSeparator(char c) {
            switch (c) {
                case '(': case ')': case '<': case '>': case '@':
                case ',': case ';': case ':': case '\\': case '"':
                case '/': case '[': case ']': case '?': case '=':
                case '{': case '}':
                    return true;
                default:
                    return false;
            }
        }

        /**
         * @brief Rejects handshake header text that would terminate the field it is written into.
         *
         * Ticket #2089 / SR-AUD-248. `ClientWebSocket::performHandshake` concatenates
         * `name + ": " + value + "\r\n"` straight into the upgrade request, so before this a
         * value carrying CRLF injected an additional header field and a *name* carrying CRLF
         * injected a whole request line — `build-probe/2089_probe1_before.log` shows a
         * six-field request becoming an eight-field one carrying a smuggled
         * `GET /admin HTTP/1.1`.
         *
         * The predicate is `System::Net::detail::ContainsProtocolFieldTerminator`, the single
         * shared body that `System::Net::Http` #2063 established for the same rule across ten
         * HTTP doors. It is deliberately **not** a second local copy of the policy.
         *
         * @note The exception is a `System::ArgumentException` rather than the
         *       `System::FormatException` that `System::Net::Http`'s protocol-field doors
         *       throw, because within *this* class the sibling character check
         *       (`AddSubProtocol`) already reports invalid characters as an argument error.
         *       Reporting one door's bad characters as an argument error and its neighbour's
         *       as a format error would be arbitrary; #2063 resolved the same tension the same
         *       way for its multipart doors. The reference tree is absent, so this is recorded
         *       as **this port's choice**, not as a match to .NET.
         *
         * @note The offending text is deliberately not echoed into the message.
         */
        static void validateHeaderField(const std::string& text, const char* paramName) {
            if (System::Net::detail::ContainsProtocolFieldTerminator(text)) {
                throw System::ArgumentException(
                    "A WebSocket handshake header must not contain a carriage return, a line "
                    "feed or a NUL character.",
                    std::string(paramName));
            }
        }

        friend class ClientWebSocket;
        void setToReadOnly() { isReadOnly_ = true; }

    public:
        ClientWebSocketOptions() = default;

        /**
         * @brief Sets an HTTP header to send during the WebSocket handshake.
         *
         * @throws System::InvalidOperationException if the WebSocket has already been started.
         * @throws System::ArgumentException if @p headerName or @p headerValue contains a
         *         carriage return, a line feed or a NUL character (ticket #2089): the header is
         *         concatenated into the upgrade request verbatim, so such text would inject an
         *         extra header field or a whole request line. The rejection happens at this
         *         door, before any socket is opened.
         */
        void SetRequestHeader(const std::string& headerName, const std::string& headerValue) {
            throwIfReadOnly();
            validateHeaderField(headerName, "headerName");
            validateHeaderField(headerValue, "headerValue");
            requestHeaders_[headerName] = headerValue;
        }

        /** @return The headers set via SetRequestHeader(), keyed by header name. */
        [[nodiscard]] const std::map<std::string, std::string>& getRequestHeadersProperty() const { return requestHeaders_; }

        /**
         * @brief Adds a sub-protocol to request during the handshake.
         * @note Verified against ClientWebSocketOptions.cs's AddSubProtocol: the duplicate
         * check compares via StringComparison.OrdinalIgnoreCase, not an exact match -- this
         * previously used a case-sensitive comparison, silently allowing e.g. "chat" and
         * "Chat" to both be added as if they were distinct protocols.
         */
        void AddSubProtocol(const std::string& subProtocol) {
            throwIfReadOnly();
            validateSubprotocol(subProtocol);
            for (const auto& existing : requestedSubProtocols_) {
                if (System::String::Equals(existing, subProtocol, System::StringComparison::OrdinalIgnoreCase)) {
                    throw System::ArgumentException("Duplicate protocols are not allowed: " + subProtocol, "subProtocol");
                }
            }
            requestedSubProtocols_.push_back(subProtocol);
        }

        /** @return The sub-protocols requested via AddSubProtocol(). */
        [[nodiscard]] const std::vector<std::string>& getRequestedSubProtocolsProperty() const { return requestedSubProtocols_; }

        /** @return The keep-alive interval, or TimeSpan::Zero/InfiniteTimeSpan to disable keep-alives. */
        [[nodiscard]] System::TimeSpan getKeepAliveIntervalProperty() const { return keepAliveInterval_; }
        /** @brief Sets the keep-alive interval. */
        void setKeepAliveIntervalProperty(System::TimeSpan value) {
            throwIfReadOnly();
            if (value.getTicksProperty() != System::Threading::Timeout::InfiniteTimeSpan && value.getTicksProperty() < 0) {
                throw System::ArgumentOutOfRangeException("value", "The TimeSpan must be non-negative or Timeout.InfiniteTimeSpan.");
            }
            keepAliveInterval_ = value;
        }

        /** @return The timeout to wait for a PONG in response to a PING. */
        [[nodiscard]] System::TimeSpan getKeepAliveTimeoutProperty() const { return keepAliveTimeout_; }
        /** @brief Sets the timeout to wait for a PONG in response to a PING. */
        void setKeepAliveTimeoutProperty(System::TimeSpan value) {
            throwIfReadOnly();
            if (value.getTicksProperty() != System::Threading::Timeout::InfiniteTimeSpan && value.getTicksProperty() < 0) {
                throw System::ArgumentOutOfRangeException("value", "The TimeSpan must be non-negative or Timeout.InfiniteTimeSpan.");
            }
            keepAliveTimeout_ = value;
        }

        /** @brief Sets the internal send/receive buffer sizes to use once connected. */
        void SetBuffer(SharpRuntime::intcs receiveBufferSize, SharpRuntime::intcs sendBufferSize) {
            throwIfReadOnly();
            System::ArgumentOutOfRangeException::ThrowIfNegativeOrZero(receiveBufferSize, "receiveBufferSize");
            System::ArgumentOutOfRangeException::ThrowIfNegativeOrZero(sendBufferSize, "sendBufferSize");
            receiveBufferSize_ = receiveBufferSize;
            sendBufferSize_ = sendBufferSize;
        }

        /** @return The configured receive buffer size. */
        [[nodiscard]] SharpRuntime::intcs getReceiveBufferSizeProperty() const { return receiveBufferSize_; }
        /** @return The configured send buffer size. */
        [[nodiscard]] SharpRuntime::intcs getSendBufferSizeProperty() const { return sendBufferSize_; }

        /** @return true if HttpStatusCode/HttpResponseHeaders should be captured during ConnectAsync(). */
        [[nodiscard]] bool getCollectHttpResponseDetailsProperty() const { return collectHttpResponseDetails_; }
        /** @brief Sets whether HttpStatusCode/HttpResponseHeaders should be captured during ConnectAsync(). */
        void setCollectHttpResponseDetailsProperty(bool value) {
            throwIfReadOnly();
            collectHttpResponseDetails_ = value;
        }
    };

} // namespace System::Net::WebSockets
