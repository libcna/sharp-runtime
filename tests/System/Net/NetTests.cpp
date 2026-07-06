// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include "System/Net/WebUtility.hpp"
#include "System/Net/DecompressionMethods.hpp"
#include "System/Net/HttpRequestHeader.hpp"
#include "System/Net/HttpResponseHeader.hpp"
#include "System/Net/HttpVersion.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::HttpStatusCode;
using System::Net::WebUtility;
using System::Net::DecompressionMethods;
using System::Net::HttpRequestHeader;
using System::Net::HttpResponseHeader;
using System::Net::HttpRequestHeaderGetName;
using System::Net::HttpResponseHeaderGetName;
using System::Net::HttpVersion;

// ===========================================================================
// IPAddress
// ===========================================================================

TEST(IPAddressTests, DefaultConstructor_AddressIsZero) {
    IPAddress ip;
    EXPECT_EQ(ip.getAddressProperty(), 0u);
}

TEST(IPAddressTests, Uint32Constructor_StoresAddress) {
    IPAddress ip(0x7F000001u);
    EXPECT_EQ(ip.getAddressProperty(), 0x7F000001u);
}

TEST(IPAddressTests, Parse_Loopback_ToString) {
    IPAddress ip = IPAddress::Parse("127.0.0.1");
    EXPECT_EQ(ip.ToString(), "127.0.0.1");
}

TEST(IPAddressTests, Parse_AllZeros) {
    IPAddress ip = IPAddress::Parse("0.0.0.0");
    EXPECT_EQ(ip.ToString(), "0.0.0.0");
}

TEST(IPAddressTests, Parse_Broadcast) {
    IPAddress ip = IPAddress::Parse("255.255.255.255");
    EXPECT_EQ(ip.ToString(), "255.255.255.255");
}

TEST(IPAddressTests, Parse_ArbitraryAddress) {
    IPAddress ip = IPAddress::Parse("192.168.1.100");
    EXPECT_EQ(ip.ToString(), "192.168.1.100");
}

TEST(IPAddressTests, Parse_InvalidAddress_Throws) {
    EXPECT_THROW(IPAddress::Parse("not.an.ip"), std::invalid_argument);
}

TEST(IPAddressTests, Parse_IncompleteOctets_Throws) {
    EXPECT_THROW(IPAddress::Parse("192.168.1"), std::invalid_argument);
}

TEST(IPAddressTests, Equality_SameAddress_True) {
    IPAddress a = IPAddress::Parse("10.0.0.1");
    IPAddress b = IPAddress::Parse("10.0.0.1");
    EXPECT_TRUE(a == b);
}

TEST(IPAddressTests, Equality_DifferentAddress_False) {
    IPAddress a = IPAddress::Parse("10.0.0.1");
    IPAddress b = IPAddress::Parse("10.0.0.2");
    EXPECT_FALSE(a == b);
}

TEST(IPAddressTests, Inequality_DifferentAddress_True) {
    IPAddress a = IPAddress::Parse("1.2.3.4");
    IPAddress b = IPAddress::Parse("4.3.2.1");
    EXPECT_TRUE(a != b);
}

TEST(IPAddressTests, StaticLoopback_Is127_0_0_1) {
    EXPECT_EQ(IPAddress::Loopback.ToString(), "127.0.0.1");
}

TEST(IPAddressTests, StaticAny_Is0_0_0_0) {
    EXPECT_EQ(IPAddress::Any.ToString(), "0.0.0.0");
}

TEST(IPAddressTests, StaticBroadcast_Is255_255_255_255) {
    EXPECT_EQ(IPAddress::Broadcast.ToString(), "255.255.255.255");
}

TEST(IPAddressTests, StaticNone_SameAsBroadcast) {
    EXPECT_EQ(IPAddress::None, IPAddress::Broadcast);
}

TEST(IPAddressTests, Parse_RoundTrip_PreservesAddress) {
    std::string addr = "172.16.254.1";
    EXPECT_EQ(IPAddress::Parse(addr).ToString(), addr);
}

TEST(IPAddressTests, AddressFamily_IPv4) {
    EXPECT_EQ(IPAddress::Loopback.getAddressFamilyProperty(), System::Net::Sockets::AddressFamily::InterNetwork);
}

TEST(IPAddressTests, TryParse_Invalid_ReturnsFalse) {
    IPAddress result;
    EXPECT_FALSE(IPAddress::TryParse("not an ip", result));
}

TEST(IPAddressTests, TryParse_Valid_ReturnsTrue) {
    IPAddress result;
    EXPECT_TRUE(IPAddress::TryParse("8.8.8.8", result));
    EXPECT_EQ(result.ToString(), "8.8.8.8");
}

TEST(IPAddressTests, GetAddressBytes_IPv4) {
    IPAddress ip = IPAddress::Parse("192.168.1.100");
    auto bytes = ip.GetAddressBytes();
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 192);
    EXPECT_EQ(bytes[1], 168);
    EXPECT_EQ(bytes[2], 1);
    EXPECT_EQ(bytes[3], 100);
}

TEST(IPAddressTests, IsLoopback_IPv4) {
    EXPECT_TRUE(IPAddress::IsLoopback(IPAddress::Parse("127.0.0.1")));
    EXPECT_TRUE(IPAddress::IsLoopback(IPAddress::Parse("127.5.5.5")));
    EXPECT_FALSE(IPAddress::IsLoopback(IPAddress::Parse("10.0.0.1")));
}

// ===========================================================================
// IPAddress — IPv6
// ===========================================================================

TEST(IPAddressIPv6Tests, Parse_FullForm) {
    IPAddress ip = IPAddress::Parse("2001:0db8:0000:0000:0000:ff00:0042:8329");
    EXPECT_TRUE(ip.getIsIPv6Property());
    EXPECT_EQ(ip.getAddressFamilyProperty(), System::Net::Sockets::AddressFamily::InterNetworkV6);
}

TEST(IPAddressIPv6Tests, Parse_CompressedForm_RoundTrips) {
    IPAddress ip = IPAddress::Parse("2001:db8::ff00:42:8329");
    EXPECT_EQ(ip.ToString(), "2001:db8::ff00:42:8329");
}

TEST(IPAddressIPv6Tests, Parse_Loopback) {
    IPAddress ip = IPAddress::Parse("::1");
    EXPECT_EQ(ip, IPAddress::IPv6Loopback);
    EXPECT_EQ(ip.ToString(), "::1");
}

TEST(IPAddressIPv6Tests, Parse_Any) {
    IPAddress ip = IPAddress::Parse("::");
    EXPECT_EQ(ip, IPAddress::IPv6Any);
    EXPECT_EQ(ip.ToString(), "::");
}

TEST(IPAddressIPv6Tests, Parse_TrailingDoubleColon) {
    IPAddress ip = IPAddress::Parse("2001:db8::");
    auto bytes = ip.GetAddressBytes();
    ASSERT_EQ(bytes.size(), 16u);
    EXPECT_EQ(bytes[0], 0x20);
    EXPECT_EQ(bytes[1], 0x01);
}

TEST(IPAddressIPv6Tests, Parse_WithScopeId) {
    IPAddress ip = IPAddress::Parse("fe80::1%3");
    EXPECT_EQ(ip.getScopeIdProperty(), 3);
}

TEST(IPAddressIPv6Tests, Parse_EmbeddedIPv4) {
    IPAddress ip = IPAddress::Parse("::ffff:192.168.1.1");
    EXPECT_TRUE(ip.getIsIPv4MappedToIPv6Property());
    EXPECT_EQ(ip.ToString(), "::ffff:192.168.1.1");
}

TEST(IPAddressIPv6Tests, Parse_Invalid_Throws) {
    EXPECT_THROW(IPAddress::Parse("garbage:::too:many:colons"), std::invalid_argument);
}

TEST(IPAddressIPv6Tests, GetAddressProperty_Throws) {
    IPAddress ip = IPAddress::Parse("::1");
    EXPECT_THROW((void)ip.getAddressProperty(), std::logic_error);
}

TEST(IPAddressIPv6Tests, GetScopeIdProperty_OnIPv4_Throws) {
    EXPECT_THROW((void)IPAddress::Loopback.getScopeIdProperty(), std::logic_error);
}

TEST(IPAddressIPv6Tests, MapToIPv6_ThenMapToIPv4_RoundTrips) {
    IPAddress v4 = IPAddress::Parse("192.168.1.1");
    IPAddress v6 = v4.MapToIPv6();
    EXPECT_TRUE(v6.getIsIPv4MappedToIPv6Property());
    IPAddress back = v6.MapToIPv4();
    EXPECT_EQ(back, v4);
}

TEST(IPAddressIPv6Tests, IsLoopback_IPv6) {
    EXPECT_TRUE(IPAddress::IsLoopback(IPAddress::IPv6Loopback));
    EXPECT_FALSE(IPAddress::IsLoopback(IPAddress::Parse("2001:db8::1")));
}

TEST(IPAddressIPv6Tests, IsIPv6LinkLocal) {
    EXPECT_TRUE(IPAddress::Parse("fe80::1").getIsIPv6LinkLocalProperty());
    EXPECT_FALSE(IPAddress::Parse("2001:db8::1").getIsIPv6LinkLocalProperty());
}

TEST(IPAddressIPv6Tests, Equality) {
    EXPECT_EQ(IPAddress::Parse("2001:db8::1"), IPAddress::Parse("2001:0db8:0000:0000:0000:0000:0000:0001"));
}

// ===========================================================================
// IPEndPoint
// ===========================================================================

TEST(IPEndPointTests, DefaultConstructor_ZeroAddressZeroPort) {
    IPEndPoint ep;
    EXPECT_EQ(ep.getAddressProperty().getAddressProperty(), 0u);
    EXPECT_EQ(ep.getPortProperty(), 0);
}

TEST(IPEndPointTests, Constructor_IPAddressAndPort) {
    IPEndPoint ep(IPAddress::Loopback, 8080);
    EXPECT_EQ(ep.getAddressProperty(), IPAddress::Loopback);
    EXPECT_EQ(ep.getPortProperty(), 8080);
}

TEST(IPEndPointTests, Constructor_Uint32AndPort) {
    IPEndPoint ep(0x7F000001u, 443);
    EXPECT_EQ(ep.getAddressProperty().getAddressProperty(), 0x7F000001u);
    EXPECT_EQ(ep.getPortProperty(), 443);
}

TEST(IPEndPointTests, ToString_LoopbackPort80) {
    IPEndPoint ep(IPAddress::Loopback, 80);
    EXPECT_EQ(ep.ToString(), "127.0.0.1:80");
}

TEST(IPEndPointTests, ToString_AnyPort0) {
    IPEndPoint ep(IPAddress::Any, 0);
    EXPECT_EQ(ep.ToString(), "0.0.0.0:0");
}

TEST(IPEndPointTests, SetAddress_UpdatesAddress) {
    IPEndPoint ep;
    ep.setAddressProperty(IPAddress::Loopback);
    EXPECT_EQ(ep.getAddressProperty(), IPAddress::Loopback);
}

TEST(IPEndPointTests, SetPort_UpdatesPort) {
    IPEndPoint ep;
    ep.setPortProperty(9090);
    EXPECT_EQ(ep.getPortProperty(), 9090);
}

TEST(IPEndPointTests, Equality_SameAddressAndPort_True) {
    IPEndPoint a(IPAddress::Loopback, 80);
    IPEndPoint b(IPAddress::Loopback, 80);
    EXPECT_TRUE(a == b);
}

TEST(IPEndPointTests, Equality_DifferentPort_False) {
    IPEndPoint a(IPAddress::Loopback, 80);
    IPEndPoint b(IPAddress::Loopback, 443);
    EXPECT_FALSE(a == b);
}

TEST(IPEndPointTests, Inequality_DifferentAddress_True) {
    IPEndPoint a(IPAddress::Any, 80);
    IPEndPoint b(IPAddress::Loopback, 80);
    EXPECT_TRUE(a != b);
}

TEST(IPEndPointTests, MinPort_IsZero) {
    EXPECT_EQ(static_cast<int>(IPEndPoint::MinPort), 0);
}

TEST(IPEndPointTests, MaxPort_Is65535) {
    EXPECT_EQ(static_cast<int>(IPEndPoint::MaxPort), 65535);
}

// ===========================================================================
// HttpStatusCode
// ===========================================================================

TEST(HttpStatusCodeTests, OK_Is200) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::OK), 200);
}

TEST(HttpStatusCodeTests, Created_Is201) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Created), 201);
}

TEST(HttpStatusCodeTests, NoContent_Is204) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::NoContent), 204);
}

TEST(HttpStatusCodeTests, MovedPermanently_Is301) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::MovedPermanently), 301);
}

TEST(HttpStatusCodeTests, NotModified_Is304) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::NotModified), 304);
}

TEST(HttpStatusCodeTests, BadRequest_Is400) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::BadRequest), 400);
}

TEST(HttpStatusCodeTests, Unauthorized_Is401) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Unauthorized), 401);
}

TEST(HttpStatusCodeTests, Forbidden_Is403) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Forbidden), 403);
}

TEST(HttpStatusCodeTests, NotFound_Is404) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::NotFound), 404);
}

TEST(HttpStatusCodeTests, InternalServerError_Is500) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::InternalServerError), 500);
}

TEST(HttpStatusCodeTests, NotImplemented_Is501) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::NotImplemented), 501);
}

TEST(HttpStatusCodeTests, ServiceUnavailable_Is503) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::ServiceUnavailable), 503);
}

TEST(HttpStatusCodeTests, Continue_Is100) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Continue), 100);
}

TEST(HttpStatusCodeTests, TooManyRequests_Is429) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::TooManyRequests), 429);
}

TEST(HttpStatusCodeTests, Ambiguous_SameAsMultipleChoices) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Ambiguous),
              static_cast<int>(HttpStatusCode::MultipleChoices));
}

TEST(HttpStatusCodeTests, Moved_SameAsMovedPermanently) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::Moved),
              static_cast<int>(HttpStatusCode::MovedPermanently));
}

TEST(HttpStatusCodeTests, UnprocessableEntity_SameAsUnprocessableContent) {
    EXPECT_EQ(static_cast<int>(HttpStatusCode::UnprocessableEntity),
              static_cast<int>(HttpStatusCode::UnprocessableContent));
}

// ===========================================================================
// WebUtility
// ===========================================================================

TEST(WebUtilityTests, HtmlEncode_Ampersand) {
    EXPECT_EQ(WebUtility::HtmlEncode("a&b"), "a&amp;b");
}

TEST(WebUtilityTests, HtmlEncode_LessThan) {
    EXPECT_EQ(WebUtility::HtmlEncode("<p>"), "&lt;p&gt;");
}

TEST(WebUtilityTests, HtmlEncode_GreaterThan) {
    EXPECT_EQ(WebUtility::HtmlEncode(">"), "&gt;");
}

TEST(WebUtilityTests, HtmlEncode_DoubleQuote) {
    EXPECT_EQ(WebUtility::HtmlEncode("\"hello\""), "&quot;hello&quot;");
}

TEST(WebUtilityTests, HtmlEncode_SingleQuote) {
    EXPECT_EQ(WebUtility::HtmlEncode("it's"), "it&#39;s");
}

TEST(WebUtilityTests, HtmlEncode_NoSpecialChars_Unchanged) {
    EXPECT_EQ(WebUtility::HtmlEncode("hello world"), "hello world");
}

TEST(WebUtilityTests, HtmlEncode_Empty_ReturnsEmpty) {
    EXPECT_EQ(WebUtility::HtmlEncode(""), "");
}

TEST(WebUtilityTests, HtmlDecode_Ampersand) {
    EXPECT_EQ(WebUtility::HtmlDecode("a&amp;b"), "a&b");
}

TEST(WebUtilityTests, HtmlDecode_LtGt) {
    EXPECT_EQ(WebUtility::HtmlDecode("&lt;p&gt;"), "<p>");
}

TEST(WebUtilityTests, HtmlDecode_Quot) {
    EXPECT_EQ(WebUtility::HtmlDecode("&quot;hi&quot;"), "\"hi\"");
}

TEST(WebUtilityTests, HtmlDecode_Hash39) {
    EXPECT_EQ(WebUtility::HtmlDecode("it&#39;s"), "it's");
}

TEST(WebUtilityTests, HtmlDecode_NoEntities_Unchanged) {
    EXPECT_EQ(WebUtility::HtmlDecode("plain text"), "plain text");
}

TEST(WebUtilityTests, HtmlEncode_HtmlDecode_RoundTrip) {
    std::string original = "<a href=\"url\">link & more</a>";
    EXPECT_EQ(WebUtility::HtmlDecode(WebUtility::HtmlEncode(original)), original);
}

TEST(WebUtilityTests, UrlEncode_SpaceBecomesPlus) {
    EXPECT_EQ(WebUtility::UrlEncode("hello world"), "hello+world");
}

TEST(WebUtilityTests, UrlEncode_AlphanumericUnchanged) {
    EXPECT_EQ(WebUtility::UrlEncode("abc123"), "abc123");
}

TEST(WebUtilityTests, UrlEncode_SafeCharsUnchanged) {
    EXPECT_EQ(WebUtility::UrlEncode("-_.~"), "-_.~");
}

TEST(WebUtilityTests, UrlEncode_SpecialCharsPercent) {
    std::string encoded = WebUtility::UrlEncode("a=b");
    EXPECT_EQ(encoded, "a%3Db");
}

TEST(WebUtilityTests, UrlEncode_Empty_ReturnsEmpty) {
    EXPECT_EQ(WebUtility::UrlEncode(""), "");
}

TEST(WebUtilityTests, UrlDecode_PlusBecomesSpace) {
    EXPECT_EQ(WebUtility::UrlDecode("hello+world"), "hello world");
}

TEST(WebUtilityTests, UrlDecode_PercentEncoded) {
    EXPECT_EQ(WebUtility::UrlDecode("a%3Db"), "a=b");
}

TEST(WebUtilityTests, UrlDecode_NoEncoding_Unchanged) {
    EXPECT_EQ(WebUtility::UrlDecode("plain"), "plain");
}

TEST(WebUtilityTests, UrlEncode_UrlDecode_RoundTrip) {
    std::string original = "key=hello world&other=1+2";
    EXPECT_EQ(WebUtility::UrlDecode(WebUtility::UrlEncode(original)), original);
}

// ===========================================================================
// DecompressionMethods
// ===========================================================================

TEST(DecompressionMethodsTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(DecompressionMethods::None), 0);
}

TEST(DecompressionMethodsTests, FlagsCombine) {
    auto v = DecompressionMethods::GZip | DecompressionMethods::Deflate;
    EXPECT_EQ(static_cast<int>(v & DecompressionMethods::GZip), static_cast<int>(DecompressionMethods::GZip));
    EXPECT_EQ(static_cast<int>(v & DecompressionMethods::Deflate), static_cast<int>(DecompressionMethods::Deflate));
    EXPECT_EQ(static_cast<int>(v & DecompressionMethods::Brotli), 0);
}

TEST(DecompressionMethodsTests, All_ContainsEveryKnownMethod) {
    auto all = DecompressionMethods::All;
    EXPECT_NE(static_cast<int>(all & DecompressionMethods::GZip), 0);
    EXPECT_NE(static_cast<int>(all & DecompressionMethods::Deflate), 0);
    EXPECT_NE(static_cast<int>(all & DecompressionMethods::Brotli), 0);
    EXPECT_NE(static_cast<int>(all & DecompressionMethods::Zstandard), 0);
}

// ===========================================================================
// HttpRequestHeader / HttpResponseHeader
// ===========================================================================

TEST(HttpRequestHeaderTests, GetName_KnownValues) {
    EXPECT_EQ(HttpRequestHeaderGetName(HttpRequestHeader::CacheControl), "Cache-Control");
    EXPECT_EQ(HttpRequestHeaderGetName(HttpRequestHeader::ContentMd5), "Content-MD5");
    EXPECT_EQ(HttpRequestHeaderGetName(HttpRequestHeader::UserAgent), "User-Agent");
}

TEST(HttpResponseHeaderTests, GetName_KnownValues) {
    EXPECT_EQ(HttpResponseHeaderGetName(HttpResponseHeader::SetCookie), "Set-Cookie");
    EXPECT_EQ(HttpResponseHeaderGetName(HttpResponseHeader::WwwAuthenticate), "WWW-Authenticate");
    EXPECT_EQ(HttpResponseHeaderGetName(HttpResponseHeader::ETag), "ETag");
}

// ===========================================================================
// HttpVersion
// ===========================================================================

TEST(HttpVersionTests, Version11_Is1Dot1) {
    EXPECT_EQ(HttpVersion::Version11.Major, 1);
    EXPECT_EQ(HttpVersion::Version11.Minor, 1);
}

TEST(HttpVersionTests, Unknown_IsZeroZero) {
    EXPECT_EQ(HttpVersion::Unknown.Major, 0);
    EXPECT_EQ(HttpVersion::Unknown.Minor, 0);
}

TEST(HttpVersionTests, Version20_Is2Dot0) {
    EXPECT_EQ(HttpVersion::Version20.Major, 2);
    EXPECT_EQ(HttpVersion::Version20.Minor, 0);
}
