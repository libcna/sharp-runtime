// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include <optional>
#include <functional>
#include <any>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/StringContent.hpp"
#include "System/Net/Http/ByteArrayContent.hpp"
#include "System/Net/Http/HttpRequestMessage.hpp"
#include "System/Net/Http/HttpResponseMessage.hpp"
#include "System/Net/Http/HttpCompletionOption.hpp"
#include "System/Net/Http/HttpVersionPolicy.hpp"
#include "System/Net/Http/HttpRequestError.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/HttpIOException.hpp"
#include "System/Net/Http/HttpProtocolException.hpp"
#include "System/Net/Http/ReadOnlyMemoryContent.hpp"
#include "System/Net/Http/StreamContent.hpp"
#include "System/Net/Http/MultipartContent.hpp"
#include "System/Net/Http/MultipartFormDataContent.hpp"
#include "System/Net/Http/HttpMessageHandler.hpp"
#include "System/Net/Http/HttpMessageInvoker.hpp"
#include "System/Net/Http/DelegatingHandler.hpp"
#include "System/Net/Http/HttpClientHandler.hpp"
#include "System/Net/Http/HttpRequestOptions.hpp"
#include "System/Net/Http/HttpRequestOptionsKey.hpp"
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieContainer.hpp"
#include "System/Net/CookieException.hpp"
#include "System/Uri.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/NotSupportedException.hpp"
#include "System/UriFormatException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationTokenSource.hpp"

using namespace System::Net::Http;
using System::Net::HttpStatusCode;

// ---------------------------------------------------------------------------
// HttpMethod
// ---------------------------------------------------------------------------

TEST(HttpMethodTests, GetMethod) {
    HttpMethod m = HttpMethod::Get();
    EXPECT_EQ(m.getMethodProperty(), "GET");
    EXPECT_EQ(m.ToString(), "GET");
}

TEST(HttpMethodTests, PostMethod) {
    EXPECT_EQ(HttpMethod::Post().getMethodProperty(), "POST");
}

TEST(HttpMethodTests, CustomMethod) {
    HttpMethod m("TRACE");
    EXPECT_EQ(m.getMethodProperty(), "TRACE");
}

TEST(HttpMethodTests, Equality) {
    EXPECT_TRUE(HttpMethod::Get() == HttpMethod::Get());
    EXPECT_FALSE(HttpMethod::Get() == HttpMethod::Post());
    EXPECT_TRUE(HttpMethod::Get() != HttpMethod::Post());
}

TEST(HttpMethodTests, Equality_CaseInsensitive) {
    HttpMethod a("get");
    EXPECT_TRUE(a == HttpMethod::Get());
}

TEST(HttpMethodTests, GetHashCode_CaseInsensitive_Equal) {
    HttpMethod a("get");
    HttpMethod b("GET");
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(HttpMethodTests, Constructor_Empty_Throws) {
    EXPECT_THROW(HttpMethod(""), System::ArgumentException);
}

TEST(HttpMethodTests, Constructor_Whitespace_Throws) {
    EXPECT_THROW(HttpMethod("   "), System::ArgumentException);
}

TEST(HttpMethodTests, Constructor_InvalidChars_Throws) {
    EXPECT_THROW(HttpMethod("GET /foo"), System::FormatException);
    EXPECT_THROW(HttpMethod("GET,POST"), System::FormatException);
}

TEST(HttpMethodTests, Constructor_EmbeddedNul_Throws) {
    EXPECT_THROW(HttpMethod(std::string("GE\0T", 4)), System::FormatException);
}

TEST(HttpMethodTests, Parse_KnownMethod_CaseInsensitive_ReturnsSingleton) {
    EXPECT_EQ(HttpMethod::Parse("get").getMethodProperty(), "GET");
    EXPECT_EQ(HttpMethod::Parse("POST").getMethodProperty(), "POST");
    EXPECT_EQ(HttpMethod::Parse("Delete").getMethodProperty(), "DELETE");
}

TEST(HttpMethodTests, Parse_UnknownMethod_ConstructsNew) {
    HttpMethod m = HttpMethod::Parse("PROPFIND");
    EXPECT_EQ(m.getMethodProperty(), "PROPFIND");
}

TEST(HttpMethodTests, Parse_Invalid_Throws) {
    EXPECT_THROW(HttpMethod::Parse(""), System::ArgumentException);
    EXPECT_THROW(HttpMethod::Parse("GET /foo"), System::FormatException);
}

TEST(HttpMethodTests, TraceConnectQuery_Methods) {
    EXPECT_EQ(HttpMethod::Trace().getMethodProperty(), "TRACE");
    EXPECT_EQ(HttpMethod::Connect().getMethodProperty(), "CONNECT");
    EXPECT_EQ(HttpMethod::Query().getMethodProperty(), "QUERY");
}

// ---------------------------------------------------------------------------
// StringContent
// ---------------------------------------------------------------------------

TEST(StringContentTests, ReadAsString) {
    StringContent c("hello world");
    EXPECT_EQ(c.ReadAsString(), "hello world");
}

TEST(StringContentTests, DefaultContentType) {
    StringContent c("body");
    EXPECT_EQ(c.getContentTypeProperty(), "text/plain");
}

TEST(StringContentTests, CustomMediaType) {
    StringContent c("{}", "utf-8", "application/json");
    EXPECT_EQ(c.getContentTypeProperty(), "application/json");
    EXPECT_EQ(c.getCharSetProperty(), "utf-8");
}

TEST(StringContentTests, ReadAsByteArray) {
    StringContent c("abc");
    auto bytes = c.ReadAsByteArray();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 'a');
    EXPECT_EQ(bytes[1], 'b');
    EXPECT_EQ(bytes[2], 'c');
}

// ---------------------------------------------------------------------------
// ByteArrayContent
// ---------------------------------------------------------------------------

TEST(ByteArrayContentTests, RoundTrip) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0xFF};
    ByteArrayContent c(data);
    EXPECT_EQ(c.ReadAsByteArray(), data);
}

TEST(ByteArrayContentTests, DefaultContentType) {
    ByteArrayContent c({});
    EXPECT_EQ(c.getContentTypeProperty(), "application/octet-stream");
}

TEST(ByteArrayContentTests, ReadAsString) {
    std::vector<uint8_t> data = {'h', 'i'};
    ByteArrayContent c(data);
    EXPECT_EQ(c.ReadAsString(), "hi");
}

// ---------------------------------------------------------------------------
// HttpResponseMessage
// ---------------------------------------------------------------------------

TEST(HttpResponseMessageTests, DefaultStatus200) {
    HttpResponseMessage r;
    EXPECT_EQ(r.getStatusCodeProperty(), HttpStatusCode::OK);
}

TEST(HttpResponseMessageTests, IsSuccessTrue) {
    HttpResponseMessage r(HttpStatusCode::OK);
    EXPECT_TRUE(r.getIsSuccessStatusCodeProperty());
}

TEST(HttpResponseMessageTests, IsSuccessFalse_404) {
    HttpResponseMessage r(HttpStatusCode::NotFound);
    EXPECT_FALSE(r.getIsSuccessStatusCodeProperty());
}

TEST(HttpResponseMessageTests, IsSuccessFalse_500) {
    HttpResponseMessage r(HttpStatusCode::InternalServerError);
    EXPECT_FALSE(r.getIsSuccessStatusCodeProperty());
}

TEST(HttpResponseMessageTests, EnsureSuccessThrows) {
    HttpResponseMessage r(HttpStatusCode::NotFound);
    r.setReasonPhraseProperty("Not Found");
    EXPECT_THROW(r.EnsureSuccessStatusCode(), HttpRequestException);
}

TEST(HttpResponseMessageTests, EnsureSuccessThrows_MessageIncludesReasonPhrase) {
    HttpResponseMessage r(HttpStatusCode::NotFound);
    r.setReasonPhraseProperty("Not Found");
    try {
        r.EnsureSuccessStatusCode();
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& ex) {
        EXPECT_EQ(ex.getMessageProperty(), "Response status code does not indicate success: 404 (Not Found).");
    }
}

TEST(HttpResponseMessageTests, EnsureSuccessThrows_EmptyReasonPhrase_NoParenthetical) {
    HttpResponseMessage r(HttpStatusCode::NotFound);
    try {
        r.EnsureSuccessStatusCode();
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& ex) {
        EXPECT_EQ(ex.getMessageProperty(), "Response status code does not indicate success: 404.");
    }
}

TEST(HttpResponseMessageTests, EnsureSuccessThrows_CarriesStatusCode) {
    HttpResponseMessage r(HttpStatusCode::NotFound);
    try {
        r.EnsureSuccessStatusCode();
        FAIL() << "expected HttpRequestException";
    } catch (const HttpRequestException& ex) {
        ASSERT_TRUE(ex.getStatusCodeProperty().has_value());
        EXPECT_EQ(*ex.getStatusCodeProperty(), HttpStatusCode::NotFound);
    }
}

TEST(HttpResponseMessageTests, EnsureSuccessNoThrow) {
    HttpResponseMessage r(HttpStatusCode::OK);
    EXPECT_NO_THROW(r.EnsureSuccessStatusCode());
}

TEST(HttpResponseMessageTests, HeaderRoundTrip) {
    HttpResponseMessage r;
    r.setHeader("Content-Type", "application/json");
    EXPECT_EQ(r.getHeader("Content-Type"), "application/json");
}

TEST(HttpResponseMessageTests, MissingHeaderReturnsEmpty) {
    HttpResponseMessage r;
    EXPECT_EQ(r.getHeader("X-Missing"), "");
}

// ---------------------------------------------------------------------------
// HttpRequestMessage
// ---------------------------------------------------------------------------

TEST(HttpRequestMessageTests, DefaultIsGet) {
    HttpRequestMessage req;
    EXPECT_EQ(req.getMethodProperty().getMethodProperty(), "GET");
}

TEST(HttpRequestMessageTests, ConstructorSetsMethodAndUri) {
    HttpRequestMessage req(HttpMethod::Post(), "http://example.com/api");
    EXPECT_EQ(req.getMethodProperty().getMethodProperty(), "POST");
    EXPECT_EQ(req.getRequestUriProperty(), "http://example.com/api");
}

TEST(HttpRequestMessageTests, ContentRoundTrip) {
    HttpRequestMessage req;
    req.setContentProperty(std::make_shared<StringContent>("payload"));
    ASSERT_NE(req.getContentProperty(), nullptr);
    EXPECT_EQ(req.getContentProperty()->ReadAsString(), "payload");
}

// ---------------------------------------------------------------------------
// HttpClient — URL parsing
// ---------------------------------------------------------------------------

TEST(HttpClientUrlParseTests, SimpleUrl) {
    auto p = HttpClient::parseUrl("http://example.com/path");
    EXPECT_EQ(p.scheme, "http");
    EXPECT_EQ(p.host,   "example.com");
    EXPECT_EQ(p.port,   80);
    EXPECT_EQ(p.path,   "/path");
}

TEST(HttpClientUrlParseTests, UrlWithPort) {
    auto p = HttpClient::parseUrl("http://example.com:8080/api/v1");
    EXPECT_EQ(p.host, "example.com");
    EXPECT_EQ(p.port, 8080);
    EXPECT_EQ(p.path, "/api/v1");
}

TEST(HttpClientUrlParseTests, UrlNoPath) {
    auto p = HttpClient::parseUrl("http://example.com");
    EXPECT_EQ(p.path, "/");
}

TEST(HttpClientUrlParseTests, QueryStringPreserved) {
    auto p = HttpClient::parseUrl("http://example.com/search?q=hello");
    EXPECT_EQ(p.path, "/search?q=hello");
}

// Regression tests for a wave-3 audit finding: parseUrl threw std::invalid_argument (an
// unrelated std:: exception type) for every failure path instead of a System:: exception,
// escaping uncaught by code catching System::Exception&. A malformed URI now throws
// System::UriFormatException (matching real .NET's Uri construction failure); an
// unsupported-but-well-formed scheme now throws System::NotSupportedException (HTTPS/TLS is
// out of scope for this runtime, not a format error).
TEST(HttpClientUrlParseTests, UnsupportedSchemeThrowsNotSupportedException) {
    EXPECT_THROW(HttpClient::parseUrl("https://example.com"), System::NotSupportedException);
}

TEST(HttpClientUrlParseTests, MissingSchemeThrowsUriFormatException) {
    EXPECT_THROW(HttpClient::parseUrl("example.com/path"), System::UriFormatException);
}

TEST(HttpClientUrlParseTests, InvalidPortThrowsUriFormatException) {
    EXPECT_THROW(HttpClient::parseUrl("http://example.com:notaport/path"), System::UriFormatException);
}

TEST(HttpClientUrlParseTests, EmptyHostThrowsUriFormatException) {
    EXPECT_THROW(HttpClient::parseUrl("http://:8080/path"), System::UriFormatException);
}

// Regression tests for a wave-3 audit finding: parseUrl split host:port on the last ':' in
// the string, which is wrong for an IPv6 literal in bracket notation -- the address itself
// contains colons, so "[::1]" (no port) had its host/port split in the middle of the address
// instead of at the bracket, and the brackets themselves were never stripped even when a
// port was present.
TEST(HttpClientUrlParseTests, IPv6Literal_WithPort_StripsBracketsAndParsesPort) {
    auto p = HttpClient::parseUrl("http://[::1]:8080/path");
    EXPECT_EQ(p.host, "::1");
    EXPECT_EQ(p.port, 8080);
    EXPECT_EQ(p.path, "/path");
}

TEST(HttpClientUrlParseTests, IPv6Literal_NoPort_StripsBracketsAndDefaultsPort80) {
    auto p = HttpClient::parseUrl("http://[::1]/path");
    EXPECT_EQ(p.host, "::1");
    EXPECT_EQ(p.port, 80);
    EXPECT_EQ(p.path, "/path");
}

TEST(HttpClientUrlParseTests, IPv6Literal_FullAddress_WithPort) {
    auto p = HttpClient::parseUrl("http://[2001:db8::1]:9090/api");
    EXPECT_EQ(p.host, "2001:db8::1");
    EXPECT_EQ(p.port, 9090);
}

TEST(HttpClientUrlParseTests, IPv6Literal_NoPath_DefaultsToRoot) {
    auto p = HttpClient::parseUrl("http://[::1]");
    EXPECT_EQ(p.host, "::1");
    EXPECT_EQ(p.path, "/");
}

TEST(HttpClientUrlParseTests, IPv6Literal_Unterminated_ThrowsUriFormatException) {
    EXPECT_THROW(HttpClient::parseUrl("http://[::1/path"), System::UriFormatException);
}

// ---------------------------------------------------------------------------
// HttpClient — status line parser
// ---------------------------------------------------------------------------

TEST(HttpClientStatusLineParseTests, WellFormed_ParsesCodeAndReason) {
    auto p = HttpClient::parseStatusLine("HTTP/1.1 200 OK");
    EXPECT_EQ(p.statusCode, 200);
    EXPECT_EQ(p.reason, "OK");
}

TEST(HttpClientStatusLineParseTests, MultiWordReason_KeptIntact) {
    auto p = HttpClient::parseStatusLine("HTTP/1.1 404 Not Found");
    EXPECT_EQ(p.statusCode, 404);
    EXPECT_EQ(p.reason, "Not Found");
}

TEST(HttpClientStatusLineParseTests, NoReasonPhrase_EmptyReason) {
    auto p = HttpClient::parseStatusLine("HTTP/1.1 200");
    EXPECT_EQ(p.statusCode, 200);
    EXPECT_EQ(p.reason, "");
}

// Regression tests for a wave-3 audit finding: a malformed status line silently defaulted to
// statusCode=200 (OK) whenever the line had no space at all, making a garbled/empty response
// from the server indistinguishable from success. It now throws HttpRequestException.
TEST(HttpClientStatusLineParseTests, NoSpaces_ThrowsHttpRequestException) {
    EXPECT_THROW(HttpClient::parseStatusLine("garbage"), HttpRequestException);
}

TEST(HttpClientStatusLineParseTests, EmptyLine_ThrowsHttpRequestException) {
    EXPECT_THROW(HttpClient::parseStatusLine(""), HttpRequestException);
}

TEST(HttpClientStatusLineParseTests, NonNumericStatusCode_ThrowsHttpRequestException) {
    EXPECT_THROW(HttpClient::parseStatusLine("HTTP/1.1 XYZ OK"), HttpRequestException);
}

// ---------------------------------------------------------------------------
// HttpClient — construction and default headers
// ---------------------------------------------------------------------------

TEST(HttpClientTests, Construction) {
    EXPECT_NO_THROW({ HttpClient client; });
}

TEST(HttpClientTests, DefaultHeaders) {
    HttpClient client;
    client.setDefaultHeader("Accept", "application/json");
    EXPECT_EQ(client.getDefaultHeader("Accept"), "application/json");
}

TEST(HttpClientTests, MissingDefaultHeaderReturnsEmpty) {
    HttpClient client;
    EXPECT_EQ(client.getDefaultHeader("X-Missing"), "");
}

TEST(HttpClientTests, BaseAddress) {
    HttpClient client;
    client.setBaseAddressProperty("http://api.example.com");
    EXPECT_EQ(client.getBaseAddressProperty(), "http://api.example.com");
}

// Verified against HttpClient.cs's CheckRequestBeforeSend: every Send overload validates
// the request is non-null. Previously this dereferenced a null request immediately (UB/
// crash) instead of throwing a catchable exception.
TEST(HttpClientTests, Send_NullRequest_ThrowsArgumentNullException) {
    HttpClient client;
    std::shared_ptr<HttpRequestMessage> nullRequest;
    EXPECT_THROW(client.Send(nullRequest), System::ArgumentNullException);
}

// ---------------------------------------------------------------------------
// FormUrlEncodedContent
// ---------------------------------------------------------------------------

#include "System/Net/Http/FormUrlEncodedContent.hpp"

TEST(FormUrlEncodedContentTests, ContentType) {
    FormUrlEncodedContent c({});
    EXPECT_EQ(c.getContentTypeProperty(), "application/x-www-form-urlencoded");
}

TEST(FormUrlEncodedContentTests, EmptyPairs) {
    FormUrlEncodedContent c({});
    EXPECT_EQ(c.ReadAsString(), "");
}

TEST(FormUrlEncodedContentTests, SinglePair) {
    FormUrlEncodedContent c({{"name", "Alice"}});
    EXPECT_EQ(c.ReadAsString(), "name=Alice");
}

TEST(FormUrlEncodedContentTests, MultiplePairs) {
    FormUrlEncodedContent c({{"a", "1"}, {"b", "2"}});
    EXPECT_EQ(c.ReadAsString(), "a=1&b=2");
}

TEST(FormUrlEncodedContentTests, SpaceEncodedAsPlus) {
    FormUrlEncodedContent c({{"q", "hello world"}});
    EXPECT_EQ(c.ReadAsString(), "q=hello+world");
}

TEST(FormUrlEncodedContentTests, SpecialCharsPercentEncoded) {
    FormUrlEncodedContent c({{"url", "a&b=c"}});
    std::string result = c.ReadAsString();
    // '&' and '=' must be percent-encoded in values
    EXPECT_EQ(result, "url=a%26b%3Dc");
}

TEST(FormUrlEncodedContentTests, Rfc3986UnreservedCharacters_ArePreservedAndAsteriskIsEscaped) {
    FormUrlEncodedContent c({{"key", "-._~*"}});
    EXPECT_EQ(c.ReadAsString(), "key=-._~%2A");
}

TEST(FormUrlEncodedContentTests, Utf8Characters_AreEncodedByteByByte) {
    FormUrlEncodedContent c({{"key", std::string("value\xE3\x82\xAF")}});
    EXPECT_EQ(c.ReadAsString(), "key=value%E3%82%AF");
}

TEST(FormUrlEncodedContentTests, ReadAsByteArrayMatchesString) {
    FormUrlEncodedContent c({{"k", "v"}});
    std::string s = c.ReadAsString();
    auto bytes = c.ReadAsByteArray();
    std::string fromBytes(bytes.begin(), bytes.end());
    EXPECT_EQ(s, fromBytes);
}

// ---------------------------------------------------------------------------
// HttpRequestOptions / HttpRequestOptionsKey
// ---------------------------------------------------------------------------

TEST(HttpRequestOptionsKeyTests, Constructor_StoresKeyName) {
    HttpRequestOptionsKey<bool> key("WebAssemblyEnableStreamingResponse");
    EXPECT_EQ(key.getKeyProperty(), "WebAssemblyEnableStreamingResponse");
}

TEST(HttpRequestOptionsTests, TypedSetAndGet_RoundTripThroughRequestOptions) {
    HttpRequestOptionsKey<bool> key("Streaming");
    HttpRequestMessage request;
    request.getOptionsProperty().Set(key, true);

    bool value = false;
    EXPECT_TRUE(request.getOptionsProperty().TryGetValue(key, value));
    EXPECT_TRUE(value);
    EXPECT_EQ(request.getOptionsProperty().getCountProperty(), 1);
}

TEST(HttpRequestOptionsTests, WrongTypedKey_ReturnsFalseAndDefaultValue) {
    HttpRequestOptions options;
    options.Set(HttpRequestOptionsKey<std::string>("Option"), std::string("value"));

    SharpRuntime::intcs value = 42;
    EXPECT_FALSE(options.TryGetValue(HttpRequestOptionsKey<SharpRuntime::intcs>("Option"), value));
    EXPECT_EQ(value, 0);
}

TEST(HttpRequestOptionsTests, UntypedDictionaryOperations_RespectAddRemoveAndLookupSemantics) {
    HttpRequestOptions options;
    options.Add("RetryCount", SharpRuntime::intcs{3});
    EXPECT_TRUE(options.ContainsKey("RetryCount"));

    std::any value;
    EXPECT_TRUE(options.TryGetValue("RetryCount", value));
    EXPECT_EQ(std::any_cast<SharpRuntime::intcs>(value), 3);
    EXPECT_THROW(options.Add("RetryCount", SharpRuntime::intcs{4}), System::ArgumentException);
    EXPECT_TRUE(options.Remove("RetryCount"));
    EXPECT_FALSE(options.Remove("RetryCount"));
    EXPECT_FALSE(options.TryGetValue("RetryCount", value));
    EXPECT_FALSE(value.has_value());
}

// ---------------------------------------------------------------------------
// HttpCompletionOption / HttpVersionPolicy / HttpRequestError
// ---------------------------------------------------------------------------

TEST(HttpCompletionOptionTests, ResponseContentRead_IsZero) {
    EXPECT_EQ(static_cast<int>(HttpCompletionOption::ResponseContentRead), 0);
}

TEST(HttpVersionPolicyTests, HasThreeDistinctValues) {
    EXPECT_NE(HttpVersionPolicy::RequestVersionOrLower, HttpVersionPolicy::RequestVersionOrHigher);
    EXPECT_NE(HttpVersionPolicy::RequestVersionOrHigher, HttpVersionPolicy::RequestVersionExact);
}

TEST(HttpRequestErrorTests, Unknown_IsZero) {
    EXPECT_EQ(static_cast<int>(HttpRequestError::Unknown), 0);
}

// ---------------------------------------------------------------------------
// HttpIOException / HttpProtocolException
// ---------------------------------------------------------------------------

TEST(HttpIOExceptionTests, MessageIncludesErrorCategory) {
    HttpIOException ex(HttpRequestError::NameResolutionError, "DNS failed");
    std::string msg = ex.getMessageProperty();
    EXPECT_NE(msg.find("DNS failed"), std::string::npos);
    EXPECT_NE(msg.find("NameResolutionError"), std::string::npos);
}

TEST(HttpIOExceptionTests, GetHttpRequestErrorProperty) {
    HttpIOException ex(HttpRequestError::ConnectionError, "boom");
    EXPECT_EQ(ex.getHttpRequestErrorProperty(), HttpRequestError::ConnectionError);
}

TEST(HttpIOExceptionTests, IsA_IOException) {
    HttpIOException ex(HttpRequestError::Unknown, "x");
    System::IO::IOException* base = &ex;
    EXPECT_NE(std::string(base->what()).find("x"), std::string::npos);
}

TEST(HttpIOExceptionTests, DefaultMessage_FallsBackToIOExceptionDefault) {
    HttpIOException ex(HttpRequestError::ConnectionError);
    std::string msg = ex.getMessageProperty();
    EXPECT_NE(msg.find("I/O error occurred."), std::string::npos);
    EXPECT_NE(msg.find("ConnectionError"), std::string::npos);
    EXPECT_NE(msg.front(), ' ');
}

TEST(HttpProtocolExceptionTests, StoresErrorCodeAndHttpProtocolErrorCategory) {
    HttpProtocolException ex(0x1, "stream error", nullptr);
    EXPECT_EQ(ex.getErrorCodeProperty(), 0x1);
    EXPECT_EQ(ex.getHttpRequestErrorProperty(), HttpRequestError::HttpProtocolError);
}

TEST(HttpProtocolExceptionTests, IsA_HttpIOException) {
    HttpProtocolException ex(2, "y", nullptr);
    HttpIOException* base = &ex;
    EXPECT_EQ(base->getHttpRequestErrorProperty(), HttpRequestError::HttpProtocolError);
}

// ---------------------------------------------------------------------------
// ReadOnlyMemoryContent
// ---------------------------------------------------------------------------

TEST(ReadOnlyMemoryContentTests, ReadAsString) {
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    System::ReadOnlyMemory<uint8_t> mem(data);
    ReadOnlyMemoryContent c(mem);
    EXPECT_EQ(c.ReadAsString(), "hello");
}

TEST(ReadOnlyMemoryContentTests, ReadAsByteArray) {
    std::vector<uint8_t> data = {1, 2, 3};
    System::ReadOnlyMemory<uint8_t> mem(data);
    ReadOnlyMemoryContent c(mem);
    EXPECT_EQ(c.ReadAsByteArray(), data);
}

TEST(ReadOnlyMemoryContentTests, DefaultContentType) {
    std::vector<uint8_t> data = {1};
    System::ReadOnlyMemory<uint8_t> mem(data);
    ReadOnlyMemoryContent c(mem);
    EXPECT_EQ(c.getContentTypeProperty(), "application/octet-stream");
}

// ---------------------------------------------------------------------------
// StreamContent
// ---------------------------------------------------------------------------

TEST(StreamContentTests, ReadsEntireStream) {
    std::string src = "stream body";
    auto stream = std::make_shared<System::IO::MemoryStream>(
        reinterpret_cast<const uint8_t*>(src.data()), static_cast<SharpRuntime::intcs>(src.size()));
    StreamContent c(stream);
    EXPECT_EQ(c.ReadAsString(), src);
}

TEST(StreamContentTests, ReadAsByteArrayMatchesSource) {
    std::vector<uint8_t> src = {10, 20, 30};
    auto stream = std::make_shared<System::IO::MemoryStream>(src.data(), static_cast<SharpRuntime::intcs>(src.size()));
    StreamContent c(stream);
    EXPECT_EQ(c.ReadAsByteArray(), src);
}

TEST(StreamContentTests, DefaultContentType) {
    auto stream = std::make_shared<System::IO::MemoryStream>();
    StreamContent c(stream);
    EXPECT_EQ(c.getContentTypeProperty(), "application/octet-stream");
}

TEST(StreamContentTests, NullStream_Throws) {
    EXPECT_THROW(StreamContent(nullptr), System::ArgumentNullException);
}

// ---------------------------------------------------------------------------
// MultipartContent / MultipartFormDataContent
// ---------------------------------------------------------------------------

TEST(MultipartContentTests, DefaultContentType_IsMixed) {
    MultipartContent c;
    EXPECT_NE(c.getContentTypeProperty().find("multipart/mixed"), std::string::npos);
    EXPECT_NE(c.getContentTypeProperty().find("boundary="), std::string::npos);
}

TEST(MultipartContentTests, CustomSubtypeAndBoundary) {
    MultipartContent c("alternative", "myBoundary123");
    EXPECT_EQ(c.getContentTypeProperty(), "multipart/alternative; boundary=\"myBoundary123\"");
}

TEST(MultipartContentTests, InvalidBoundary_Throws) {
    EXPECT_THROW(MultipartContent("mixed", "bad boundary "), System::ArgumentException);
    EXPECT_THROW(MultipartContent("mixed", "bad;boundary"), System::ArgumentException);
}

TEST(MultipartContentTests, InvalidBoundary_EmbeddedNul_Throws) {
    EXPECT_THROW(MultipartContent("mixed", std::string("AB\0CD", 5)), System::ArgumentException);
}

TEST(MultipartContentTests, InvalidBoundary_TooLong_Throws) {
    EXPECT_THROW(MultipartContent("mixed", std::string(71, 'a')), System::ArgumentException);
}

TEST(MultipartContentTests, Add_ThenGetContents) {
    MultipartContent c("mixed", "B");
    c.Add(std::make_shared<StringContent>("part1"));
    c.Add(std::make_shared<StringContent>("part2"));
    EXPECT_EQ(c.getContentsProperty().size(), 2u);
}

TEST(MultipartContentTests, ReadAsString_ContainsBoundariesAndContent) {
    MultipartContent c("mixed", "B");
    c.Add(std::make_shared<StringContent>("first"));
    c.Add(std::make_shared<StringContent>("second"));
    std::string body = c.ReadAsString();

    EXPECT_EQ(body.substr(0, 4), "--B\r");
    EXPECT_NE(body.find("first"), std::string::npos);
    EXPECT_NE(body.find("second"), std::string::npos);
    EXPECT_NE(body.find("--B--\r\n"), std::string::npos);
    EXPECT_NE(body.find("Content-Type: text/plain"), std::string::npos);
}

TEST(MultipartContentTests, ReadAsString_EmptyContent_JustBoundaries) {
    MultipartContent c("mixed", "B");
    EXPECT_EQ(c.ReadAsString(), "--B\r\n\r\n--B--\r\n");
}

TEST(MultipartContentTests, ReadAsString_ThreeParts_ExactByteLayout) {
    MultipartContent c("mixed", "B");
    c.Add(std::make_shared<StringContent>("one", "", ""));
    c.Add(std::make_shared<StringContent>("two", "", ""));
    c.Add(std::make_shared<StringContent>("three", "", ""));
    std::string body = c.ReadAsString();

    EXPECT_EQ(body,
        "--B\r\n"
        "\r\n"
        "one"
        "\r\n--B\r\n"
        "\r\n"
        "two"
        "\r\n--B\r\n"
        "\r\n"
        "three"
        "\r\n--B--\r\n");
}

TEST(MultipartContentTests, ReadAsByteArrayMatchesString) {
    MultipartContent c("mixed", "B");
    c.Add(std::make_shared<StringContent>("x"));
    auto bytes = c.ReadAsByteArray();
    std::string fromBytes(bytes.begin(), bytes.end());
    EXPECT_EQ(fromBytes, c.ReadAsString());
}

TEST(MultipartFormDataContentTests, DefaultContentType_IsFormData) {
    MultipartFormDataContent c("B");
    EXPECT_EQ(c.getContentTypeProperty(), "multipart/form-data; boundary=\"B\"");
}

TEST(MultipartFormDataContentTests, AddWithName_IncludesContentDisposition) {
    MultipartFormDataContent c("B");
    c.Add(std::make_shared<StringContent>("value1"), "field1");
    std::string body = c.ReadAsString();
    EXPECT_NE(body.find("Content-Disposition: form-data; name=\"field1\""), std::string::npos);
    EXPECT_NE(body.find("value1"), std::string::npos);
}

TEST(MultipartFormDataContentTests, AddWithFileName_IncludesFileName) {
    MultipartFormDataContent c("B");
    c.Add(std::make_shared<StringContent>("filedata"), "file1", "test.txt");
    std::string body = c.ReadAsString();
    EXPECT_NE(body.find("name=\"file1\""), std::string::npos);
    EXPECT_NE(body.find("filename=\"test.txt\""), std::string::npos);
}

TEST(MultipartFormDataContentTests, IsA_MultipartContent) {
    MultipartFormDataContent c("B");
    MultipartContent* base = &c;
    EXPECT_EQ(base->getContentTypeProperty(), "multipart/form-data; boundary=\"B\"");
}

// ---------------------------------------------------------------------------
// performRequest() response-header parsing (ticket 267) -- loopback server, since
// Content-Length/chunk-size parsing lives inside the file-local performRequest(), not exposed
// like parseUrl()/parseStatusLine() are for direct unit testing.
// ---------------------------------------------------------------------------

namespace {
SharpRuntime::intcs startMockHttpServer(std::shared_ptr<System::Net::Sockets::Socket>& listenerOut) {
    listenerOut = std::make_shared<System::Net::Sockets::Socket>(
        System::Net::Sockets::AddressFamily::InterNetwork, System::Net::Sockets::SocketType::Stream,
        System::Net::Sockets::ProtocolType::Tcp);
    listenerOut->Bind(System::Net::IPEndPoint(System::Net::IPAddress::Loopback, 0));
    listenerOut->Listen();
    auto local = std::dynamic_pointer_cast<System::Net::IPEndPoint>(listenerOut->getLocalEndPointProperty());
    return local->getPortProperty();
}

std::vector<SharpRuntime::bytecs> toBytes(const std::string& s) {
    return std::vector<SharpRuntime::bytecs>(s.begin(), s.end());
}
} // namespace

// Regression test (ticket 267): a malformed/non-numeric Content-Length header threw a raw
// std::invalid_argument straight out of Send()/GetAsync(), invisible to code catching
// System::Exception&/HttpRequestException& -- same bug class parseUrl()/parseStatusLine() in
// this same file were already fixed for; this one was missed.
TEST(HttpClientTests, MalformedContentLengthHeader_ThrowsHttpRequestException) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(4096);
        server->Receive(reqBuf);
        server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: not-a-number\r\n\r\n"));
        server->Close();
    });

    HttpClient client;
    EXPECT_THROW(client.GetString("http://127.0.0.1:" + std::to_string(port) + "/"), HttpRequestException);

    serverThread.join();
}

// Regression test (ticket 267): a malformed (non-hex) chunk-size line in a chunked response
// threw a raw std::invalid_argument the same way.
TEST(HttpClientTests, MalformedChunkSize_ThrowsHttpRequestException) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(4096);
        server->Receive(reqBuf);
        server->Send(toBytes("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZZ\r\ndata\r\n0\r\n\r\n"));
        server->Close();
    });

    HttpClient client;
    EXPECT_THROW(client.GetString("http://127.0.0.1:" + std::to_string(port) + "/"), HttpRequestException);

    serverThread.join();
}

// Regression test (ticket 932): a server that closes the connection before delivering all
// `Content-Length`-declared bytes previously made recvExact() silently return the truncated data
// it had so far, so the caller saw a "successful" (but incomplete) response instead of an error.
TEST(HttpClientTests, ConnectionClosedBeforeContentLengthBytes_ThrowsHttpRequestException) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(4096);
        server->Receive(reqBuf);
        // Declares 100 bytes of body but sends only 5, then closes.
        server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nhello"));
        server->Close();
    });

    HttpClient client;
    EXPECT_THROW(client.GetString("http://127.0.0.1:" + std::to_string(port) + "/"), HttpRequestException);

    serverThread.join();
}

// ---------------------------------------------------------------------------
// HttpMessageHandler / DelegatingHandler (ticket 1496)
// ---------------------------------------------------------------------------

namespace {
// A DelegatingHandler spy that records whether it was invoked and injects a header, then
// forwards to the real inner handler -- the classic auth-injection/logging middleware pattern.
class SpyHandler : public DelegatingHandler {
public:
    explicit SpyHandler(std::shared_ptr<HttpMessageHandler> inner) : DelegatingHandler(std::move(inner)) {}

    bool invoked = false;

    std::shared_ptr<HttpResponseMessage> Send(std::shared_ptr<HttpRequestMessage> request) override {
        invoked = true;
        request->setHeader("X-Spy-Injected", "yes");
        return DelegatingHandler::Send(std::move(request));
    }
};

class RecordingHandler final : public HttpMessageHandler {
public:
    bool sent = false;
    int disposeCount = 0;
    std::shared_ptr<HttpRequestMessage> receivedRequest;

    std::shared_ptr<HttpResponseMessage> Send(std::shared_ptr<HttpRequestMessage> request) override {
        sent = true;
        receivedRequest = std::move(request);
        return std::make_shared<HttpResponseMessage>(HttpStatusCode::Accepted);
    }

    void Dispose() override { ++disposeCount; }
};
} // namespace

TEST(HttpMessageHandlerTests, HttpClient_CustomHandlerChain_IsInvokedEndToEnd) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::string receivedRequest;
    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(4096);
        SharpRuntime::intcs n = server->Receive(reqBuf);
        receivedRequest.assign(reqBuf.begin(), reqBuf.begin() + n);
        server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
        server->Close();
    });

    auto terminal = std::make_shared<HttpClientHandler>();
    auto spy = std::make_shared<SpyHandler>(terminal);
    HttpClient client(spy);

    std::string body = client.GetString("http://127.0.0.1:" + std::to_string(port) + "/");
    serverThread.join();

    EXPECT_TRUE(spy->invoked);
    EXPECT_EQ(body, "OK");
    EXPECT_NE(receivedRequest.find("X-Spy-Injected: yes"), std::string::npos);
}

// ---------------------------------------------------------------------------
// #2128 -- RFC 9112 6.1 on the wire, which is the only place it can be enforced
//
// Transfer-Encoding is a REQUEST header and Content-Length a CONTENT header. They live in two
// collections that cannot see each other, which is why neither this port's header collections
// nor .NET's enforce their coexistence rule -- the message writer does. These two cases assert
// the rule where it actually applies: on the bytes that leave the socket.
// ---------------------------------------------------------------------------

namespace {
std::string captureRequestBytes(const std::function<void(HttpClient&, const std::string&)>& act) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::string received;
    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(8192);
        SharpRuntime::intcs n = server->Receive(reqBuf);
        received.assign(reqBuf.begin(), reqBuf.begin() + n);
        server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
        server->Close();
    });

    HttpClient client;
    act(client, "http://127.0.0.1:" + std::to_string(port) + "/");
    serverThread.join();
    return received;
}

bool mentionsHeader(const std::string& request, const std::string& name) {
    return request.find("\r\n" + name + ":") != std::string::npos;
}
} // namespace

TEST(HttpClientWireFormatTests, RequestWithTransferEncodingOmitsContentLength) {
    // A request carrying BOTH is the exact shape a request-smuggling chain relies on two
    // intermediaries disagreeing about: one frames by Content-Length, the other by chunks.
    const std::string sent = captureRequestBytes([](HttpClient& client, const std::string& url) {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Post(), url);
        request->setContentProperty(std::make_shared<StringContent>("payload"));
        request->setHeader("Transfer-Encoding", "chunked");
        (void)client.Send(request);
    });

    EXPECT_TRUE(mentionsHeader(sent, "Transfer-Encoding")) << sent;
    EXPECT_FALSE(mentionsHeader(sent, "Content-Length"))
        << "RFC 9112 6.1: Content-Length must not accompany Transfer-Encoding\n" << sent;
}

TEST(HttpClientWireFormatTests, ACallerSuppliedContentLengthIsNotEmittedTwice) {
    // The handler derives its own Content-Length from the body. Before #2128 it emitted that
    // line unconditionally, so a caller who set the header themselves got the field twice in one
    // message -- the same disagreement, reached by a different route.
    const std::string sent = captureRequestBytes([](HttpClient& client, const std::string& url) {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Post(), url);
        request->setContentProperty(std::make_shared<StringContent>("payload"));
        request->setHeader("Content-Length", "7");
        (void)client.Send(request);
    });

    std::size_t count = 0;
    for (std::size_t at = sent.find("\r\nContent-Length:"); at != std::string::npos;
         at = sent.find("\r\nContent-Length:", at + 1)) {
        ++count;
    }
    EXPECT_EQ(count, 1u) << "Content-Length was emitted " << count << " times\n" << sent;
}

TEST(HttpClientWireFormatTests, AnOrdinaryRequestStillCarriesItsContentLength) {
    // The control the two cases above need: without Transfer-Encoding and without a
    // caller-supplied value, the handler's own Content-Length is still emitted, so neither rule
    // above can be satisfied by simply never sending the header.
    const std::string sent = captureRequestBytes([](HttpClient& client, const std::string& url) {
        auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Post(), url);
        request->setContentProperty(std::make_shared<StringContent>("payload"));
        (void)client.Send(request);
    });

    EXPECT_NE(sent.find("\r\nContent-Length: 7\r\n"), std::string::npos) << sent;
}

// ---------------------------------------------------------------------------
// #2066 / SR-AUD-310 (CCF-019) — the liveness boundary
//
// SendAsync/GetAsync/PostAsync/GetStringAsync/GetByteArrayAsync each build a TaskT from a lambda
// capturing raw `this`; a task outliving its client read a freed defaultHeaders_ and a freed
// handler_, which the audit confirmed under ASan. .NET needs no boundary because the GC keeps
// the client alive for as long as the captured delegate can reach it; C++ has no such mechanism,
// so ~HttpClient waits -- the same shape #2134 gave Socket.
// ---------------------------------------------------------------------------

TEST(HttpClientLivenessTests, DestroyingTheClientWaitsForAnInFlightRequest) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    // The server holds the request open briefly, so the destructor really meets an in-flight
    // body rather than winning a race against one that has already finished.
    std::thread serverThread([&]() {
        auto server = listener->Accept();
        std::vector<SharpRuntime::bytecs> reqBuf(4096);
        (void)server->Receive(reqBuf);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
        server->Close();
    });

    std::optional<System::Threading::Tasks::TaskT<std::string>> pending;
    {
        HttpClient client;
        pending = client.GetStringAsync("http://127.0.0.1:" + std::to_string(port) + "/");
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }   // ~HttpClient must WAIT here

    // THE assertion: the destructor must not RETURN until the body is finished. Checked before
    // anything else can let the body catch up, and as an ASSERT because getResultProperty()
    // below blocks -- without the boundary this would hang rather than fail.
    ASSERT_TRUE(pending->getIsCompletedProperty())
        << "the destructor returned while an async request was still running";
    EXPECT_EQ(pending->getResultProperty(), "OK");
    serverThread.join();
}

TEST(HttpClientLivenessTests, TheBoundaryIsANoOpWhenNothingIsInFlight) {
    // The control: a client with no outstanding work must not pay for the boundary, and a
    // destructor that waited unconditionally would hang here rather than return.
    const auto started = std::chrono::steady_clock::now();
    { HttpClient client; }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);
}

TEST(HttpMessageHandlerTests, DelegatingHandler_NoInnerHandler_Throws) {
    DelegatingHandler handler;
    auto req = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
    EXPECT_THROW(handler.Send(req), System::InvalidOperationException);
}

TEST(HttpMessageHandlerTests, DelegatingHandler_InnerHandlerProperty_RoundTrips) {
    auto inner = std::make_shared<HttpClientHandler>();
    DelegatingHandler handler(inner);
    EXPECT_EQ(handler.getInnerHandlerProperty(), inner);
}

TEST(HttpMessageInvokerTests, Send_ForwardsRequestToHandler) {
    auto handler = std::make_shared<RecordingHandler>();
    HttpMessageInvoker invoker(handler, false);
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");

    auto response = invoker.Send(request);
    EXPECT_TRUE(handler->sent);
    EXPECT_EQ(handler->receivedRequest, request);
    EXPECT_EQ(response->getStatusCodeProperty(), HttpStatusCode::Accepted);
}

TEST(HttpMessageInvokerTests, SendAsync_ForwardsRequestToHandler) {
    auto handler = std::make_shared<RecordingHandler>();
    HttpMessageInvoker invoker(handler, false);
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");

    auto task = invoker.SendAsync(request);
    auto response = task.getResultProperty();
    EXPECT_TRUE(handler->sent);
    EXPECT_EQ(response->getStatusCodeProperty(), HttpStatusCode::Accepted);
}

TEST(HttpMessageInvokerTests, Dispose_HandlerPolicyAndSubsequentSendMatchDotNetLifetimeContract) {
    auto handler = std::make_shared<RecordingHandler>();
    HttpMessageInvoker invoker(handler, true);
    invoker.Dispose();
    invoker.Dispose();

    EXPECT_EQ(handler->disposeCount, 1);
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
    EXPECT_THROW(invoker.Send(request), System::ObjectDisposedException);
}

TEST(HttpMessageInvokerTests, CancelledToken_PreventsHandlerInvocation) {
    auto handler = std::make_shared<RecordingHandler>();
    HttpMessageInvoker invoker(handler, false);
    System::Threading::CancellationTokenSource cancellationSource;
    cancellationSource.Cancel();
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");

    EXPECT_THROW(invoker.Send(request, cancellationSource.getTokenProperty()),
                 System::OperationCanceledException);
    EXPECT_FALSE(handler->sent);
}

TEST(HttpMessageInvokerTests, NullHandlerAndRequest_ThrowArgumentNullException) {
    std::shared_ptr<HttpMessageHandler> nullHandler;
    EXPECT_THROW((void)HttpMessageInvoker{nullHandler}, System::ArgumentNullException);

    auto handler = std::make_shared<RecordingHandler>();
    HttpMessageInvoker invoker(handler, false);
    std::shared_ptr<HttpRequestMessage> nullRequest;
    EXPECT_THROW(invoker.Send(nullRequest), System::ArgumentNullException);
}

TEST(HttpClientHandlerTests, CapabilityFlagsAndDisposeState_DescribeTheLightweightImplementation) {
    HttpClientHandler handler;
    EXPECT_FALSE(handler.getSupportsAutomaticDecompressionProperty());
    EXPECT_FALSE(handler.getSupportsProxyProperty());
    EXPECT_FALSE(handler.getSupportsRedirectConfigurationProperty());
    EXPECT_TRUE(handler.getUseCookiesProperty());
    EXPECT_THROW(handler.setCookieContainerProperty(nullptr), System::ArgumentNullException);

    handler.Dispose();
    EXPECT_THROW(handler.getUseCookiesProperty(), System::ObjectDisposedException);
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
    EXPECT_THROW(handler.Send(request), System::ObjectDisposedException);
}

// ---------------------------------------------------------------------------
// Cookie (ticket 1498)
// ---------------------------------------------------------------------------

using System::Net::Cookie;
using System::Net::CookieContainer;
using System::Net::CookieException;

TEST(CookieTests, NameValue_ToString) {
    Cookie c("session", "abc123");
    EXPECT_EQ(c.ToString(), "session=abc123");
}

TEST(CookieTests, EmptyName_Throws) {
    EXPECT_THROW(Cookie("", "v"), CookieException);
}

TEST(CookieTests, NameStartingWithDollar_Throws) {
    EXPECT_THROW(Cookie("$reserved", "v"), CookieException);
}

TEST(CookieTests, NameWithEquals_Throws) {
    EXPECT_THROW(Cookie("na=me", "v"), CookieException);
}

TEST(CookieTests, DefaultPath_IsRoot) {
    Cookie c("a", "b");
    EXPECT_EQ(c.getPathProperty(), "/");
}

TEST(CookieTests, Expired_SessionCookie_NeverExpires) {
    Cookie c("a", "b");
    EXPECT_FALSE(c.getExpiredProperty());
}

// ---------------------------------------------------------------------------
// CookieContainer (ticket 1498)
// ---------------------------------------------------------------------------

TEST(CookieContainerTests, GetCookieHeader_ExactDomainMatch) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    container.Add(uri, Cookie("session", "abc"));
    EXPECT_EQ(container.GetCookieHeader(uri), "session=abc");
}

TEST(CookieContainerTests, GetCookieHeader_DifferentDomain_NoLeak) {
    CookieContainer container;
    container.Add(System::Uri("http://example.com/"), Cookie("session", "abc"));
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://other.com/")), "");
}

TEST(CookieContainerTests, GetCookieHeader_LeadingDotDomain_MatchesSubdomain) {
    CookieContainer container;
    Cookie c("session", "abc");
    c.setDomainProperty(".example.com");
    container.Add(System::Uri("http://example.com/"), c);
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://api.example.com/")), "session=abc");
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://example.com/")), "session=abc");
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://notexample.com/")), "");
}

TEST(CookieContainerTests, GetCookieHeader_PathScoped_NoLeakOutsidePath) {
    CookieContainer container;
    Cookie c("session", "abc", "/api");
    container.Add(System::Uri("http://example.com/api/"), c);
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://example.com/api/users")), "session=abc");
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://example.com/other")), "");
}

TEST(CookieContainerTests, GetCookieHeader_SecureCookie_OnlyOverHttps) {
    CookieContainer container;
    Cookie c("session", "abc");
    c.setSecureProperty(true);
    container.Add(System::Uri("http://example.com/"), c);
    EXPECT_EQ(container.GetCookieHeader(System::Uri("http://example.com/")), "");
    EXPECT_EQ(container.GetCookieHeader(System::Uri("https://example.com/")), "session=abc");
}

TEST(CookieContainerTests, GetCookieHeader_MultipleCookies_JoinedWithSemicolon) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    container.Add(uri, Cookie("a", "1"));
    container.Add(uri, Cookie("b", "2"));
    std::string header = container.GetCookieHeader(uri);
    EXPECT_NE(header.find("a=1"), std::string::npos);
    EXPECT_NE(header.find("b=2"), std::string::npos);
    EXPECT_NE(header.find("; "), std::string::npos);
}

TEST(CookieContainerTests, SetCookies_ParsesNameValue) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    container.SetCookies(uri, "session=abc123");
    EXPECT_EQ(container.GetCookieHeader(uri), "session=abc123");
}

TEST(CookieContainerTests, SetCookies_ParsesDomainAndPathAttributes) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    container.SetCookies(uri, "session=abc; Domain=.example.com; Path=/app; Secure; HttpOnly");
    EXPECT_EQ(container.GetCookieHeader(System::Uri("https://sub.example.com/app/page")), "session=abc");
}

TEST(CookieContainerTests, SetCookies_MaxAgeZero_ExpiresImmediately) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    container.SetCookies(uri, "session=abc; Max-Age=0");
    EXPECT_EQ(container.GetCookieHeader(uri), "");
}

TEST(CookieContainerTests, SetCookies_MissingEquals_Throws) {
    CookieContainer container;
    System::Uri uri("http://example.com/");
    EXPECT_THROW(container.SetCookies(uri, "not-a-valid-cookie"), CookieException);
}

// ---------------------------------------------------------------------------
// HttpClientHandler cookie wiring (ticket 1498) -- loopback server integration
// ---------------------------------------------------------------------------

TEST(HttpClientHandlerCookieTests, SetCookieResponse_CapturedAndSentOnNextRequest) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);
    std::string base = "http://127.0.0.1:" + std::to_string(port);

    std::string secondRequestRaw;
    std::thread serverThread([&]() {
        // First request: server issues a Set-Cookie.
        auto s1 = listener->Accept();
        std::vector<SharpRuntime::bytecs> buf1(4096);
        s1->Receive(buf1);
        s1->Send(toBytes(
            "HTTP/1.1 200 OK\r\nSet-Cookie: sid=xyz789; Path=/\r\nContent-Length: 2\r\n\r\nOK"));
        s1->Close();

        // Second request: client should now send the Cookie header back.
        auto s2 = listener->Accept();
        std::vector<SharpRuntime::bytecs> buf2(4096);
        SharpRuntime::intcs n2 = s2->Receive(buf2);
        secondRequestRaw.assign(buf2.begin(), buf2.begin() + n2);
        s2->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
        s2->Close();
    });

    HttpClient client;
    client.GetString(base + "/login");
    client.GetString(base + "/dashboard");
    serverThread.join();

    EXPECT_NE(secondRequestRaw.find("Cookie: sid=xyz789"), std::string::npos);
}

TEST(HttpClientHandlerCookieTests, UseCookies_False_DoesNotSendCapturedCookie) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);
    std::string base = "http://127.0.0.1:" + std::to_string(port);

    std::string secondRequestRaw;
    std::thread serverThread([&]() {
        auto s1 = listener->Accept();
        std::vector<SharpRuntime::bytecs> buf1(4096);
        s1->Receive(buf1);
        s1->Send(toBytes(
            "HTTP/1.1 200 OK\r\nSet-Cookie: sid=xyz789; Path=/\r\nContent-Length: 2\r\n\r\nOK"));
        s1->Close();

        auto s2 = listener->Accept();
        std::vector<SharpRuntime::bytecs> buf2(4096);
        SharpRuntime::intcs n2 = s2->Receive(buf2);
        secondRequestRaw.assign(buf2.begin(), buf2.begin() + n2);
        s2->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
        s2->Close();
    });

    auto handler = std::make_shared<HttpClientHandler>();
    handler->setUseCookiesProperty(false);
    HttpClient client(handler);
    client.GetString(base + "/login");
    client.GetString(base + "/dashboard");
    serverThread.join();

    EXPECT_EQ(secondRequestRaw.find("Cookie:"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Ticket #2065 -- a post-connect throw must not leak the socket descriptor
// (SR-AUD-318's leak half; docs/SystemNetHttpNamespaceReviewPlan.md §4.7).
//
// Before #2065 the connected descriptor was a bare local closed at exactly one
// point, after the whole body had been read, so every throw in between leaked
// it -- and all four of those throws are chosen by the REMOTE PEER. Measured
// then: 20 requests, 20 leaked descriptors, in each of four modes.
//
// The instrument is the process's own open-descriptor count. LSan does NOT
// cover this: it tracks memory, not descriptors, so a clean LSan run would say
// nothing about it and must not be substituted. Where /proc/self/fd does not
// exist (any non-Linux platform) the assertion is SKIPPED rather than failed --
// a missing instrument is not a passing measurement.
// ---------------------------------------------------------------------------

namespace {

#if defined(__linux__)
constexpr bool kDescriptorCountAvailable = true;
SharpRuntime::intcs openDescriptorCount() {
    SharpRuntime::intcs count = 0;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator("/proc/self/fd", ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        ++count;
    }
    return count;
}
#else
constexpr bool kDescriptorCountAvailable = false;
SharpRuntime::intcs openDescriptorCount() { return 0; }
#endif

// Unblocks a mock server still waiting in Accept() because the client never
// connected, so a test whose client fails early reports a FAILURE instead of
// hanging forever in join(). Pair it with a server lambda that swallows its own
// exceptions: between them, the server thread always terminates and join()
// always returns, whatever the client did. Both halves are needed -- a mutation
// check that hangs or aborts proves nothing about the test it was aimed at.
void pokeMockServer(SharpRuntime::intcs port) {
    try {
        System::Net::Sockets::Socket poke(
            System::Net::Sockets::AddressFamily::InterNetwork,
            System::Net::Sockets::SocketType::Stream,
            System::Net::Sockets::ProtocolType::Tcp);
        poke.Connect(System::Net::IPEndPoint(System::Net::IPAddress::Loopback, port));
        poke.Close();
    } catch (const System::Exception&) {
        // The server may already have gone; nothing to unblock.
    }
}

// The result of one descriptor measurement. BOTH endpoints are reported, not just their
// difference: a bare delta cannot distinguish "nothing leaked" from "the instrument was
// measuring the wrong thing", and telling those apart is exactly what ticket #2107 is about.
struct DescriptorMeasurement {
    SharpRuntime::intcs before = 0;
    SharpRuntime::intcs after = 0;
    bool quiesced = false;   ///< false => the server thread never reached a known state
    [[nodiscard]] SharpRuntime::intcs delta() const { return after - before; }
};

// Waits for a specific EVENT -- the mock server having closed its `n`th accepted socket -- and
// NOT for an arbitrary duration. The deadline is only a safety bound so that a wedged server
// thread FAILS the test instead of hanging it; the measurement never relies on it elapsing.
// A sleep long enough to "usually" work is precisely the fix ticket #2107 forbids.
bool waitForServerToClose(const std::atomic<int>& closed, int n) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (closed.load(std::memory_order_acquire) < n) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

// Runs `iterations` requests against a loopback server that always answers with
// `reply`, and reports this process's open-descriptor count before and after.
//
// TICKET #2107 -- WHY BOTH SAMPLES ARE QUIESCED. The mock server runs on its own thread and
// holds an accepted socket for each connection. That socket is a descriptor of THIS process, so
// whether it happens to be open at a sample point is pure scheduling luck, and it corrupted the
// measurement in BOTH directions. Measured deterministically by injecting a server-side delay
// (build-probe/2107_probe1_instrument.cpp, log build-probe/2107_probe1_before.log):
//
//   server lags on the WARM-UP connection -> `before` is inflated by 1 -> delta = -1
//   server lags on the LAST    connection -> `after`  is inflated by 1 -> delta = +1
//
// The -1 is the symptom that was observed in the wild, and it is definitionally not a leak. The
// +1 is worse and was NOT recorded on the ticket: it is a FALSE LEAK REPORT that would send a
// maintainer hunting through production code for a defect that does not exist.
//
// #2107's stated root cause -- "the after sample is taken before join()" -- names only HALF the
// fix. Joining before the final sample removes the +1 direction completely, but the probe shows
// the -1 direction SURVIVES it, because that one corrupts the BASELINE. Both ends must be
// quiesced:
//
//   * `before` is taken only after the server has closed the warm-up connection;
//   * `after`  is taken only after the server thread has been JOINED, so no accepted socket of
//     any iteration can still be open.
//
// Between the two samples the only thread touching descriptors is the client, so nothing
// unrelated is being counted. The listener stays open across both samples and cancels out.
//
// This does NOT weaken the guarantees #2063/#2065 established: a descriptor genuinely leaked by
// the client is still open after the join, and is still counted. Proven by mutation.
DescriptorMeasurement descriptorDeltaOver(const std::string& reply,
                                          int iterations,
                                          int& threwOut) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::atomic<int> connectionsClosed{0};

    std::thread serverThread([&]() {
        // The server swallows its own exceptions so that join() always returns. Without this an
        // Accept() that threw would escape a std::thread and call std::terminate, turning a test
        // failure into a process abort that proves nothing.
        try {
            for (int i = 0; i < iterations; ++i) {
                auto server = listener->Accept();
                std::vector<SharpRuntime::bytecs> reqBuf(4096);
                server->Receive(reqBuf);
                server->Send(toBytes(reply));
                server->Close();
                connectionsClosed.fetch_add(1, std::memory_order_release);
            }
        } catch (const System::Exception&) {
            // Leave connectionsClosed where it is; the waiter's deadline turns this into a
            // reported failure rather than a hang.
        }
    });

    HttpClient client;
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";

    // One warm-up request outside the measurement: the first call through this
    // client resolves, allocates and may open descriptors that legitimately
    // stay open, and counting those would make every mode look like a leak.
    try { (void)client.Get(url); } catch (const System::Exception&) {}

    DescriptorMeasurement measurement;
    // The client returning is NOT enough: it says nothing about the server's end of the same
    // connection. Wait for the server to have closed it before establishing the baseline.
    measurement.quiesced = waitForServerToClose(connectionsClosed, 1);
    measurement.before = openDescriptorCount();

    threwOut = 0;
    for (int i = 1; i < iterations; ++i) {
        try {
            (void)client.Get(url);
        } catch (const System::Exception&) {
            ++threwOut;
        }
    }

    // JOIN BEFORE SAMPLING, not after. Every accepted socket is closed once this returns.
    serverThread.join();
    measurement.after = openDescriptorCount();

    listener->Close();
    return measurement;
}

// The assertion every descriptor-leak test makes. It exists so that the three distinct outcomes
// stay distinguishable in the failure message:
//
//   delta > 0  a real leak -- what these tests are for;
//   delta < 0  IMPOSSIBLE as a leak. Fewer descriptors after than before means the instrument
//              measured something other than the client, and it must be reported as an
//              instrument fault rather than silently passing an `EXPECT_LE(0, delta)` or
//              failing as though it were a leak;
//   !quiesced  the server thread never reached a known state, so nothing was measured at all.
void expectNoDescriptorLeak(const DescriptorMeasurement& m, const char* what) {
    EXPECT_TRUE(m.quiesced)
        << what << ": the mock server never quiesced, so no measurement was taken";
    if (m.delta() < 0) {
        ADD_FAILURE() << what << ": INSTRUMENT FAULT, not a leak -- the descriptor count FELL from "
                      << m.before << " to " << m.after << " (delta " << m.delta()
                      << "). A negative delta cannot represent a leak; see ticket #2107.";
        return;
    }
    EXPECT_EQ(m.before, m.after)
        << what << ": descriptor count went from " << m.before << " to " << m.after
        << " (delta " << m.delta() << ")";
}

} // namespace

TEST(HttpClientDescriptorLeakTests, GarbledStatusLineDoesNotLeakADescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(descriptorDeltaOver("GARBLED\r\n\r\n", 20, threw),
                           "GarbledStatusLine");
    EXPECT_EQ(19, threw) << "every request must still fail -- otherwise nothing was measured";
}

TEST(HttpClientDescriptorLeakTests, MalformedContentLengthDoesNotLeakADescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(descriptorDeltaOver(
        "HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n", 20, threw), "MalformedContentLength");
    EXPECT_EQ(19, threw);
}

TEST(HttpClientDescriptorLeakTests, MalformedChunkSizeDoesNotLeakADescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(descriptorDeltaOver(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZZ\r\n", 20, threw),
        "MalformedChunkSize");
    EXPECT_EQ(19, threw);
}

TEST(HttpClientDescriptorLeakTests, TruncatedBodyDoesNotLeakADescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(descriptorDeltaOver(
        "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort", 20, threw), "TruncatedBody");
    EXPECT_EQ(19, threw);
}

TEST(HttpClientDescriptorLeakTests, TheSuccessPathStillClosesItsDescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(descriptorDeltaOver(
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi", 20, threw), "SuccessPath");
    EXPECT_EQ(0, threw) << "the success path must not throw -- otherwise the wrong path was measured";
}

// ---------------------------------------------------------------------------
// Ticket #2063 -- a control character must not cross a public door into an HTTP
// or MIME frame (SR-AUD-313 and SR-AUD-316's reason half, cause NH-B;
// docs/SystemNetHttpNamespaceReviewPlan.md §4.1 and §20.3).
//
// CR and LF terminate a field in an HTTP/1.1 or MIME frame and NUL truncates
// every C-string-shaped consumer downstream of it, so any of the three inside a
// value this port concatenates into such a frame emits fields the caller never
// wrote. Measured before the repair, ALL of these doors were open:
//
//   request/response header name, request/response header value, the client's
//   default header name and value, the reason phrase, the whole request URI
//   (authority AND path -- a CRLF in the path injects a second REQUEST LINE),
//   the response status line, a response header line, StringContent's charset
//   and media type, the three other contents' media type, MultipartContent's
//   subtype, and MultipartFormDataContent's name and file name.
//
// The narrowing is deliberate and documented in
// docs/Migration-HttpControlCharacterRejection.md.
// ---------------------------------------------------------------------------

namespace {

// Each control character at the START, in the MIDDLE and at the END of a field,
// plus the CRLF pair. A repair that only inspected one end passes a
// single-position test and fails this one.
const std::vector<std::string>& controlBearingFields() {
    static const std::vector<std::string> values = {
        "\ra", "a\rb", "a\r",
        "\na", "a\nb", "a\n",
        std::string("\0a", 2), std::string("a\0b", 3), std::string("a\0", 2),
        "a\r\nb", "\r\n", "a\r\nX-Injected: yes",
    };
    return values;
}

// Text that is legal in an HTTP field and must STILL be accepted, so the
// narrowing cannot quietly grow.
const std::vector<std::string>& legalFields() {
    static const std::vector<std::string> values = {
        "", "a", "a b", "a\tb", " ", "\t", "text/plain; charset=utf-8",
        "%0d%0a", "a;b", "a\"b",
    };
    return values;
}

} // namespace

TEST(HttpControlCharacterTests, RequestSetHeaderValue_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        HttpRequestMessage request;
        EXPECT_THROW(request.setHeader("X-A", bad), System::FormatException);
        EXPECT_TRUE(request.getHeadersProperty().empty())
            << "a rejected header must not have been stored";
    }
}

TEST(HttpControlCharacterTests, RequestSetHeaderName_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        HttpRequestMessage request;
        EXPECT_THROW(request.setHeader(bad, "v"), System::FormatException);
        EXPECT_TRUE(request.getHeadersProperty().empty());
    }
}

TEST(HttpControlCharacterTests, RequestSetHeader_LegalText_StillAccepted) {
    for (const auto& good : legalFields()) {
        HttpRequestMessage request;
        EXPECT_NO_THROW(request.setHeader("X-A", good)) << "value must stay accepted";
        EXPECT_EQ(request.getHeadersProperty().at("X-A"), good);
    }
}

TEST(HttpControlCharacterTests, ResponseSetHeader_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        HttpResponseMessage response;
        EXPECT_THROW(response.setHeader("X-A", bad), System::FormatException);
        EXPECT_THROW(response.setHeader(bad, "v"), System::FormatException);
        EXPECT_TRUE(response.getHeadersProperty().empty());
    }
}

TEST(HttpControlCharacterTests, ResponseSetHeader_LegalText_StillAccepted) {
    for (const auto& good : legalFields()) {
        HttpResponseMessage response;
        EXPECT_NO_THROW(response.setHeader("X-A", good));
        EXPECT_EQ(response.getHeader("X-A"), good);
    }
}

TEST(HttpControlCharacterTests, SetDefaultHeader_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        HttpClient client;
        EXPECT_THROW(client.setDefaultHeader("X-A", bad), System::FormatException);
        EXPECT_THROW(client.setDefaultHeader(bad, "v"), System::FormatException);
        EXPECT_EQ(client.getDefaultHeader("X-A"), "");
    }
}

TEST(HttpControlCharacterTests, SetDefaultHeader_LegalText_StillAccepted) {
    HttpClient client;
    for (const auto& good : legalFields()) {
        EXPECT_NO_THROW(client.setDefaultHeader("X-A", good));
        EXPECT_EQ(client.getDefaultHeader("X-A"), good);
    }
}

TEST(HttpControlCharacterTests, ReasonPhrase_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        HttpResponseMessage response;
        EXPECT_THROW(response.setReasonPhraseProperty(bad), System::FormatException);
        EXPECT_EQ(response.getReasonPhraseProperty(), "")
            << "a rejected reason phrase must not have been stored";
    }
}

TEST(HttpControlCharacterTests, ReasonPhrase_LegalText_StillAccepted) {
    for (const auto& good : legalFields()) {
        HttpResponseMessage response;
        EXPECT_NO_THROW(response.setReasonPhraseProperty(good));
        EXPECT_EQ(response.getReasonPhraseProperty(), good);
    }
}

// The review plan named the AUTHORITY as the request URI's injection vector.
// The PATH is a second one and is worse: it lands in the request LINE, so a
// CRLF there injects a whole second request rather than one header field.
TEST(HttpControlCharacterTests, ParseUrl_ControlCharacterInAnyComponent_ThrowsUriFormatException) {
    const std::vector<std::string> urls = {
        "http://ho\rst/p",
        "http://ho\nst/p",
        "http://ho\r\nst/p",
        std::string("http://ho\0st/p", 14),
        "http://host/pa\r\nX-Injected: yes",      // the request LINE
        "http://host/p?q=a\r\nb",
        "http://host/p#f\r\n",
        "http://host:8\r\n0/p",
        "http\r\n://host/p",
        "http://host/p\r",
        "http://host/p\n",
        std::string("http://host/p\0", 14),
    };
    for (const auto& url : urls) {
        EXPECT_THROW(HttpClient::parseUrl(url), System::UriFormatException) << "url index";
    }
}

// System::UriFormatException IS a System::FormatException, so the whole family
// of doors reports through one catchable base.
TEST(HttpControlCharacterTests, ParseUrl_RejectionIsAlsoAFormatException) {
    EXPECT_THROW(HttpClient::parseUrl("http://ho\r\nst/p"), System::FormatException);
}

// A percent-encoded CRLF is ordinary text, not a control character, and must
// stay accepted: escaping it is exactly how a caller passes such bytes safely.
TEST(HttpControlCharacterTests, ParseUrl_PercentEncodedControlCharacters_StillAccepted) {
    auto parsed = HttpClient::parseUrl("http://host/p%0d%0aX:%20y");
    EXPECT_EQ(parsed.host, "host");
    EXPECT_EQ(parsed.path, "/p%0d%0aX:%20y");
}

TEST(HttpControlCharacterTests, ParseStatusLine_ControlCharacter_ThrowsHttpRequestException) {
    const std::vector<std::string> lines = {
        std::string("HTTP/1.1 200 O\0K", 16),
        "HTTP/1.1 200 O\rK",
        "HTTP/1.1 200 O\nK",
        "HTTP/1.1 200 OK\r",
        "HTTP/1.1 200 OK\r\nX-Injected: yes",
    };
    for (const auto& line : lines) {
        EXPECT_THROW(HttpClient::parseStatusLine(line), HttpRequestException);
    }
}

TEST(HttpControlCharacterTests, ContentMediaTypeAndCharset_ControlCharacter_ThrowsFormatException) {
    for (const auto& bad : controlBearingFields()) {
        EXPECT_THROW(StringContent("body", "utf-8", bad), System::FormatException);
        EXPECT_THROW(StringContent("body", bad, "text/plain"), System::FormatException);
        EXPECT_THROW(ByteArrayContent(std::vector<SharpRuntime::bytecs>{1, 2}, bad),
                     System::FormatException);

        std::vector<SharpRuntime::bytecs> data = {1};
        System::ReadOnlyMemory<SharpRuntime::bytecs> memory(data);
        EXPECT_THROW(ReadOnlyMemoryContent(memory, bad), System::FormatException);

        auto stream = std::make_shared<System::IO::MemoryStream>();
        EXPECT_THROW(StreamContent(stream, bad), System::FormatException);
    }
}

// The BODY is payload, not a protocol field, and must not be validated.
TEST(HttpControlCharacterTests, ContentBodyWithControlCharacters_StillAccepted) {
    StringContent content("line1\r\nline2\r\n", "utf-8", "text/plain");
    EXPECT_EQ(content.ReadAsString(), "line1\r\nline2\r\n");
    ByteArrayContent bytes(std::vector<SharpRuntime::bytecs>{'\r', '\n', 0, 'x'});
    EXPECT_EQ(bytes.ReadAsByteArray().size(), 4u);
}

// A null stream is still reported as a null stream even when the media type is
// also malformed -- the pre-existing check runs first.
TEST(HttpControlCharacterTests, StreamContent_NullStreamOutranksBadMediaType) {
    EXPECT_THROW(StreamContent(nullptr, "a\r\nb"), System::ArgumentNullException);
}

TEST(HttpControlCharacterTests, MultipartSubtype_ControlCharacter_ThrowsArgumentException) {
    for (const auto& bad : controlBearingFields()) {
        // A subtype that is only whitespace is rejected by the pre-existing
        // ThrowIfNullOrWhiteSpace check, which is also an ArgumentException.
        EXPECT_THROW(MultipartContent("mixed" + bad, "B"), System::ArgumentException);
    }
}

TEST(HttpControlCharacterTests, MultipartFormDataNameAndFileName_ControlCharacter_ThrowsArgumentException) {
    for (const auto& bad : controlBearingFields()) {
        MultipartFormDataContent content("B");
        EXPECT_THROW(content.Add(std::make_shared<StringContent>("v"), "n" + bad),
                     System::ArgumentException);
        EXPECT_THROW(content.Add(std::make_shared<StringContent>("v"), "n", "f" + bad),
                     System::ArgumentException);
        EXPECT_TRUE(content.getContentsProperty().empty())
            << "a rejected part must not have been added";
    }
}

// End-to-end: the whole point of the four doors is that nothing reaches the
// wire. A URI carrying a CRLF must fail BEFORE a socket is opened, so the
// process's descriptor count cannot move -- the same instrument #2065
// established, and a stronger statement than "an exception was thrown".
TEST(HttpControlCharacterTests, InjectedRequestUriNeverOpensASocket) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";

    HttpClient client;
    // Port 1 is not listening; if the guard ever stopped working the connect
    // attempt itself would be visible in the descriptor count.
    const std::string injected = "http://127.0.0.1:1/pa\r\nX-Injected: yes";

    const SharpRuntime::intcs before = openDescriptorCount();
    int threw = 0;
    for (int i = 0; i < 20; ++i) {
        try {
            (void)client.Get(injected);
        } catch (const System::UriFormatException&) {
            ++threw;
        }
    }
    const SharpRuntime::intcs after = openDescriptorCount();

    EXPECT_EQ(20, threw) << "every attempt must be rejected at the parser";
    EXPECT_EQ(0, after - before) << "no socket may be opened for a rejected request URI";
}

// A malformed RESPONSE stays a response error: the handler rejects a
// control-bearing header line itself rather than letting
// HttpResponseMessage::setHeader raise System::FormatException at it.
TEST(HttpControlCharacterTests, ResponseHeaderLineWithControlCharacter_ThrowsHttpRequestException) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::thread serverThread([&]() {
        try {
            auto server = listener->Accept();
            std::vector<SharpRuntime::bytecs> reqBuf(4096);
            server->Receive(reqBuf);
            // A NUL inside a header field. recvLine() has already consumed the
            // terminating CRLF, so this is what a malformed field looks like by
            // the time the handler sees it.
            server->Send(toBytes(std::string(
                "HTTP/1.1 200 OK\r\nX-Bad: a\0b\r\nContent-Length: 2\r\n\r\nOK", 52)));
            server->Close();
        } catch (const System::Exception&) {
        }
    });

    HttpClient client;
    EXPECT_THROW(client.GetString("http://127.0.0.1:" + std::to_string(port) + "/"),
                 HttpRequestException);

    serverThread.join();
    listener->Close();
}

// A caller-set header still reaches the wire unchanged when it is legal --
// the repair must not have broken the ordinary path.
TEST(HttpControlCharacterTests, LegalCallerHeaderStillReachesTheWire) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::string raw;
    std::thread serverThread([&]() {
        try {
            auto server = listener->Accept();
            std::vector<SharpRuntime::bytecs> reqBuf(4096);
            SharpRuntime::intcs n = server->Receive(reqBuf);
            raw.assign(reqBuf.begin(), reqBuf.begin() + n);
            server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
            server->Close();
        } catch (const System::Exception&) {
        }
    });

    HttpClient client;
    auto request = std::make_shared<HttpRequestMessage>(
        HttpMethod::Get(), "http://127.0.0.1:" + std::to_string(port) + "/");
    request->setHeader("X-Legal", "a b\tc");
    // The join must happen whatever Send() does: an unjoined std::thread
    // terminates the whole executable, which would hide every other result
    // behind one abort instead of reporting one failing test.
    std::shared_ptr<HttpResponseMessage> response;
    std::string threwWhat;
    try { response = client.Send(request); }
    catch (const System::Exception& e) { threwWhat = e.getMessageProperty(); }
    if (!threwWhat.empty()) pokeMockServer(port);
    serverThread.join();
    listener->Close();

    ASSERT_EQ(threwWhat, "");
    EXPECT_EQ(response->getStatusCodeProperty(), HttpStatusCode::OK);
    EXPECT_NE(raw.find("X-Legal: a b\tc\r\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Ticket #2063's disclosure suite -- every behaviour a BLOCKED or DEFERRED
// ticket of this namespace would change is pinned here, so no future approved
// option can land silently. These tests assert what the port does TODAY; each
// one is expected to be edited, deliberately and visibly, by the ticket named
// in its comment. None of them asserts that the current behaviour is correct.
// ---------------------------------------------------------------------------

namespace {

// #2066's repair is an OWNERSHIP change, and the defect it repairs is a
// use-after-free. A use-after-free is not a behaviour to pin -- it is undefined
// behaviour, and a test that exercised it would be a test with no defined
// meaning. What IS pinnable, and what #2066 must change, is the ownership MODEL:
// HttpClient does not participate in shared ownership today, so its async
// members have nothing to keep alive.
static_assert(!std::is_base_of_v<std::enable_shared_from_this<HttpClient>, HttpClient>,
              "#2066 would make HttpClient participate in shared ownership "
              "(enable_shared_from_this) -- SR-AUD-310, CCF-019, NOT APPROVED");

// Object layout: expressed against a probe struct with the same declared
// members rather than a literal byte count, so the pin is exact on every
// standard library rather than only on the one this baseline uses. Measured on
// the verified Linux/GCC/libstdc++ baseline: HttpClient 104,
// HttpRequestMessage 192, HttpResponseMessage 112, StringContent 104,
// ByteArrayContent 64, MultipartContent 96, HttpMethod 32.
struct HttpRequestMessageLayoutProbe {
    HttpMethod                                   method;
    std::string                                  uri;
    std::shared_ptr<HttpContent>                 content;
    std::unordered_map<std::string, std::string> headers;
    HttpRequestOptions                           options;
};
struct HttpResponseMessageLayoutProbe {
    System::Net::HttpStatusCode                  statusCode;
    std::string                                  reasonPhrase;
    std::shared_ptr<HttpContent>                 content;
    std::unordered_map<std::string, std::string> headers;
};

} // namespace

static_assert(sizeof(HttpRequestMessage) == sizeof(HttpRequestMessageLayoutProbe),
              "#2067's sent-state flag would grow HttpRequestMessage -- SR-AUD-314, "
              "OBJECT LAYOUT CHANGE, NOT APPROVED");
static_assert(sizeof(HttpResponseMessage) == sizeof(HttpResponseMessageLayoutProbe),
              "#2068/#2069 must not add state to HttpResponseMessage -- SR-AUD-315/316, "
              "NOT APPROVED");

// The comparator of the returned map is PUBLIC SURFACE. #2062's review concluded from that
// that #2068 "cannot make the lookup case-insensitive without changing this type, which is why
// it is blocked rather than merely unimplemented".
//
// #2068 LANDED WITHOUT CHANGING IT, so these assertions now say something stronger than they
// did: the case-insensitive rule was implemented as a case-insensitive LOOKUP over an unchanged
// map, not as a changed map type. The collections hold a handful of entries, so a linear scan
// costs nothing measurable -- and it costs nothing of SA-2's machinery either. They stay as
// pins: swapping the comparator in later is still a public source break.
static_assert(std::is_same_v<decltype(std::declval<const HttpRequestMessage&>().getHeadersProperty()),
                             const std::unordered_map<std::string, std::string>&>,
              "changing the PUBLIC returned map type is a source break -- #2068 deliberately "
              "did not need to");
static_assert(std::is_same_v<decltype(std::declval<const HttpResponseMessage&>().getHeadersProperty()),
                             const std::unordered_map<std::string, std::string>&>,
              "changing the PUBLIC returned map type is a source break -- #2068 deliberately "
              "did not need to");

TEST(NetHttpGatedBehaviourPins, LayoutAndOwnershipModelAreStaticallyAsserted) {
    // The static_asserts above are the test; this keeps them visible in the
    // suite and records the measured baseline numbers in the run output.
    EXPECT_EQ(sizeof(HttpRequestMessage), sizeof(HttpRequestMessageLayoutProbe));
    EXPECT_EQ(sizeof(HttpResponseMessage), sizeof(HttpResponseMessageLayoutProbe));
}

// #2067 (SR-AUD-314) -- .NET throws InvalidOperationException when one
// HttpRequestMessage is sent twice. This port has no sent state.
TEST(NetHttpGatedBehaviourPins, Pin2067_OneRequestMessageCanStillBeSentTwice) {
    auto handler = std::make_shared<RecordingHandler>();
    HttpClient client(handler);
    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");

    EXPECT_NO_THROW((void)client.Send(request));
    EXPECT_NO_THROW((void)client.Send(request));
    EXPECT_EQ(handler->receivedRequest, request)
        << "the SAME message object reaches the handler a second time";
}

// #2068 (SR-AUD-315) LANDED -- field names are compared case-insensitively, as RFC 9110 5.1
// requires and as .NET does everywhere (HeaderDescriptor is built on OrdinalIgnoreCase).
TEST(NetHttpGatedBehaviourPins, Fix2068_HeaderNamesAreCaseInsensitive) {
    HttpRequestMessage request;
    request.setHeader("Content-Type", "a");
    request.setHeader("content-type", "b");
    EXPECT_EQ(request.getHeadersProperty().size(), 1u)
        << "two spellings of one field are ONE header";
    EXPECT_EQ(request.getHeader("CONTENT-TYPE"), "b") << "last write wins, and lookup is blind to case";

    HttpResponseMessage response;
    response.setHeader("Content-Type", "a");
    EXPECT_EQ(response.getHeader("content-type"), "a");
    EXPECT_EQ(response.getHeader("Content-Type"), "a");
    EXPECT_EQ(response.getHeader("CoNtEnT-tYpE"), "a");
    EXPECT_EQ(response.getHeader("Content-Typ"), "") << "a genuinely different name still misses";

    // The CALLER's spelling is what is stored, matching .NET, which keeps the string it was
    // given rather than canonicalising it.
    EXPECT_TRUE(request.getHeadersProperty().count("content-type") == 1);
    EXPECT_TRUE(request.getHeadersProperty().count("Content-Type") == 0);
}

TEST(NetHttpGatedBehaviourPins, Fix2068_ClientDefaultHeadersAreCaseInsensitiveToo) {
    // The merge in HttpClient::Send tested "did the caller already set it" with a byte-exact
    // find(), so a default named `Accept` was merged on top of a request already carrying
    // `accept` and both went on the wire.
    auto handler = std::make_shared<RecordingHandler>();
    HttpClient client(handler);
    client.setDefaultHeader("Accept", "application/json");
    EXPECT_EQ(client.getDefaultHeader("accept"), "application/json");

    auto request = std::make_shared<HttpRequestMessage>(HttpMethod::Get(), "http://example.com/");
    request->setHeader("accept", "text/plain");
    (void)client.Send(request);
    EXPECT_EQ(handler->receivedRequest->getHeadersProperty().size(), 1u)
        << "the client default must not be merged on top of the caller's own spelling";
    EXPECT_EQ(handler->receivedRequest->getHeader("Accept"), "text/plain");
}

// #2068's second half, which SR-AUD-315 does not name and #2062's review measured: the handler
// wrote Host/User-Agent/Accept/Connection UNCONDITIONALLY, before the caller's map, so a
// caller-set Host went on the wire TWICE -- request-smuggling-adjacent, because two
// intermediaries may pick different ones. Each is now a default, emitted only when the caller
// supplied nothing with that name.
TEST(NetHttpGatedBehaviourPins, Fix2068_TheHandlersDefaultFieldsYieldToTheCallers) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::string raw;
    std::thread serverThread([&]() {
        try {
            auto server = listener->Accept();
            std::vector<SharpRuntime::bytecs> reqBuf(4096);
            SharpRuntime::intcs n = server->Receive(reqBuf);
            raw.assign(reqBuf.begin(), reqBuf.begin() + n);
            server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
            server->Close();
        } catch (const System::Exception&) {
        }
    });

    HttpClient client;
    auto request = std::make_shared<HttpRequestMessage>(
        HttpMethod::Get(), "http://127.0.0.1:" + std::to_string(port) + "/");
    request->setHeader("Host", "caller.example");
    std::string threwWhat;
    try { (void)client.Send(request); }
    catch (const System::Exception& e) { threwWhat = e.getMessageProperty(); }
    if (!threwWhat.empty()) pokeMockServer(port);
    serverThread.join();
    listener->Close();
    ASSERT_EQ(threwWhat, "");

    size_t hosts = 0;
    for (size_t at = raw.find("Host: "); at != std::string::npos; at = raw.find("Host: ", at + 1))
        ++hosts;
    EXPECT_EQ(hosts, 1u) << "exactly one Host field may reach the wire";
    EXPECT_NE(raw.find("Host: caller.example"), std::string::npos)
        << "and it must be the caller's, not the handler's";
}

// #2069 (SR-AUD-316's status-code half) -- the constructor accepts any number.
TEST(NetHttpGatedBehaviourPins, Pin2069_ResponseAcceptsAnyStatusNumber) {
    for (int code : {-1, 0, 1000, 99999}) {
        HttpResponseMessage response(static_cast<HttpStatusCode>(code));
        EXPECT_EQ(static_cast<int>(response.getStatusCodeProperty()), code);
        EXPECT_FALSE(response.getIsSuccessStatusCodeProperty());
        EXPECT_THROW(response.EnsureSuccessStatusCode(), HttpRequestException);
    }
}

// #2070 (SR-AUD-317) -- the charset is a label; the bytes are always the
// storage bytes. Whether .NET encodes through the label is unverified.
TEST(NetHttpGatedBehaviourPins, Pin2070_StringContentEmitsStorageBytesUnderAnyCharsetLabel) {
    StringContent content("\xc3\xa9", "utf-16", "text/plain");
    auto bytes = content.ReadAsByteArray();
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(static_cast<unsigned>(bytes[0]), 0xc3u);
    EXPECT_EQ(static_cast<unsigned>(bytes[1]), 0xa9u);
    EXPECT_EQ(content.getCharSetProperty(), "utf-16");
}

// #2071 (SR-AUD-318's limits half) -- response reads are unbounded. This pins
// that no maximum is applied at a modest size; it deliberately does NOT try to
// exhaust memory.
TEST(NetHttpGatedBehaviourPins, Pin2071_ResponseReadsAreUnbounded) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    const size_t bodySize = 256u * 1024u;
    std::thread serverThread([&]() {
        try {
            auto server = listener->Accept();
            std::vector<SharpRuntime::bytecs> reqBuf(4096);
            server->Receive(reqBuf);
            server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: " +
                                 std::to_string(bodySize) + "\r\n\r\n" +
                                 std::string(bodySize, 'x')));
            server->Close();
        } catch (const System::Exception&) {
        }
    });

    HttpClient client;
    std::string body;
    std::string threwWhat;
    // The join must happen whatever the client does. Without this, an approved
    // #2071 that started rejecting an over-large response would leave the server
    // thread unjoined and std::terminate the whole executable, hiding every
    // other result behind one abort instead of reporting one failing pin.
    try {
        body = client.GetString("http://127.0.0.1:" + std::to_string(port) + "/");
    } catch (const System::Exception& e) {
        threwWhat = e.getMessageProperty();
    }
    if (!threwWhat.empty()) pokeMockServer(port);
    serverThread.join();
    listener->Close();

    EXPECT_EQ(threwWhat, "") << "no response-size limit exists to reject this today";
    EXPECT_EQ(body.size(), bodySize)
        << "no maximum response size is applied -- .NET's MaxResponseContentBufferSize "
           "has no equivalent here (#2071, NOT APPROVED)";
}

// ---------------------------------------------------------------------------
// Ticket #2064 -- parseUrl and parseStatusLine must consume the WHOLE field and
// apply real domain checks (SR-AUD-311 + SR-AUD-312, cause NH-A / CCF-002;
// docs/SystemNetHttpNamespaceReviewPlan.md §4.2, §4.3 and §20.6).
//
// std::sto* stops at the first character it cannot use and reports SUCCESS for
// the prefix it consumed, and it additionally skips leading whitespace and
// accepts a sign. Nothing checked that the whole field had been consumed.
// Measured before the repair: port "80abc" -> 80, "-1" -> -1, "99999" -> 99999,
// " 80" -> 80, "+80" -> 80; status "200trailer" -> 200, "2" -> 2, "-5" -> -5,
// "99999" -> 99999; and the version token was never examined AT ALL, so
// "GARBAGE 200 OK" -> 200.
//
// Two of these are VALUE changes rather than rejections, and they are the more
// dangerous half: "http://host?q=1" parsed to host "host?q=1" (so the query
// reached DNS and the Host header while the request line asked for "/"), and
// the host was never lowercased while the scheme already was.
// ---------------------------------------------------------------------------

TEST(HttpClientUrlParseTests, PortDomain_WholeFieldAndRange) {
    EXPECT_EQ(HttpClient::parseUrl("http://host:0/p").port, 0);
    EXPECT_EQ(HttpClient::parseUrl("http://host:80/p").port, 80);
    EXPECT_EQ(HttpClient::parseUrl("http://host:8080/p").port, 8080);
    EXPECT_EQ(HttpClient::parseUrl("http://host:65535/p").port, 65535);

    for (const char* bad : {"http://host:65536/p",     // one past the domain
                            "http://host:99999/p",
                            "http://host:2147483648/p",
                            "http://host:80abc",       // valid prefix, invalid tail
                            "http://host:-1/p",        // sign
                            "http://host:+80/p",       // sign
                            "http://host: 80/p",       // leading space std::stoi skipped
                            "http://host:/p",          // empty port text
                            "http://host:8 0/p"}) {
        EXPECT_THROW(HttpClient::parseUrl(bad), System::UriFormatException) << bad;
    }
}

TEST(HttpClientUrlParseTests, PortDomain_AppliesToIPv6LiteralsToo) {
    EXPECT_EQ(HttpClient::parseUrl("http://[::1]:65535/p").port, 65535);
    EXPECT_THROW(HttpClient::parseUrl("http://[::1]:70000"), System::UriFormatException);
    EXPECT_THROW(HttpClient::parseUrl("http://[::1]:80abc"), System::UriFormatException);
}

// The worst row of §4.2: a query string became part of the DNS name AND of the
// Host header, while the request line asked for "/".
TEST(HttpClientUrlParseTests, AuthorityEndsAtTheFirstSlashQuestionOrHash) {
    auto query = HttpClient::parseUrl("http://host?q=1");
    EXPECT_EQ(query.host, "host");
    EXPECT_EQ(query.path, "/?q=1");

    auto secret = HttpClient::parseUrl("http://api.example?key=secret");
    EXPECT_EQ(secret.host, "api.example") << "the query must not reach DNS or the Host header";
    EXPECT_EQ(secret.path, "/?key=secret");

    auto fragment = HttpClient::parseUrl("http://host#frag");
    EXPECT_EQ(fragment.host, "host");
    EXPECT_EQ(fragment.path, "/");

    auto both = HttpClient::parseUrl("http://host:8080?q=1");
    EXPECT_EQ(both.host, "host");
    EXPECT_EQ(both.port, 8080);
    EXPECT_EQ(both.path, "/?q=1");
}

// A fragment is client-side only (RFC 9110 §7.1) and is never part of a request
// target, so it is dropped rather than written into the request line.
TEST(HttpClientUrlParseTests, FragmentIsNeverPartOfTheRequestTarget) {
    EXPECT_EQ(HttpClient::parseUrl("http://host/p#frag").path, "/p");
    EXPECT_EQ(HttpClient::parseUrl("http://host/p?q=1#frag").path, "/p?q=1");
    EXPECT_EQ(HttpClient::parseUrl("http://host/#frag").path, "/");
    EXPECT_EQ(HttpClient::parseUrl("http://host#frag").path, "/");
}

// A query that is part of the path is untouched -- this is the behaviour the
// pre-existing QueryStringPreserved test pins, and #2064 must not change it.
TEST(HttpClientUrlParseTests, QueryAfterAPathIsStillPartOfTheRequestTarget) {
    EXPECT_EQ(HttpClient::parseUrl("http://example.com/search?q=hello").path, "/search?q=hello");
    EXPECT_EQ(HttpClient::parseUrl("http://example.com/search?q=hello").host, "example.com");
}

TEST(HttpClientUrlParseTests, HostIsLowercasedLikeTheSchemeAlreadyWas) {
    EXPECT_EQ(HttpClient::parseUrl("http://HOST.EXAMPLE/p").host, "host.example");
    EXPECT_EQ(HttpClient::parseUrl("HTTP://HOST.EXAMPLE/p").scheme, "http");
    EXPECT_EQ(HttpClient::parseUrl("http://[2001:DB8::1]/p").host, "2001:db8::1");
    // The path is case-SENSITIVE and must not be touched.
    EXPECT_EQ(HttpClient::parseUrl("http://HOST/Path/Mixed?Q=V").path, "/Path/Mixed?Q=V");
}

TEST(HttpClientStatusLineParseTests, VersionTokenMustBeHttpDigitDotDigit) {
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 200 OK").statusCode, 200);
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.0 204").statusCode, 204);

    for (const char* bad : {"GARBAGE 200 OK",   // never examined before #2064
                            "HTTP/11 200 OK",
                            "HTTP/1.1x 200 OK",
                            "HTTP/1.10 200 OK",
                            "HTTP 200 OK",
                            "http/1.1 200 OK",  // the token is case-sensitive
                            "/1.1 200 OK"}) {
        EXPECT_THROW(HttpClient::parseStatusLine(bad), HttpRequestException) << bad;
    }
}

// PINNED CHOICE, not a verified match: HTTP/9.9 satisfies the grammar, so a
// version this port does not speak is reported rather than treated as a parse
// error. .NET's behaviour is unknown here (/rv/tmp/runtime/ absent).
TEST(HttpClientStatusLineParseTests, UnknownButWellFormedVersionIsAccepted) {
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/9.9 200 OK").statusCode, 200);
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/0.9 200 OK").statusCode, 200);
}

TEST(HttpClientStatusLineParseTests, StatusCodeMustBeExactlyThreeDigits) {
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 100 Continue").statusCode, 100);
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 999 Weird").statusCode, 999);

    for (const char* bad : {"HTTP/1.1 200trailer OK",
                            "HTTP/1.1 2 OK",
                            "HTTP/1.1 20 OK",
                            "HTTP/1.1 2000 OK",
                            "HTTP/1.1 99999 OK",
                            "HTTP/1.1 -5 OK",
                            "HTTP/1.1 +5 OK",
                            "HTTP/1.1 XYZ OK",
                            "HTTP/1.1  200 OK",   // double space -- unchanged, still rejected
                            "HTTP/1.1 "}) {
        EXPECT_THROW(HttpClient::parseStatusLine(bad), HttpRequestException) << bad;
    }
}

// PINNED CHOICE, not a verified match: "099" is exactly three digits, which is
// all RFC 9112 §4's grammar asks for, so it is accepted as the code 99. The
// review's §11 requires this row to be pinned whichever way it is decided.
TEST(HttpClientStatusLineParseTests, LeadingZeroThreeDigitCodeIsAcceptedAsThatNumber) {
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 099 OK").statusCode, 99);
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 000 Zero").statusCode, 0);
    EXPECT_EQ(HttpClient::parseStatusLine("HTTP/1.1 007 Bond").reason, "Bond");
}

// A negative code used to be cast into System::Net::HttpStatusCode, a public
// enum, giving it a value no enumerator names.
TEST(HttpClientStatusLineParseTests, NoStatusCodeCanEscapeTheEnumDomain) {
    for (const char* line : {"HTTP/1.1 000 a", "HTTP/1.1 999 a", "HTTP/1.1 200 a"}) {
        auto parsed = HttpClient::parseStatusLine(line);
        EXPECT_GE(parsed.statusCode, 0);
        EXPECT_LE(parsed.statusCode, 999);
    }
}

// ---------------------------------------------------------------------------
// #2063 x #2064 interaction -- both repairs touch parseUrl, and neither may
// undo the other.
// ---------------------------------------------------------------------------

TEST(HttpParserInteractionTests, ControlCharacterIsRejectedBeforeAnySplitting) {
    // A URL that is BOTH control-bearing and domain-invalid must report the
    // control character: #2063's check runs on the whole string before #2064's
    // authority split can look at anything.
    EXPECT_THROW(HttpClient::parseUrl("http://ho\r\nst:99999/p"), System::UriFormatException);
    EXPECT_THROW(HttpClient::parseUrl("http://host:9\r\n9999?q=1"), System::UriFormatException);
    try {
        HttpClient::parseUrl("http://ho\r\nst:99999/p");
        FAIL() << "expected a rejection";
    } catch (const System::UriFormatException& e) {
        EXPECT_NE(e.getMessageProperty().find("carriage return"), std::string::npos)
            << "#2064's port check must not have shadowed #2063's control-character check";
    }
}

TEST(HttpParserInteractionTests, TheNewAuthoritySplitCannotReintroduceAControlCharacter) {
    // #2064 regroups the authority, query and fragment. Because #2063 validates
    // the WHOLE url string first, no regrouping can produce a component holding
    // a control character -- checked here for each split boundary.
    for (const char* bad : {"http://host\r\n?q=1", "http://host?q=\r\n1", "http://host#\r\n",
                            "http://host/p\r\n?q=1", "http://host/p?q=1#\r\n"}) {
        EXPECT_THROW(HttpClient::parseUrl(bad), System::UriFormatException) << bad;
    }
    // And a legal URL exercising every one of those boundaries still parses.
    auto parsed = HttpClient::parseUrl("http://HOST.example:8080/a/b?q=1#frag");
    EXPECT_EQ(parsed.host, "host.example");
    EXPECT_EQ(parsed.port, 8080);
    EXPECT_EQ(parsed.path, "/a/b?q=1");
}

TEST(HttpParserInteractionTests, StatusLineKeepsBothTheControlCheckAndTheGrammar) {
    // #2063's control check first...
    EXPECT_THROW(HttpClient::parseStatusLine(std::string("HTTP/1.1 200 O\0K", 16)),
                 HttpRequestException);
    // ...#2064's grammar second, and the two together on one line.
    EXPECT_THROW(HttpClient::parseStatusLine("GARBAGE 2 OK"), HttpRequestException);
    EXPECT_THROW(HttpClient::parseStatusLine(std::string("GARBAGE 2 O\0K", 13)),
                 HttpRequestException);
    // A well-formed line is untouched by either.
    auto parsed = HttpClient::parseStatusLine("HTTP/1.1 404 Not Found");
    EXPECT_EQ(parsed.statusCode, 404);
    EXPECT_EQ(parsed.reason, "Not Found");
}

// End-to-end: a server whose status line is not HTTP at all is now rejected,
// and rejecting it must not reintroduce the #2065 descriptor leak.
TEST(HttpParserInteractionTests, NonHttpStatusLineIsRejectedAndLeaksNoDescriptor) {
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";
    int threw = 0;
    expectNoDescriptorLeak(
        descriptorDeltaOver("GARBAGE 200 OK\r\nContent-Length: 2\r\n\r\nOK", 20, threw),
        "NonHttpStatusLine");
    EXPECT_EQ(19, threw) << "the version token must be examined -- this used to parse as 200";
}

// End-to-end: an authority-only query now reaches the wire as the request
// target instead of as part of the Host header.
TEST(HttpParserInteractionTests, AuthorityOnlyQueryReachesTheWireAsTheRequestTarget) {
    std::shared_ptr<System::Net::Sockets::Socket> listener;
    SharpRuntime::intcs port = startMockHttpServer(listener);

    std::string raw;
    std::thread serverThread([&]() {
        try {
            auto server = listener->Accept();
            std::vector<SharpRuntime::bytecs> reqBuf(4096);
            SharpRuntime::intcs n = server->Receive(reqBuf);
            raw.assign(reqBuf.begin(), reqBuf.begin() + n);
            server->Send(toBytes("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK"));
            server->Close();
        } catch (const System::Exception&) {
        }
    });

    HttpClient client;
    std::string body;
    std::string threwWhat;
    try { body = client.GetString("http://127.0.0.1:" + std::to_string(port) + "?key=secret"); }
    catch (const System::Exception& e) { threwWhat = e.getMessageProperty(); }
    if (!threwWhat.empty()) pokeMockServer(port);
    serverThread.join();
    listener->Close();

    ASSERT_EQ(threwWhat, "") << "an authority-only query must still reach the server";
    EXPECT_EQ(body, "OK");
    EXPECT_NE(raw.find("GET /?key=secret HTTP/1.1\r\n"), std::string::npos)
        << "the query belongs in the request target";
    EXPECT_EQ(raw.find("key=secret\r\n"), std::string::npos)
        << "and must not appear as the tail of the Host header";
}
