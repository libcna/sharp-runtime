// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Uri.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/UriFormatException.hpp"
#include <cctype>
#include <functional>

namespace System {

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

intcs Uri::defaultPortForScheme(const std::string& scheme) {
    // Matches .NET's built-in UriParser scheme table exactly (UriSyntax.cs);
    // note "ssh" and bare "smtp" are not .NET built-in schemes and have no default port.
    if (scheme == "http")    return 80;
    if (scheme == "https")   return 443;
    if (scheme == "ws")      return 80;
    if (scheme == "wss")     return 443;
    if (scheme == "ftp")     return 21;
    if (scheme == "gopher")  return 70;
    if (scheme == "nntp")    return 119;
    if (scheme == "mailto")  return 25;
    if (scheme == "telnet")  return 23;
    if (scheme == "ldap")    return 389;
    if (scheme == "net.tcp") return 808;
    return -1;
}

namespace {
    /**
     * Detects a leading RFC 3986 scheme "ALPHA *(ALPHA/DIGIT/"+"/"-"/".") \":\""
     * and returns the index of the ':', or npos if the string has no valid scheme prefix.
     */
    std::size_t findSchemeColon(const std::string& s) {
        if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0])))
            return std::string::npos;
        std::size_t i = 1;
        while (i < s.size()) {
            char c = s[i];
            if (c == ':') return i;
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.') {
                ++i;
                continue;
            }
            return std::string::npos;
        }
        return std::string::npos;
    }

    /**
     * @brief Removes "." and ".." path segments per RFC 3986 §5.2.4.
     *
     * Used when combining a base and relative URI, matching .NET's Uri.Compress()
     * step in CombineUri (Uri.cs), which normalizes the merged path this same way.
     */
    std::string removeDotSegments(const std::string& path) {
        std::string input = path;
        std::string output;
        while (!input.empty()) {
            if (input.rfind("../", 0) == 0) {
                input.erase(0, 3);
            } else if (input.rfind("./", 0) == 0) {
                input.erase(0, 2);
            } else if (input.rfind("/./", 0) == 0) {
                input.erase(0, 2); // leaves the leading '/'
            } else if (input == "/.") {
                input = "/";
            } else if (input.rfind("/../", 0) == 0) {
                input.erase(0, 3); // leaves the leading '/'
                auto lastSlash = output.rfind('/');
                output.erase(lastSlash == std::string::npos ? 0 : lastSlash);
            } else if (input == "/..") {
                input = "/";
                auto lastSlash = output.rfind('/');
                output.erase(lastSlash == std::string::npos ? 0 : lastSlash);
            } else if (input == "." || input == "..") {
                input.clear();
            } else {
                std::size_t start = (input[0] == '/') ? 1 : 0;
                auto nextSlash = input.find('/', start);
                std::size_t segEnd = (nextSlash == std::string::npos) ? input.size() : nextSlash;
                output += input.substr(0, segEnd);
                input.erase(0, segEnd);
            }
        }
        return output;
    }
}

void Uri::parse(const std::string& uriString) {
    if (uriString.empty())
        throw System::UriFormatException("URI string must not be empty");

    auto schemeSep = uriString.find("://");
    std::size_t opaqueColon = std::string::npos;
    if (schemeSep == std::string::npos)
        opaqueColon = findSchemeColon(uriString);

    if (schemeSep == std::string::npos && opaqueColon == std::string::npos) {
        // treat as relative
        isAbsoluteUri_ = false;
        absoluteUri_   = uriString;
        path_          = uriString;
        return;
    }

    if (schemeSep == std::string::npos) {
        // Opaque (non-hierarchical) absolute URI, e.g. "mailto:user@example.com" or
        // "urn:isbn:0-395-36341-1" — a scheme followed by ':' with no "//" authority.
        scheme_ = uriString.substr(0, opaqueColon);
        std::string rest = uriString.substr(opaqueColon + 1);

        auto fragPos = rest.find('#');
        if (fragPos != std::string::npos) {
            fragment_ = rest.substr(fragPos);
            rest      = rest.substr(0, fragPos);
        }
        auto queryPos = rest.find('?');
        if (queryPos != std::string::npos) {
            query_ = rest.substr(queryPos);
            rest   = rest.substr(0, queryPos);
        }

        path_          = rest;
        host_.clear();
        userInfo_.clear();
        port_          = -1;
        absoluteUri_   = uriString;
        isAbsoluteUri_ = true;
        return;
    }

    scheme_ = uriString.substr(0, schemeSep);
    if (scheme_.empty())
        throw System::UriFormatException("URI scheme must not be empty");

    // validate scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    for (std::size_t i = 0; i < scheme_.size(); ++i) {
        char c = scheme_[i];
        if (i == 0 && !std::isalpha(static_cast<unsigned char>(c)))
            throw System::UriFormatException("URI scheme must start with a letter");
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.')
            throw System::UriFormatException("Invalid character in URI scheme");
    }

    std::string rest = uriString.substr(schemeSep + 3);

    // extract fragment
    auto fragPos = rest.find('#');
    if (fragPos != std::string::npos) {
        fragment_ = rest.substr(fragPos);
        rest      = rest.substr(0, fragPos);
    }

    // extract query
    auto queryPos = rest.find('?');
    if (queryPos != std::string::npos) {
        query_ = rest.substr(queryPos);
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

    // extract port — look for last ':' after any IPv6 ']'
    auto bracketClose = authority.rfind(']');
    auto colonPos     = authority.rfind(':');
    if (colonPos != std::string::npos &&
        (bracketClose == std::string::npos || colonPos > bracketClose)) {
        std::string portStr = authority.substr(colonPos + 1);
        if (!portStr.empty()) {
            try {
                port_ = std::stoi(portStr);
                host_ = authority.substr(0, colonPos);
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

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Uri::Uri(const std::string& uriString) {
    parse(uriString);
}

Uri::Uri(const std::string& uriString, UriKind uriKind) {
    parse(uriString);
    if (uriKind == UriKind::Absolute && !isAbsoluteUri_)
        throw System::UriFormatException("URI must be absolute");
    if (uriKind == UriKind::Relative && isAbsoluteUri_)
        throw System::UriFormatException("URI must be relative");
}

Uri::Uri(const Uri& baseUri, const std::string& relativeUri) {
    if (!baseUri.isAbsoluteUri_)
        throw System::ArgumentOutOfRangeException("baseUri");
    if (relativeUri.empty()) {
        *this = baseUri;
        return;
    }
    if (relativeUri.find("://") != std::string::npos) {
        parse(relativeUri);
        return;
    }
    // Split off query/fragment so dot-segment removal (RFC 3986 §5.2.4) only ever
    // operates on the path component, matching .NET's CombineUri + Compress() (Uri.cs).
    std::string relativePath = relativeUri;
    std::string relativeTail; // query/fragment, appended back verbatim after normalization
    auto tailPos = relativePath.find_first_of("?#");
    if (tailPos != std::string::npos) {
        relativeTail = relativePath.substr(tailPos);
        relativePath = relativePath.substr(0, tailPos);
    }

    std::string mergedPath;
    if (!relativePath.empty() && relativePath[0] == '/') {
        mergedPath = relativePath;
    } else {
        // RFC 3986 §5.3 merge: base path truncated up to and including its last '/'.
        auto lastSlash = baseUri.path_.rfind('/');
        std::string basePrefix = (lastSlash == std::string::npos) ? "/" : baseUri.path_.substr(0, lastSlash + 1);
        mergedPath = basePrefix + relativePath;
    }
    mergedPath = removeDotSegments(mergedPath);

    std::string combined = baseUri.scheme_ + "://" + baseUri.host_;
    if (baseUri.port_ != defaultPortForScheme(baseUri.scheme_) && baseUri.port_ != -1)
        combined += ':' + std::to_string(baseUri.port_);
    combined += mergedPath;
    combined += relativeTail;
    parse(combined);
}

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

const std::string& Uri::getAbsoluteUriProperty()  const { return absoluteUri_; }
const std::string& Uri::getSchemeProperty()        const { return scheme_; }
const std::string& Uri::getHostProperty()          const { return host_; }
intcs              Uri::getPortProperty()           const { return port_; }
const std::string& Uri::getAbsolutePathProperty()  const { return path_; }
const std::string& Uri::getQueryProperty()         const { return query_; }
const std::string& Uri::getFragmentProperty()      const { return fragment_; }
const std::string& Uri::getUserInfoProperty()       const { return userInfo_; }
bool               Uri::getIsAbsoluteUriProperty() const { return isAbsoluteUri_; }

std::string Uri::getPathAndQueryProperty() const { return path_ + query_; }

std::string Uri::getAuthorityProperty() const {
    if (port_ == -1 || port_ == defaultPortForScheme(scheme_))
        return host_;
    return host_ + ':' + std::to_string(port_);
}

bool Uri::getIsLoopbackProperty() const {
    return host_ == "localhost" || host_ == "127.0.0.1" || host_ == "::1";
}

std::string Uri::ToString() const { return absoluteUri_; }

intcs Uri::GetHashCode() const {
    return static_cast<intcs>(std::hash<std::string>{}(absoluteUri_));
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

bool Uri::operator==(const Uri& other) const { return absoluteUri_ == other.absoluteUri_; }
bool Uri::operator!=(const Uri& other) const { return !(*this == other); }

// ---------------------------------------------------------------------------
// Static factory
// ---------------------------------------------------------------------------

bool Uri::TryCreate(const std::string& uriString, UriKind uriKind,
                    std::shared_ptr<Uri>& result) {
    try {
        result = std::make_shared<Uri>(uriString, uriKind);
        return true;
    } catch (...) {
        result = nullptr;
        return false;
    }
}

} // namespace System
