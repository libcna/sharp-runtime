// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <utility>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/Net/CredentialCache.hpp"
#include "System/Net/NetworkCredential.hpp"
#include "System/Net/WebProxy.hpp"
#include "System/PlatformNotSupportedException.hpp"

namespace {

    using System::Net::CredentialCache;
    using System::Net::ICredentials;
    using System::Net::IWebProxy;
    using System::Net::NetworkCredential;
    using System::Net::WebProxy;

    class DirectProxy final : public IWebProxy {
        std::shared_ptr<ICredentials> credentials_;

    public:
        std::shared_ptr<System::Uri> GetProxy(const System::Uri& destination) override {
            return std::make_shared<System::Uri>(destination);
        }

        bool IsBypassed(const System::Uri&) override { return true; }

        std::shared_ptr<ICredentials> getCredentialsProperty() const override { return credentials_; }

        void setCredentialsProperty(std::shared_ptr<ICredentials> value) override { credentials_ = std::move(value); }
    };
}

TEST(WebProxyTests, DefaultInstanceBypassesAndReturnsDestination) {
    WebProxy proxy;
    const System::Uri destination("https://example.test/path");

    EXPECT_EQ(proxy.getAddressProperty(), nullptr);
    EXPECT_TRUE(proxy.IsBypassed(destination));
    const auto result = proxy.GetProxy(destination);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, destination);
}

TEST(WebProxyTests, StringAndHostPortConstructorsCreateProxyUri) {
    WebProxy stringProxy("proxy.example:8080");
    ASSERT_NE(stringProxy.getAddressProperty(), nullptr);
    EXPECT_EQ(stringProxy.getAddressProperty()->getAbsoluteUriProperty(), "http://proxy.example:8080");

    WebProxy hostPortProxy("socks5://proxy.example/path", 1080);
    ASSERT_NE(hostPortProxy.getAddressProperty(), nullptr);
    EXPECT_EQ(hostPortProxy.getAddressProperty()->getAbsoluteUriProperty(), "socks5://proxy.example:1080/path");

    WebProxy defaultPortProxy("proxy.example", -1);
    ASSERT_NE(defaultPortProxy.getAddressProperty(), nullptr);
    EXPECT_EQ(defaultPortProxy.getAddressProperty()->getAbsoluteUriProperty(), "http://proxy.example");
}

TEST(WebProxyTests, AddressSelectsProxyUnlessDestinationIsBypassed) {
    WebProxy proxy("http://proxy.example:8080");
    const System::Uri destination("https://example.test/path");

    EXPECT_FALSE(proxy.IsBypassed(destination));
    const auto result = proxy.GetProxy(destination);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getAbsoluteUriProperty(), "http://proxy.example:8080");
}

TEST(WebProxyTests, BypassListMatchesSchemeHostAndNonDefaultPortIgnoringCase) {
    WebProxy proxy("proxy.example", false, {R"(^HTTPS://SERVICE\.EXAMPLE:8443$)"});
    const System::Uri bypassed("https://service.example:8443/resource");
    const System::Uri proxied("https://service.example/resource");

    EXPECT_TRUE(proxy.IsBypassed(bypassed));
    EXPECT_FALSE(proxy.IsBypassed(proxied));
    EXPECT_EQ(proxy.getBypassListProperty(), (std::vector<std::string>{R"(^HTTPS://SERVICE\.EXAMPLE:8443$)"}));
}

TEST(WebProxyTests, BypassArrayListMutationsAreObservedAndInvalidMutationsAreIgnored) {
    WebProxy proxy("proxy.example");
    proxy.getBypassArrayListProperty().Add(std::any(std::string(R"(^http://internal\.example$)")));
    EXPECT_TRUE(proxy.IsBypassed(System::Uri("http://internal.example/path")));

    proxy.getBypassArrayListProperty().Add(std::any(std::string("[invalid")));
    EXPECT_FALSE(proxy.IsBypassed(System::Uri("http://internal.example/path")));
    EXPECT_EQ(proxy.getBypassListProperty(),
              (std::vector<std::string>{R"(^http://internal\.example$)", "[invalid"}));
}

TEST(WebProxyTests, InvalidBypassListThrowsDuringSet) {
    WebProxy proxy("proxy.example");
    EXPECT_THROW(proxy.setBypassListProperty({"[invalid"}), System::ArgumentException);
}

TEST(WebProxyTests, BypassOnLocalRecognizesLoopbackAndSingleLabelHost) {
    WebProxy proxy("proxy.example", true);

    EXPECT_TRUE(proxy.IsBypassed(System::Uri("http://localhost/path")));
    EXPECT_TRUE(proxy.IsBypassed(System::Uri("http://intranet/path")));
    EXPECT_FALSE(proxy.IsBypassed(System::Uri("http://remote.example/path")));
}

TEST(WebProxyTests, AddressUserInfoSuppliesCredentialsAndExplicitCredentialsWin) {
    WebProxy uriCredentials("http://user%20name:pass%21@proxy.example:8080");
    const auto extracted = std::dynamic_pointer_cast<NetworkCredential>(uriCredentials.getCredentialsProperty());
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(extracted->getUserNameProperty(), "user name");
    EXPECT_EQ(extracted->getPasswordProperty(), "pass!");

    const auto explicitCredentials = std::make_shared<NetworkCredential>("explicit", "secret");
    WebProxy explicitWins("http://uri:credentials@proxy.example", false, {}, explicitCredentials);
    EXPECT_EQ(explicitWins.getCredentialsProperty(), explicitCredentials);
}

TEST(WebProxyTests, UseDefaultCredentialsUsesCredentialCacheIdentity) {
    WebProxy proxy("proxy.example");
    EXPECT_FALSE(proxy.getUseDefaultCredentialsProperty());

    proxy.setUseDefaultCredentialsProperty(true);
    EXPECT_TRUE(proxy.getUseDefaultCredentialsProperty());
    EXPECT_EQ(proxy.getCredentialsProperty(), CredentialCache::getDefaultCredentialsProperty());

    proxy.setUseDefaultCredentialsProperty(false);
    EXPECT_FALSE(proxy.getUseDefaultCredentialsProperty());
    EXPECT_EQ(proxy.getCredentialsProperty(), nullptr);
}

TEST(WebProxyTests, IWebProxyCanBeUsedPolymorphically) {
    DirectProxy direct;
    IWebProxy& proxy = direct;
    const System::Uri destination("https://example.test/");

    EXPECT_TRUE(proxy.IsBypassed(destination));
    EXPECT_EQ(*proxy.GetProxy(destination), destination);
}

TEST(WebProxyTests, GetDefaultProxyMatchesDotNetCorePlatformNotSupportedBehavior) {
    EXPECT_THROW(WebProxy::GetDefaultProxy(), System::PlatformNotSupportedException);
}
