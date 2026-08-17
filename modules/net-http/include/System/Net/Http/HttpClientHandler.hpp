// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Net/CookieContainer.hpp"
#include "System/Net/Http/HttpMessageHandler.hpp"

namespace System::Net::Http {

    /**
     * @brief The default, socket-level HttpMessageHandler used by HttpClient.
     *
     * C++ counterpart of .NET System.Net.Http.HttpClientHandler. This is the terminal handler
     * that actually performs the HTTP/1.1 request/response exchange over a TCP socket (the
     * logic previously lived directly inside HttpClient::Send; it now lives here so that
     * HttpClient can be pointed at a custom HttpMessageHandler/DelegatingHandler chain
     * instead). Supports HTTP only -- HTTPS/TLS is out of scope for this runtime (see
     * CLAUDE.md's documented deviations).
     */
    class HttpClientHandler : public HttpMessageHandler {
    public:
        HttpClientHandler();
        ~HttpClientHandler() override;

        HttpClientHandler(const HttpClientHandler&) = delete;
        HttpClientHandler& operator=(const HttpClientHandler&) = delete;

        std::shared_ptr<HttpResponseMessage> Send(std::shared_ptr<HttpRequestMessage> request) override;

        /** Releases this handler. Subsequent requests and mutable properties throw. */
        void Dispose() override;

        /** This lightweight handler has no decompression implementation. */
        [[nodiscard]] bool getSupportsAutomaticDecompressionProperty() const noexcept { return false; }

        /** This lightweight handler has no proxy implementation. */
        [[nodiscard]] bool getSupportsProxyProperty() const noexcept { return false; }

        /** This lightweight handler does not implement automatic redirects. */
        [[nodiscard]] bool getSupportsRedirectConfigurationProperty() const noexcept { return false; }

        /**
         * @brief Gets or sets whether the handler sends a Cookie request header (built from
         * getCookieContainerProperty()) and captures Set-Cookie response headers into it.
         * Defaults to true, matching real .NET.
         */
        [[nodiscard]] bool getUseCookiesProperty() const { ThrowIfDisposed(); return useCookies_; }
        void setUseCookiesProperty(bool v) { ThrowIfDisposed(); useCookies_ = v; }

        /**
         * @brief Gets or sets the cookie container used to store server cookies and build the
         * Cookie request header. Defaults to a fresh, empty CookieContainer.
         */
        [[nodiscard]] const std::shared_ptr<System::Net::CookieContainer>& getCookieContainerProperty() const {
            ThrowIfDisposed();
            return cookieContainer_;
        }
        void setCookieContainerProperty(std::shared_ptr<System::Net::CookieContainer> v) {
            ThrowIfDisposed();
            System::ArgumentNullException::ThrowIfNull(v.get(), "value");
            cookieContainer_ = std::move(v);
        }

        /**
         * @brief The largest response body this handler will accumulate, in bytes.
         *
         * Ticket #2071 (SR-AUD-318 limits half). `recvAll`, the `Content-Length`-bounded
         * `recvExact` and the chunked reader all accumulated **without bound**, and
         * `std::stoul(chunkLine, nullptr, 16)` accepted a chunk size up to `SIZE_MAX` — so a
         * hostile or broken server could drive the client out of memory with a header it never
         * had to back with real bytes.
         *
         * The default is .NET's: `HttpContent.MaxBufferSize`, which is `int.MaxValue`
         * (`HttpContent.cs:25`, and `HttpClient`'s constructor assigns it at
         * `HttpClient.cs:149`). Exceeding it raises
         * `HttpRequestException(HttpRequestError::ConfigurationLimitExceeded, …)`, which is the
         * error .NET raises for the same condition (`HttpContent.cs:780`).
         *
         * @note .NET carries this on `HttpClient`, because .NET's handler **streams** and the
         *       client applies the limit afterwards through `LoadIntoBuffer`. This port's
         *       handler reads the whole body eagerly, so the limit has to live where the bytes
         *       are actually accumulated. `HttpClient` forwards to it, so a caller reaches the
         *       same knob under the same name.
         */
        [[nodiscard]] SharpRuntime::longcs getMaxResponseContentBufferSizeProperty() const {
            ThrowIfDisposed();
            return maxResponseContentBufferSize_;
        }
        /**
         * @brief Sets the response-body ceiling.
         * @throws System::ArgumentOutOfRangeException if @p value is not positive, or exceeds
         *         `INTCS_MAX` — both checks transcribed from `HttpClient.cs:117-131`.
         */
        void setMaxResponseContentBufferSizeProperty(SharpRuntime::longcs value) {
            ThrowIfDisposed();
            if (value <= 0) {
                throw System::ArgumentOutOfRangeException("value",
                    "MaxResponseContentBufferSize must be greater than zero.");
            }
            if (value > static_cast<SharpRuntime::longcs>(SharpRuntime::INTCS_MAX)) {
                throw System::ArgumentOutOfRangeException("value",
                    "MaxResponseContentBufferSize must not exceed 2147483647.");
            }
            maxResponseContentBufferSize_ = value;
        }

    private:
        bool useCookies_ = true;
        bool disposed_ = false;
        SharpRuntime::longcs maxResponseContentBufferSize_ =
            static_cast<SharpRuntime::longcs>(SharpRuntime::INTCS_MAX);   // .NET's HttpContent.MaxBufferSize
        std::shared_ptr<System::Net::CookieContainer> cookieContainer_ =
            std::make_shared<System::Net::CookieContainer>();

        void ThrowIfDisposed() const {
            System::ObjectDisposedException::ThrowIf(disposed_, "HttpClientHandler");
        }
    };

} // namespace System::Net::Http
