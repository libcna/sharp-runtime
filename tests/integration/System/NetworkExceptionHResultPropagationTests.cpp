// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <gtest/gtest.h>

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Exception.hpp"
#include "System/FormatException.hpp"
#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/Http/HttpMessageHandler.hpp"
#include "System/Net/Http/HttpMessageInvoker.hpp"
#include "System/Net/Http/HttpRequestError.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/HttpRequestMessage.hpp"
#include "System/Net/Http/HttpResponseMessage.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include "System/Net/WebException.hpp"
#include "System/Net/WebExceptionStatus.hpp"

namespace {

using HResult = SharpRuntime::intcs;
using System::Net::HttpStatusCode;
using System::Net::Http::HttpClient;
using System::Net::Http::HttpMessageHandler;
using System::Net::Http::HttpMessageInvoker;
using System::Net::Http::HttpMethod;
using System::Net::Http::HttpRequestError;
using System::Net::Http::HttpRequestException;
using System::Net::Http::HttpRequestMessage;
using System::Net::Http::HttpResponseMessage;
using System::Net::WebException;
using System::Net::WebExceptionStatus;

constexpr HResult kCorEException = static_cast<HResult>(0x80131500u);
constexpr HResult kCorEFormat = static_cast<HResult>(0x80131537u);
constexpr HResult kCorEInvalidOperation = static_cast<HResult>(0x80131509u);
constexpr HResult kCustomHResult = static_cast<HResult>(0x81234567u);
constexpr HResult kNestedHttpHResult = static_cast<HResult>(0x82345678u);
constexpr HResult kNestedWebHResult = static_cast<HResult>(0x83456789u);

struct InnerCase {
    const char* label;
    std::exception_ptr pointer;
    bool isSystemException;
    HResult hResult;
    std::string message;
    std::type_index dynamicType;
};

std::vector<InnerCase> makeInnerCases() {
    System::Exception defaultInner("default-inner");

    System::Exception zeroInner("zero-inner");
    zeroInner.setHResultProperty(0);

    System::Exception customInner("custom-inner");
    customInner.setHResultProperty(kCustomHResult);

    System::FormatException formatInner("format-inner");

    HttpRequestException nestedHttp("nested-http");
    nestedHttp.setHResultProperty(kNestedHttpHResult);

    WebException nestedWeb("nested-web", WebExceptionStatus::ReceiveFailure);
    nestedWeb.setHResultProperty(kNestedWebHResult);

    return {
        {"null", nullptr, false, 0, "", typeid(void)},
        {"base-default", std::make_exception_ptr(defaultInner), true, kCorEException,
         "default-inner", typeid(System::Exception)},
        {"zero", std::make_exception_ptr(zeroInner), true, 0,
         "zero-inner", typeid(System::Exception)},
        {"custom", std::make_exception_ptr(customInner), true, kCustomHResult,
         "custom-inner", typeid(System::Exception)},
        {"type-specific", std::make_exception_ptr(formatInner), true, kCorEFormat,
         "format-inner", typeid(System::FormatException)},
        {"nested-http", std::make_exception_ptr(nestedHttp), true, kNestedHttpHResult,
         "nested-http", typeid(HttpRequestException)},
        {"nested-web", std::make_exception_ptr(nestedWeb), true, kNestedWebHResult,
         "nested-web", typeid(WebException)},
        {"non-system", std::make_exception_ptr(std::runtime_error("native-inner")), false, 0,
         "native-inner", typeid(std::runtime_error)},
    };
}

void expectInner(const std::exception_ptr& actual, const InnerCase& expected) {
    EXPECT_EQ(actual, expected.pointer);
    if (!expected.pointer) {
        EXPECT_FALSE(actual);
        return;
    }

    std::type_index dynamicType(typeid(void));
    std::string message;
    std::optional<HResult> hResult;
    try {
        std::rethrow_exception(actual);
    } catch (const System::Exception& exception) {
        dynamicType = typeid(exception);
        message = exception.getMessageProperty();
        hResult = exception.getHResultProperty();
    } catch (const std::runtime_error& exception) {
        dynamicType = typeid(exception);
        message = exception.what();
    } catch (...) {
        FAIL() << "unexpected inner exception category";
    }

    EXPECT_EQ(dynamicType, expected.dynamicType);
    EXPECT_EQ(message, expected.message);
    if (expected.isSystemException) {
        ASSERT_TRUE(hResult.has_value());
        EXPECT_EQ(*hResult, expected.hResult);
    } else {
        EXPECT_FALSE(hResult.has_value());
    }
}

void expectHttpState(
    const HttpRequestException& exception,
    HResult expectedHResult,
    const std::string& expectedMessage,
    HttpRequestError expectedError,
    std::optional<HttpStatusCode> expectedStatus,
    const InnerCase& expectedInner)
{
    EXPECT_EQ(exception.getHResultProperty(), expectedHResult);
    EXPECT_EQ(exception.getMessageProperty(), expectedMessage);
    EXPECT_EQ(exception.getHttpRequestErrorProperty(), expectedError);
    EXPECT_EQ(exception.getStatusCodeProperty(), expectedStatus);
    expectInner(exception.getInnerExceptionProperty(), expectedInner);
}

void expectWebState(
    const WebException& exception,
    HResult expectedHResult,
    const std::string& expectedMessage,
    WebExceptionStatus expectedStatus,
    const InnerCase& expectedInner)
{
    EXPECT_EQ(exception.getHResultProperty(), expectedHResult);
    EXPECT_EQ(exception.getMessageProperty(), expectedMessage);
    EXPECT_EQ(exception.getStatusProperty(), expectedStatus);
    expectInner(exception.getInnerExceptionProperty(), expectedInner);
}

HResult expectedOuter(const InnerCase& inner, HResult baseHResult) {
    return inner.isSystemException ? inner.hResult : baseHResult;
}

class RethrowingHandler final : public HttpMessageHandler {
public:
    explicit RethrowingHandler(std::exception_ptr exception)
        : exception_(std::move(exception)) {}

    std::shared_ptr<HttpResponseMessage> Send(
        std::shared_ptr<HttpRequestMessage> /*request*/) override
    {
        std::rethrow_exception(exception_);
    }

private:
    std::exception_ptr exception_;
};

void expectCausalHttpTransport(
    const std::function<void()>& action,
    const InnerCase& inner)
{
    try {
        action();
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& exception) {
        expectHttpState(
            exception,
            kCustomHResult,
            "transport-message",
            HttpRequestError::ConnectionError,
            HttpStatusCode::ServiceUnavailable,
            inner);
    } catch (...) {
        FAIL() << "transport changed the dynamic outer exception type";
    }
}

} // namespace

TEST(NetworkExceptionHResultPropagationTests, StructuralSurfaceRemainsTheMeasuredInlineShape) {
    static_assert(std::is_polymorphic_v<HttpRequestException>);
    static_assert(std::is_polymorphic_v<WebException>);
    static_assert(std::is_constructible_v<HttpRequestException, const std::string&, std::exception_ptr>);
    static_assert(std::is_constructible_v<
                  HttpRequestException,
                  const std::string&,
                  std::exception_ptr,
                  HttpStatusCode>);
    static_assert(std::is_constructible_v<
                  HttpRequestException,
                  HttpRequestError,
                  const std::string&,
                  std::exception_ptr,
                  std::optional<HttpStatusCode>>);
    static_assert(std::is_constructible_v<WebException, const std::string&, std::exception_ptr>);
    static_assert(std::is_constructible_v<
                  WebException,
                  const std::string&,
                  std::exception_ptr,
                  WebExceptionStatus>);
    static_assert(!std::is_nothrow_constructible_v<
                  HttpRequestException,
                  const std::string&,
                  std::exception_ptr>);
    static_assert(!std::is_nothrow_constructible_v<WebException, const std::string&, std::exception_ptr>);

    EXPECT_EQ(sizeof(HttpRequestException), 176u);
    EXPECT_EQ(alignof(HttpRequestException), 8u);
    EXPECT_EQ(sizeof(WebException), 168u);
    EXPECT_EQ(alignof(WebException), 8u);
}

TEST(NetworkExceptionHResultPropagationTests, ConstructorsWithoutInnerRetainBaseValuesAndMetadata) {
    const auto nullInner = makeInnerCases().front();

    // #2323: a default-constructed HttpRequestException now carries .NET's fallback message,
    // naming ITS OWN type rather than System.Exception -- .NET's ctor is `{ }`
    // (HttpRequestException.cs:10-11), so Message falls through to Exception's formatted default.
    expectHttpState(HttpRequestException{}, kCorEException,
                    "Exception of type 'System.Net.Http.HttpRequestException' was thrown.",
                    HttpRequestError::Unknown,
                    std::nullopt, nullInner);
    expectHttpState(HttpRequestException{"http-\xCF\x80"}, kCorEException, "http-\xCF\x80",
                    HttpRequestError::Unknown, std::nullopt, nullInner);
    expectHttpState(
        HttpRequestException{
            static_cast<HttpRequestError>(999), "", nullptr,
            static_cast<HttpStatusCode>(999)},
        kCorEException, "", static_cast<HttpRequestError>(999),
        static_cast<HttpStatusCode>(999), nullInner);

    expectWebState(WebException{}, kCorEInvalidOperation,
                   "Operation is not valid due to the current state of the object.",
                   WebExceptionStatus::UnknownError, nullInner);
    expectWebState(WebException{"web-\xCF\x80"}, kCorEInvalidOperation, "web-\xCF\x80",
                   WebExceptionStatus::UnknownError, nullInner);
    expectWebState(WebException{"", static_cast<WebExceptionStatus>(999)},
                   kCorEInvalidOperation, "", static_cast<WebExceptionStatus>(999), nullInner);
}

TEST(NetworkExceptionHResultPropagationTests, HttpMessageInnerConstructorCopiesOnlySystemHResult) {
    for (const auto& inner : makeInnerCases()) {
        SCOPED_TRACE(inner.label);
        HttpRequestException exception("http-h3", inner.pointer);
        expectHttpState(exception, expectedOuter(inner, kCorEException), "http-h3",
                        HttpRequestError::Unknown, std::nullopt, inner);
    }
}

TEST(NetworkExceptionHResultPropagationTests, HttpStatusConstructorKeepsStatusOrthogonalToInnerHResult) {
    for (const auto& inner : makeInnerCases()) {
        SCOPED_TRACE(inner.label);
        HttpRequestException exception("http-h4", inner.pointer, HttpStatusCode::BadGateway);
        expectHttpState(exception, expectedOuter(inner, kCorEException), "http-h4",
                        HttpRequestError::Unknown, HttpStatusCode::BadGateway, inner);
    }
}

TEST(NetworkExceptionHResultPropagationTests, HttpErrorStatusConstructorKeepsMetadataOrthogonalToInnerHResult) {
    for (const auto& inner : makeInnerCases()) {
        SCOPED_TRACE(inner.label);
        HttpRequestException exception(
            HttpRequestError::ConnectionError,
            "http-h5",
            inner.pointer,
            HttpStatusCode::ServiceUnavailable);
        expectHttpState(exception, expectedOuter(inner, kCorEException), "http-h5",
                        HttpRequestError::ConnectionError,
                        HttpStatusCode::ServiceUnavailable, inner);
    }
}

TEST(NetworkExceptionHResultPropagationTests, WebMessageInnerConstructorCopiesOnlySystemHResult) {
    for (const auto& inner : makeInnerCases()) {
        SCOPED_TRACE(inner.label);
        WebException exception("web-w3", inner.pointer);
        expectWebState(exception, expectedOuter(inner, kCorEInvalidOperation), "web-w3",
                       WebExceptionStatus::UnknownError, inner);
    }
}

TEST(NetworkExceptionHResultPropagationTests, WebStatusConstructorKeepsStatusOrthogonalToInnerHResult) {
    for (const auto& inner : makeInnerCases()) {
        SCOPED_TRACE(inner.label);
        WebException exception("web-w5", inner.pointer, WebExceptionStatus::Timeout);
        expectWebState(exception, expectedOuter(inner, kCorEInvalidOperation), "web-w5",
                       WebExceptionStatus::Timeout, inner);
    }
}

TEST(NetworkExceptionHResultPropagationTests, HttpCopyMoveAndAssignmentsPreserveCalculatedState) {
    static_assert(std::is_copy_constructible_v<HttpRequestException>);
    static_assert(std::is_move_constructible_v<HttpRequestException>);
    static_assert(std::is_copy_assignable_v<HttpRequestException>);
    static_assert(std::is_move_assignable_v<HttpRequestException>);

    const auto inner = makeInnerCases()[3];
    const auto makeSource = [&]() {
        return HttpRequestException(
            HttpRequestError::InvalidResponse,
            "http-copy-move",
            inner.pointer,
            HttpStatusCode::BadRequest);
    };

    const HttpRequestException source = makeSource();
    const HttpRequestException copied(source);
    expectHttpState(copied, kCustomHResult, "http-copy-move",
                    HttpRequestError::InvalidResponse, HttpStatusCode::BadRequest, inner);

    auto moveConstructSource = makeSource();
    HttpRequestException moved(std::move(moveConstructSource));
    expectHttpState(moved, kCustomHResult, "http-copy-move",
                    HttpRequestError::InvalidResponse, HttpStatusCode::BadRequest, inner);

    HttpRequestException copyAssigned;
    copyAssigned = source;
    expectHttpState(copyAssigned, kCustomHResult, "http-copy-move",
                    HttpRequestError::InvalidResponse, HttpStatusCode::BadRequest, inner);

    HttpRequestException moveAssigned;
    auto moveSource = makeSource();
    moveAssigned = std::move(moveSource);
    expectHttpState(moveAssigned, kCustomHResult, "http-copy-move",
                    HttpRequestError::InvalidResponse, HttpStatusCode::BadRequest, inner);
}

TEST(NetworkExceptionHResultPropagationTests, WebCopyMoveAndAssignmentsPreserveCalculatedState) {
    static_assert(std::is_copy_constructible_v<WebException>);
    static_assert(std::is_move_constructible_v<WebException>);
    static_assert(std::is_copy_assignable_v<WebException>);
    static_assert(std::is_move_assignable_v<WebException>);

    const auto inner = makeInnerCases()[3];
    const auto makeSource = [&]() {
        return WebException("web-copy-move", inner.pointer, WebExceptionStatus::ReceiveFailure);
    };

    const WebException source = makeSource();
    const WebException copied(source);
    expectWebState(copied, kCustomHResult, "web-copy-move",
                   WebExceptionStatus::ReceiveFailure, inner);

    auto moveConstructSource = makeSource();
    WebException moved(std::move(moveConstructSource));
    expectWebState(moved, kCustomHResult, "web-copy-move",
                   WebExceptionStatus::ReceiveFailure, inner);

    WebException copyAssigned;
    copyAssigned = source;
    expectWebState(copyAssigned, kCustomHResult, "web-copy-move",
                   WebExceptionStatus::ReceiveFailure, inner);

    WebException moveAssigned;
    auto moveSource = makeSource();
    moveAssigned = std::move(moveSource);
    expectWebState(moveAssigned, kCustomHResult, "web-copy-move",
                   WebExceptionStatus::ReceiveFailure, inner);
}

TEST(NetworkExceptionHResultPropagationTests, OuterExceptionPointersPreserveDynamicTypeAndStateAcrossRepeatedRethrow) {
    const auto inner = makeInnerCases()[3];
    const HttpRequestException httpSource(
        HttpRequestError::ConnectionError,
        "http-pointer",
        inner.pointer,
        HttpStatusCode::ServiceUnavailable);
    const WebException webSource("web-pointer", inner.pointer, WebExceptionStatus::Timeout);

    const auto httpPointer = std::make_exception_ptr(httpSource);
    const auto webPointer = std::make_exception_ptr(webSource);
    for (int pass = 0; pass < 2; ++pass) {
        SCOPED_TRACE(pass);
        try {
            std::rethrow_exception(httpPointer);
        } catch (const HttpRequestException& exception) {
            expectHttpState(exception, kCustomHResult, "http-pointer",
                            HttpRequestError::ConnectionError,
                            HttpStatusCode::ServiceUnavailable, inner);
        } catch (...) {
            FAIL() << "HttpRequestException dynamic type was not preserved";
        }

        try {
            std::rethrow_exception(webPointer);
        } catch (const WebException& exception) {
            expectWebState(exception, kCustomHResult, "web-pointer",
                           WebExceptionStatus::Timeout, inner);
        } catch (...) {
            FAIL() << "WebException dynamic type was not preserved";
        }
    }
}

TEST(NetworkExceptionHResultPropagationTests, HttpClientSyncAndAsyncForwardCausalExceptionUnchanged) {
    const auto inner = makeInnerCases()[3];
    const HttpRequestException causal(
        HttpRequestError::ConnectionError,
        "transport-message",
        inner.pointer,
        HttpStatusCode::ServiceUnavailable);
    auto handler = std::make_shared<RethrowingHandler>(std::make_exception_ptr(causal));
    HttpClient client(handler);

    expectCausalHttpTransport([&] {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
        (void)client.Send(request);
    }, inner);

    expectCausalHttpTransport([&] {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
        (void)client.SendAsync(request).Wait();
    }, inner);
}

TEST(NetworkExceptionHResultPropagationTests, HttpMessageInvokerSyncAndAsyncForwardCausalExceptionUnchanged) {
    const auto inner = makeInnerCases()[3];
    const HttpRequestException causal(
        HttpRequestError::ConnectionError,
        "transport-message",
        inner.pointer,
        HttpStatusCode::ServiceUnavailable);
    auto handler = std::make_shared<RethrowingHandler>(std::make_exception_ptr(causal));
    HttpMessageInvoker invoker(handler, false);

    expectCausalHttpTransport([&] {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
        (void)invoker.Send(request);
    }, inner);

    expectCausalHttpTransport([&] {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
        (void)invoker.SendAsync(request).Wait();
    }, inner);
}

TEST(NetworkExceptionHResultPropagationTests, BuiltInNoInnerProducerControlsRetainBaseHResult) {
    const auto nullInner = makeInnerCases().front();

    try {
        (void)HttpClient::parseStatusLine("garbage");
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& exception) {
        expectHttpState(
            exception,
            kCorEException,
            "HttpClient: malformed status line: 'garbage'",
            HttpRequestError::Unknown,
            std::nullopt,
            nullInner);
    }

    HttpResponseMessage response(HttpStatusCode::NotFound);
    try {
        (void)response.EnsureSuccessStatusCode();
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& exception) {
        expectHttpState(
            exception,
            kCorEException,
            "Response status code does not indicate success: 404.",
            HttpRequestError::Unknown,
            HttpStatusCode::NotFound,
            nullInner);
    }
}
