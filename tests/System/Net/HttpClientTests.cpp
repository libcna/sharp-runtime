// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include <thread>
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
#include "System/IO/IOException.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/NotSupportedException.hpp"
#include "System/UriFormatException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"

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

TEST(FormUrlEncodedContentTests, ReadAsByteArrayMatchesString) {
    FormUrlEncodedContent c({{"k", "v"}});
    std::string s = c.ReadAsString();
    auto bytes = c.ReadAsByteArray();
    std::string fromBytes(bytes.begin(), bytes.end());
    EXPECT_EQ(s, fromBytes);
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
