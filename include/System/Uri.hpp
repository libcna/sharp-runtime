// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

namespace System {

    enum class UriKind {
        RelativeOrAbsolute,
        Absolute,
        Relative
    };

    /**
     * @brief Provides an object representation of a uniform resource identifier (URI).
     *
     * Partial C++ counterpart of .NET System.Uri.
     * Handles http/https/ftp/file/urn and other absolute URIs.
     *
     * @note Status: Partial
     */
    class Uri {
        std::string absoluteUri_;
        std::string scheme_;
        std::string userInfo_;
        std::string host_;
        int         port_     = -1;
        std::string path_;
        std::string query_;    // includes leading '?'
        std::string fragment_; // includes leading '#'
        bool        isAbsoluteUri_ = true;

        static int defaultPortForScheme(const std::string& scheme) {
            if (scheme == "http")  return 80;
            if (scheme == "https") return 443;
            if (scheme == "ftp")   return 21;
            if (scheme == "ssh")   return 22;
            if (scheme == "smtp")  return 25;
            return -1;
        }

        void parse(const std::string& uriString) {
            if (uriString.empty())
                throw std::invalid_argument("URI string must not be empty");

            auto schemeSep = uriString.find("://");
            if (schemeSep == std::string::npos) {
                // treat as relative
                isAbsoluteUri_ = false;
                absoluteUri_   = uriString;
                path_          = uriString;
                return;
            }

            scheme_ = uriString.substr(0, schemeSep);
            if (scheme_.empty())
                throw std::invalid_argument("URI scheme must not be empty");

            // validate scheme chars: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
            for (std::size_t i = 0; i < scheme_.size(); ++i) {
                char c = scheme_[i];
                if (i == 0 && !std::isalpha(static_cast<unsigned char>(c)))
                    throw std::invalid_argument("URI scheme must start with a letter");
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.')
                    throw std::invalid_argument("Invalid character in URI scheme");
            }

            std::string rest = uriString.substr(schemeSep + 3);

            // extract fragment
            auto fragPos = rest.find('#');
            if (fragPos != std::string::npos) {
                fragment_ = rest.substr(fragPos); // keep '#'
                rest      = rest.substr(0, fragPos);
            }

            // extract query
            auto queryPos = rest.find('?');
            if (queryPos != std::string::npos) {
                query_ = rest.substr(queryPos); // keep '?'
                rest   = rest.substr(0, queryPos);
            }

            // split authority and path
            auto pathPos = rest.find('/');
            std::string authority;
            if (pathPos != std::string::npos) {
                authority = rest.substr(0, pathPos);
                path_     = rest.substr(pathPos);
            } else {
                authority = rest;
                path_     = "/";
            }

            // extract userInfo
            auto atPos = authority.find('@');
            if (atPos != std::string::npos) {
                userInfo_ = authority.substr(0, atPos);
                authority = authority.substr(atPos + 1);
            }

            // extract port — look for last ':' after IPv6 ']'
            auto bracketClose = authority.rfind(']');
            auto colonPos     = authority.rfind(':');
            if (colonPos != std::string::npos &&
                (bracketClose == std::string::npos || colonPos > bracketClose)) {
                std::string portStr = authority.substr(colonPos + 1);
                if (!portStr.empty()) {
                    try {
                        port_  = std::stoi(portStr);
                        host_  = authority.substr(0, colonPos);
                    } catch (...) {
                        host_ = authority;
                    }
                } else {
                    host_ = authority.substr(0, colonPos);
                }
            } else {
                host_ = authority;
                port_ = defaultPortForScheme(scheme_);
            }

            absoluteUri_   = uriString;
            isAbsoluteUri_ = true;
        }

    public:
        explicit Uri(const std::string& uriString) {
            parse(uriString);
        }

        Uri(const std::string& uriString, UriKind uriKind) {
            parse(uriString);
            if (uriKind == UriKind::Absolute && !isAbsoluteUri_)
                throw std::invalid_argument("URI must be absolute");
            if (uriKind == UriKind::Relative && isAbsoluteUri_)
                throw std::invalid_argument("URI must be relative");
        }

        // Relative URI based on base + relative string
        Uri(const Uri& baseUri, const std::string& relativeUri) {
            if (!baseUri.isAbsoluteUri_)
                throw std::invalid_argument("Base URI must be absolute");
            if (relativeUri.empty()) {
                *this = baseUri;
                return;
            }
            if (relativeUri.find("://") != std::string::npos) {
                parse(relativeUri);
                return;
            }
            // Simple combination: base scheme+host + relative path
            std::string combined = baseUri.scheme_ + "://" + baseUri.host_;
            if (baseUri.port_ != defaultPortForScheme(baseUri.scheme_) && baseUri.port_ != -1)
                combined += ':' + std::to_string(baseUri.port_);
            if (!relativeUri.empty() && relativeUri[0] != '/')
                combined += baseUri.path_;
            combined += relativeUri;
            parse(combined);
        }

        [[nodiscard]] const std::string& getAbsoluteUriProperty()  const { return absoluteUri_; }
        [[nodiscard]] const std::string& getSchemeProperty()        const { return scheme_; }
        [[nodiscard]] const std::string& getHostProperty()          const { return host_; }
        [[nodiscard]] int                getPortProperty()           const { return port_; }
        [[nodiscard]] const std::string& getAbsolutePathProperty()  const { return path_; }
        [[nodiscard]] const std::string& getQueryProperty()         const { return query_; }
        [[nodiscard]] const std::string& getFragmentProperty()      const { return fragment_; }
        [[nodiscard]] const std::string& getUserInfoProperty()       const { return userInfo_; }
        [[nodiscard]] bool               getIsAbsoluteUriProperty() const { return isAbsoluteUri_; }

        [[nodiscard]] std::string getPathAndQueryProperty() const {
            return path_ + query_;
        }

        [[nodiscard]] std::string getAuthorityProperty() const {
            if (port_ == -1 || port_ == defaultPortForScheme(scheme_))
                return host_;
            return host_ + ':' + std::to_string(port_);
        }

        [[nodiscard]] bool getIsLoopbackProperty() const {
            return host_ == "localhost" || host_ == "127.0.0.1" || host_ == "::1";
        }

        [[nodiscard]] std::string ToString() const { return absoluteUri_; }

        bool operator==(const Uri& other) const { return absoluteUri_ == other.absoluteUri_; }
        bool operator!=(const Uri& other) const { return !(*this == other); }

        static bool TryCreate(const std::string& uriString, UriKind uriKind,
                              std::shared_ptr<Uri>& result) {
            try {
                result = std::make_shared<Uri>(uriString, uriKind);
                return true;
            } catch (...) {
                result = nullptr;
                return false;
            }
        }
    };

} // namespace System
