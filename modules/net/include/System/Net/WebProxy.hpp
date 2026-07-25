// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "System/Collections/ArrayList.hpp"
#include "System/Net/IWebProxy.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net {

    using SharpRuntime::intcs;

    /**
     * Contains HTTP proxy settings used to determine whether a Web proxy sends requests.
     *
     * C++ counterpart of .NET System.Net.WebProxy.
     *
     * @note The obsolete formatter-based serialization members are omitted under this
     * runtime's permanent serialization deviation. GetDefaultProxy() is retained and,
     * like current .NET, throws PlatformNotSupportedException.
     * @note The C++ regex engine uses ECMAScript syntax rather than .NET Regex syntax.
     * Most ordinary bypass patterns work unchanged; .NET-specific regex constructs are
     * intentionally not supported.
     * @note Local-domain detection uses the canonical local DNS name rather than
     * System.Net.NetworkInformation's domain and network-change notification service.
     */
    class WebProxy : public IWebProxy {
        std::shared_ptr<System::Uri> address_;
        System::Collections::ArrayList bypassArrayList_;
        bool bypassProxyOnLocal_ = false;
        std::shared_ptr<ICredentials> credentials_;

        static std::shared_ptr<System::Uri> CreateProxyUri(const std::string& address);
        static std::shared_ptr<System::Uri> CreateProxyUri(const std::string& host, intcs port);
        static std::shared_ptr<NetworkCredential> GetCredentialsFromUri(const std::shared_ptr<System::Uri>& uri);
        static bool IsLocal(const System::Uri& host);

        void UpdateRegexList(std::vector<std::string>& patterns) const;
        [[nodiscard]] bool IsMatchInBypassList(const System::Uri& input) const;

        void initialize(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                        const std::vector<std::string>& bypassList,
                        std::shared_ptr<ICredentials> credentials);

    public:
        /** Initializes an empty instance with a null proxy Address. */
        WebProxy();

        /** Initializes a new instance from the specified proxy URI. */
        explicit WebProxy(std::shared_ptr<System::Uri> address);

        /** Initializes a new instance from the specified proxy URI and bypass setting. */
        WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal);

        /** Initializes a new instance from a proxy URI, bypass setting, and bypass patterns. */
        WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                 const std::vector<std::string>& bypassList);

        /** Initializes a new instance from a proxy URI, bypass setting, patterns, and credentials. */
        WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                 const std::vector<std::string>& bypassList,
                 std::shared_ptr<ICredentials> credentials);

        /** Initializes a new instance from a proxy address string. A missing scheme defaults to http. */
        explicit WebProxy(const std::string& address);

        /** Initializes a new instance from an address string and bypass setting. */
        WebProxy(const std::string& address, bool bypassOnLocal);

        /** Initializes a new instance from an address string, bypass setting, and bypass patterns. */
        WebProxy(const std::string& address, bool bypassOnLocal,
                 const std::vector<std::string>& bypassList);

        /** Initializes a new instance from an address string, settings, patterns, and credentials. */
        WebProxy(const std::string& address, bool bypassOnLocal,
                 const std::vector<std::string>& bypassList,
                 std::shared_ptr<ICredentials> credentials);

        /** Initializes a new instance from a proxy host and port. */
        WebProxy(const std::string& host, intcs port);

        ~WebProxy() override = default;

        /** Returns the address of the proxy server, or null when no proxy is configured. */
        [[nodiscard]] std::shared_ptr<System::Uri> getAddressProperty() const { return address_; }

        /**
         * Sets the address of the proxy server. URI user-info, when present, becomes Credentials.
         * A null value clears the proxy address without changing existing credentials.
         */
        void setAddressProperty(std::shared_ptr<System::Uri> value);

        /**
         * Returns the mutable list of bypass regular expressions.
         *
         * Elements must be std::string values. Direct mutations are observed by IsBypassed().
         */
        [[nodiscard]] System::Collections::ArrayList& getBypassArrayListProperty() { return bypassArrayList_; }

        /** Returns a copy of the bypass regular-expression strings. */
        [[nodiscard]] std::vector<std::string> getBypassListProperty() const;

        /**
         * Replaces bypass regular expressions. An invalid pattern throws ArgumentException.
         * C++ has no nullable std::vector; an empty vector corresponds to a null or empty C# array.
         */
        void setBypassListProperty(const std::vector<std::string>& value);

        /** Returns whether the proxy is bypassed for local addresses. */
        [[nodiscard]] bool getBypassProxyOnLocalProperty() const { return bypassProxyOnLocal_; }

        /** Sets whether the proxy is bypassed for local addresses. */
        void setBypassProxyOnLocalProperty(bool value) { bypassProxyOnLocal_ = value; }

        /** Returns the credentials submitted to the proxy server for authentication. */
        [[nodiscard]] std::shared_ptr<ICredentials> getCredentialsProperty() const override { return credentials_; }

        /** Sets the credentials submitted to the proxy server for authentication. */
        void setCredentialsProperty(std::shared_ptr<ICredentials> value) override { credentials_ = std::move(value); }

        /** Returns whether CredentialCache::DefaultCredentials are used. */
        [[nodiscard]] bool getUseDefaultCredentialsProperty() const;

        /** Uses CredentialCache::DefaultCredentials when true; clears Credentials when false. */
        void setUseDefaultCredentialsProperty(bool value);

        /**
         * Returns the proxy URI for a destination, or a URI equivalent to the destination when bypassed.
         *
         * @param destination The requested Internet resource.
         */
        [[nodiscard]] std::shared_ptr<System::Uri> GetProxy(const System::Uri& destination) override;

        /** Indicates whether the proxy server should be bypassed for the specified host. */
        [[nodiscard]] bool IsBypassed(const System::Uri& host) override;

        /**
         * Returns the system proxy configuration.
         *
         * @throws System::PlatformNotSupportedException always, matching .NET Core.
         */
        [[nodiscard]] static WebProxy GetDefaultProxy();
    };

} // namespace System::Net
