// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/Net/Http/HttpMessageInvoker.hpp"

#include "System/ArgumentNullException.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::Net::Http {

HttpMessageInvoker::HttpMessageInvoker(std::shared_ptr<HttpMessageHandler> handler)
    : HttpMessageInvoker(std::move(handler), true) {}

HttpMessageInvoker::HttpMessageInvoker(std::shared_ptr<HttpMessageHandler> handler, bool disposeHandler)
    : handler_(std::move(handler)), disposeHandler_(disposeHandler) {
    System::ArgumentNullException::ThrowIfNull(handler_.get(), "handler");
}

HttpMessageInvoker::~HttpMessageInvoker() {
    Dispose();
}

std::shared_ptr<HttpResponseMessage> HttpMessageInvoker::Send(
    std::shared_ptr<HttpRequestMessage> request)
{
    return Send(std::move(request), System::Threading::CancellationToken::None());
}

std::shared_ptr<HttpResponseMessage> HttpMessageInvoker::Send(
    std::shared_ptr<HttpRequestMessage> request,
    const System::Threading::CancellationToken& cancellationToken)
{
    System::ArgumentNullException::ThrowIfNull(request.get(), "request");
    ThrowIfDisposed();
    return handler_->Send(std::move(request), cancellationToken);
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>> HttpMessageInvoker::SendAsync(
    std::shared_ptr<HttpRequestMessage> request)
{
    return SendAsync(std::move(request), System::Threading::CancellationToken::None());
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>> HttpMessageInvoker::SendAsync(
    std::shared_ptr<HttpRequestMessage> request,
    const System::Threading::CancellationToken& cancellationToken)
{
    System::ArgumentNullException::ThrowIfNull(request.get(), "request");
    ThrowIfDisposed();
    return handler_->SendAsync(std::move(request), cancellationToken);
}

void HttpMessageInvoker::Dispose() {
    Dispose(true);
}

void HttpMessageInvoker::Dispose(bool disposing) {
    if (disposing && !disposed_) {
        disposed_ = true;
        if (disposeHandler_) handler_->Dispose();
    }
}

void HttpMessageInvoker::ThrowIfDisposed() const {
    System::ObjectDisposedException::ThrowIf(disposed_, "HttpMessageInvoker");
}

} // namespace System::Net::Http
