// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <memory>

#include "System/Net/Http/HttpMessageHandler.hpp"

namespace System::Net::Http {

/**
 * @brief A base type that sends HTTP request messages through a handler.
 *
 * C++ counterpart of .NET System.Net.Http.HttpMessageInvoker.  It owns a
 * shared handler reference and, when requested, invokes the handler's
 * explicit Dispose() method when this invoker is disposed.
 */
class HttpMessageInvoker {
public:
    /** Initializes an invoker that disposes its handler. */
    explicit HttpMessageInvoker(std::shared_ptr<HttpMessageHandler> handler);

    /**
     * @brief Initializes an invoker with an explicit handler disposal policy.
     * @param handler Terminal or delegating handler used to send requests.
     * @param disposeHandler Whether Dispose() also disposes @p handler.
     */
    HttpMessageInvoker(std::shared_ptr<HttpMessageHandler> handler, bool disposeHandler);

    virtual ~HttpMessageInvoker();

    HttpMessageInvoker(const HttpMessageInvoker&) = delete;
    HttpMessageInvoker& operator=(const HttpMessageInvoker&) = delete;

    /** Sends a request synchronously using a non-cancelled token. */
    virtual std::shared_ptr<HttpResponseMessage> Send(std::shared_ptr<HttpRequestMessage> request);

    /** Sends a request synchronously with cooperative pre-send cancellation. */
    virtual std::shared_ptr<HttpResponseMessage> Send(
        std::shared_ptr<HttpRequestMessage> request,
        const System::Threading::CancellationToken& cancellationToken);

    /** Sends a request asynchronously using a non-cancelled token. */
    virtual System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>> SendAsync(
        std::shared_ptr<HttpRequestMessage> request);

    /** Sends a request asynchronously with cooperative pre-send cancellation. */
    virtual System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>> SendAsync(
        std::shared_ptr<HttpRequestMessage> request,
        const System::Threading::CancellationToken& cancellationToken);

    /** Releases this invoker and optionally disposes its handler. */
    void Dispose();

protected:
    /** Implements the disposal pattern for derived invokers. */
    virtual void Dispose(bool disposing);

private:
    std::shared_ptr<HttpMessageHandler> handler_;
    bool disposeHandler_;
    bool disposed_ = false;

    void ThrowIfDisposed() const;
};

} // namespace System::Net::Http
