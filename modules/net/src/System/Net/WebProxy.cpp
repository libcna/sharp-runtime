// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/WebProxy.hpp"

#include "System/ArgumentException.hpp"
#include "System/Net/CredentialCache.hpp"
#include "System/Net/Dns.hpp"
#include "System/Net/NetworkCredential.hpp"
#include "System/PlatformNotSupportedException.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <regex>
#include <utility>

namespace System::Net {

    namespace {
        std::string percentDecode(const std::string& value) {
            auto hexValue = [](char character) -> int {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                if (character >= 'A' && character <= 'F') return character - 'A' + 10;
                return -1;
            };

            std::string decoded;
            decoded.reserve(value.size());
            for (std::size_t index = 0; index < value.size(); ++index) {
                if (value[index] == '%' && index + 2 < value.size()) {
                    const int high = hexValue(value[index + 1]);
                    const int low = hexValue(value[index + 2]);
                    if (high >= 0 && low >= 0) {
                        decoded.push_back(static_cast<char>((high << 4) | low));
                        index += 2;
                        continue;
                    }
                }
                decoded.push_back(value[index]);
            }
            return decoded;
        }

        // Emscripten does not expose the local DNS lookup used by IsBypassed(),
        // so the only call site below is compiled out on that target.
#if !defined(__EMSCRIPTEN__)
        bool equalsIgnoreCase(const std::string& left, const std::string& right) {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
        }
#endif
    }

    WebProxy::WebProxy() {
        initialize(nullptr, false, {}, nullptr);
    }

    WebProxy::WebProxy(std::shared_ptr<System::Uri> address) {
        initialize(std::move(address), false, {}, nullptr);
    }

    WebProxy::WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal) {
        initialize(std::move(address), bypassOnLocal, {}, nullptr);
    }

    WebProxy::WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                       const std::vector<std::string>& bypassList) {
        initialize(std::move(address), bypassOnLocal, bypassList, nullptr);
    }

    WebProxy::WebProxy(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                       const std::vector<std::string>& bypassList,
                       std::shared_ptr<ICredentials> credentials) {
        initialize(std::move(address), bypassOnLocal, bypassList, std::move(credentials));
    }

    WebProxy::WebProxy(const std::string& address) {
        initialize(CreateProxyUri(address), false, {}, nullptr);
    }

    WebProxy::WebProxy(const std::string& address, bool bypassOnLocal) {
        initialize(CreateProxyUri(address), bypassOnLocal, {}, nullptr);
    }

    WebProxy::WebProxy(const std::string& address, bool bypassOnLocal,
                       const std::vector<std::string>& bypassList) {
        initialize(CreateProxyUri(address), bypassOnLocal, bypassList, nullptr);
    }

    WebProxy::WebProxy(const std::string& address, bool bypassOnLocal,
                       const std::vector<std::string>& bypassList,
                       std::shared_ptr<ICredentials> credentials) {
        initialize(CreateProxyUri(address), bypassOnLocal, bypassList, std::move(credentials));
    }

    WebProxy::WebProxy(const std::string& host, intcs port) {
        initialize(CreateProxyUri(host, port), false, {}, nullptr);
    }

    void WebProxy::initialize(std::shared_ptr<System::Uri> address, bool bypassOnLocal,
                              const std::vector<std::string>& bypassList,
                              std::shared_ptr<ICredentials> credentials) {
        setAddressProperty(std::move(address));
        if (credentials) {
            setCredentialsProperty(std::move(credentials));
        }
        setBypassProxyOnLocalProperty(bypassOnLocal);
        setBypassListProperty(bypassList);
    }

    std::shared_ptr<System::Uri> WebProxy::CreateProxyUri(const std::string& address) {
        const std::string normalized = address.find("://") == std::string::npos
            ? "http://" + address
            : address;
        return std::make_shared<System::Uri>(normalized);
    }

    std::shared_ptr<System::Uri> WebProxy::CreateProxyUri(const std::string& host, intcs port) {
        const auto proxyUri = CreateProxyUri(host);
        if (port == -1) {
            return proxyUri;
        }
        std::string authority;
        if (!proxyUri->getUserInfoProperty().empty()) {
            authority += proxyUri->getUserInfoProperty();
            authority += '@';
        }
        authority += proxyUri->getHostProperty();
        authority += ':';
        authority += std::to_string(port);
        return std::make_shared<System::Uri>(proxyUri->getSchemeProperty() + "://" + authority +
                                             proxyUri->getAbsolutePathProperty() +
                                             proxyUri->getQueryProperty() +
                                             proxyUri->getFragmentProperty());
    }

    std::shared_ptr<NetworkCredential> WebProxy::GetCredentialsFromUri(const std::shared_ptr<System::Uri>& uri) {
        if (!uri || !uri->getIsAbsoluteUriProperty() || uri->getUserInfoProperty().empty()) {
            return nullptr;
        }

        const std::string& userInfo = uri->getUserInfoProperty();
        const std::size_t colonIndex = userInfo.find(':');
        if (colonIndex == std::string::npos) {
            return std::make_shared<NetworkCredential>(percentDecode(userInfo), "");
        }
        return std::make_shared<NetworkCredential>(
            percentDecode(userInfo.substr(0, colonIndex)),
            percentDecode(userInfo.substr(colonIndex + 1)));
    }

    void WebProxy::setAddressProperty(std::shared_ptr<System::Uri> value) {
        address_ = std::move(value);
        if (const auto uriCredentials = GetCredentialsFromUri(address_)) {
            setCredentialsProperty(uriCredentials);
        }
    }

    void WebProxy::UpdateRegexList(std::vector<std::string>& patterns) const {
        patterns.clear();
        patterns.reserve(static_cast<std::size_t>(bypassArrayList_.getCountProperty()));
        for (intcs index = 0; index < bypassArrayList_.getCountProperty(); ++index) {
            const auto* value = static_cast<const std::any*>(bypassArrayList_.getItem(index));
            const auto* pattern = value == nullptr ? nullptr : std::any_cast<std::string>(value);
            if (pattern == nullptr) {
                throw System::ArgumentException("BypassArrayList elements must be std::string values.", "value");
            }
            // Create each regex here so setBypassListProperty() eagerly reports invalid patterns,
            // as WebProxy.UpdateRegexList() does in .NET.
            try {
                std::regex(*pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
            } catch (const std::regex_error&) {
                throw System::ArgumentException("Invalid regular expression pattern.", "value");
            }
            patterns.push_back(*pattern);
        }
    }

    std::vector<std::string> WebProxy::getBypassListProperty() const {
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(bypassArrayList_.getCountProperty()));
        for (intcs index = 0; index < bypassArrayList_.getCountProperty(); ++index) {
            const auto* value = static_cast<const std::any*>(bypassArrayList_.getItem(index));
            const auto* pattern = value == nullptr ? nullptr : std::any_cast<std::string>(value);
            if (pattern == nullptr) {
                throw System::ArgumentException("BypassArrayList elements must be std::string values.", "value");
            }
            result.push_back(*pattern);
        }
        return result;
    }

    void WebProxy::setBypassListProperty(const std::vector<std::string>& value) {
        bypassArrayList_.Clear();
        for (const std::string& pattern : value) {
            bypassArrayList_.Add(std::any(pattern));
        }
        std::vector<std::string> validated;
        UpdateRegexList(validated);
    }

    bool WebProxy::getUseDefaultCredentialsProperty() const {
        return credentials_ == CredentialCache::getDefaultCredentialsProperty();
    }

    void WebProxy::setUseDefaultCredentialsProperty(bool value) {
        setCredentialsProperty(value ? CredentialCache::getDefaultCredentialsProperty() : nullptr);
    }

    bool WebProxy::IsMatchInBypassList(const System::Uri& input) const {
        std::vector<std::string> patterns;
        try {
            UpdateRegexList(patterns);
        } catch (...) {
            // .NET intentionally ignores malformed changes made directly through BypassArrayList.
            return false;
        }

        const std::string value = input.getSchemeProperty() + "://" + input.getAuthorityProperty();
        for (const std::string& pattern : patterns) {
            if (std::regex_search(value, std::regex(pattern,
                                  std::regex_constants::ECMAScript | std::regex_constants::icase))) {
                return true;
            }
        }
        return false;
    }

    bool WebProxy::IsLocal(const System::Uri& host) {
        if (host.getIsLoopbackProperty()) {
            return true;
        }

        const std::string& hostString = host.getHostProperty();
        IPAddress hostAddress;
        if (IPAddress::TryParse(hostString, hostAddress)) {
#if defined(__EMSCRIPTEN__)
            return false;
#else
            const IPHostEntry localHost = Dns::GetHostEntry(Dns::GetHostName());
            return std::any_of(localHost.getAddressListProperty().begin(), localHost.getAddressListProperty().end(),
                               [&hostAddress](const IPAddress& localAddress) { return localAddress == hostAddress; });
#endif
        }

        const std::size_t dot = hostString.find('.');
        if (dot == std::string::npos) {
            return true;
        }

#if defined(__EMSCRIPTEN__)
        return false;
#else
        const std::string localName = Dns::GetHostEntry(Dns::GetHostName()).getHostNameProperty();
        const std::size_t localDot = localName.find('.');
        return localDot != std::string::npos &&
               equalsIgnoreCase(hostString.substr(dot), localName.substr(localDot));
#endif
    }

    bool WebProxy::IsBypassed(const System::Uri& host) {
        return !address_ ||
               (bypassProxyOnLocal_ && IsLocal(host)) ||
               IsMatchInBypassList(host);
    }

    std::shared_ptr<System::Uri> WebProxy::GetProxy(const System::Uri& destination) {
        if (IsBypassed(destination)) {
            return std::make_shared<System::Uri>(destination);
        }
        return address_;
    }

    WebProxy WebProxy::GetDefaultProxy() {
        throw System::PlatformNotSupportedException(
            "WebProxy.GetDefaultProxy is not supported. Use explicitly configured proxy settings.");
    }

} // namespace System::Net
