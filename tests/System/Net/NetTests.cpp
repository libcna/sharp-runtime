// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include "System/Net/WebUtility.hpp"
#include "System/Net/DecompressionMethods.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::HttpStatusCode;
using System::Net::WebUtility;
using System::Net::DecompressionMethods;

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
