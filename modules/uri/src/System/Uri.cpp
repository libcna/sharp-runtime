// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Uri.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/UriFormatException.hpp"
#include <algorithm>
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
     *
     * This is the SINGLE point at which a scheme is recognised (ticket #1988). parse()
     * previously located the scheme with `find("://")` -- a search for "://" ANYWHERE in
     * the string -- and consulted this function only when that search failed, so the file
     * carried two contradictory notions of where the scheme ends and used the wrong one
     * first. Measured consequences, every one of them ordinary input:
     *   "/path?redirect=http://evil.com"   threw "URI scheme must start with a letter"
     *   "search?url=https://example.com"   threw "Invalid character in URI scheme"
     *   "mailto:a@b.com?body=see http://x" threw "Invalid character in URI scheme"
     *   "foo:bar://baz"                    threw "Invalid character in URI scheme"
     * because the substring match landed inside a query, or past a first colon. A relative
     * reference whose query embeds an absolute URL -- the commonest redirect/callback
     * shape there is -- could not be constructed at all.
     *
     * The replacement is a strict widening, and that is a proof rather than a judgement
     * (docs/SystemUriNamespaceReviewPlan.md 9.1): a scheme that passed the old validation
     * contained no colon, so the first colon in the string was exactly the "://" the old
     * search found, which is exactly what this function returns. Every input the old code
     * ACCEPTED therefore takes the same branch here and yields identical components; only
     * inputs that threw can change, and they become relative or opaque URIs -- which is
     * what RFC 3986 calls a reference whose leading token is not a scheme.
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

    // RFC 3986 §3: the scheme is the token before the FIRST colon, and an authority is
    // present only when "//" immediately follows that colon. findSchemeColon() (above) is
    // the single point that decides both, per ticket #1988.
    const std::size_t schemeColon = findSchemeColon(uriString);
    const bool hasAuthority = schemeColon != std::string::npos &&
                              uriString.compare(schemeColon + 1, 2, "//") == 0;

    if (schemeColon == std::string::npos) {
        // No scheme token at all: an RFC 3986 relative reference.
        isAbsoluteUri_ = false;
        absoluteUri_   = uriString;
        path_          = uriString;
        return;
    }

    if (!hasAuthority) {
        // Opaque (non-hierarchical) absolute URI, e.g. "mailto:user@example.com" or
        // "urn:isbn:0-395-36341-1" — a scheme followed by ':' with no "//" authority.
        scheme_ = uriString.substr(0, schemeColon);
        std::string rest = uriString.substr(schemeColon + 1);

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
        // Ticket #1989 (SR-AUD-143): this branch used to assign -1 unconditionally, which
        // bypassed defaultPortForScheme() -- the table two dozen lines above that declares
        // mailto=25 and telnet=23 -- so the file contradicted itself. Measured before the
        // repair: "mailto:user@example.com" reported -1 and so did
        // "telnet:host.example.com". The finding names only mailto; every opaque scheme
        // with a table entry was affected.
        port_          = defaultPortForScheme(scheme_);
        absoluteUri_   = uriString;
        isAbsoluteUri_ = true;
        return;
    }

    // The scheme needs no second validation pass: findSchemeColon() returned an index only
    // because every character before it already satisfied ALPHA *( ALPHA / DIGIT / "+" /
    // "-" / "." ), which is the identical grammar the removed loop enforced. Keeping the
    // loop would restore the two-grammars problem #1988 exists to remove.
    scheme_ = uriString.substr(0, schemeColon);

    std::string rest = uriString.substr(schemeColon + 3);

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

    // Validate the bracket structure of an IP-literal authority BEFORE the port split.
    //
    // Ticket #1991 (SR-AUD-145a). The split below is `rfind(':')` guarded only by
    // `rfind(']')`, so a bracketed host whose brackets are malformed was silently
    // reinterpreted rather than rejected. Measured before the repair:
    //   "http://[::1/path"      -> host "[:"        AND port 1
    //   "http://[::1]junk/path" -> host "[::1]junk" (accepted)
    //   "http://[]/path"        -> host "[]"        (accepted)
    // The finding names only the first case's host. The fabricated PORT is the more
    // dangerous half: a caller that connects to Port reaches port 1 rather than failing,
    // because "[::1" has its last colon at index 2 and "1" reads as a port number.
    //
    // Only the bracket STRUCTURE is checked — a ']' must exist, the literal must not be
    // empty, and nothing but a ':' port separator may follow the ']'. Validating the
    // literal's CONTENT, and zone identifiers, stay out of scope
    // (docs/SystemUriNamespaceReviewPlan.md §15.4). This is the same contract this
    // repository's own HttpClient::parseUrl already enforces for an unterminated literal.
    if (!authority.empty() && authority[0] == '[') {
        auto close = authority.find(']');
        if (close == std::string::npos || close == 1 ||
            (close + 1 < authority.size() && authority[close + 1] != ':'))
            throw System::UriFormatException("Invalid URI: An invalid IPv6 address was specified.");
    }

    // extract port — look for last ':' after any IPv6 ']'
    auto bracketClose = authority.rfind(']');
    auto colonPos     = authority.rfind(':');
    if (colonPos != std::string::npos &&
        (bracketClose == std::string::npos || colonPos > bracketClose)) {
        std::string portStr = authority.substr(colonPos + 1);
        if (!portStr.empty()) {
            // Verified against Uri.cs's port-parsing loop (throws ParsingError.BadPort, which
            // surfaces as UriFormatException, for any non-digit character in the port position
            // or a value > 0xFFFF). The previous code let std::stoi's exception on a
            // non-numeric or int-overflowing port fall through to `host_ = authority`, which
            // mangled host_ into the WHOLE "host:badport" text (colon and all) instead of
            // rejecting the URI -- and a successfully-parsed but out-of-range port (e.g.
            // "http://host:99999/") wasn't range-checked at all.
            bool validDigits = std::all_of(portStr.begin(), portStr.end(),
                [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
            long parsedPort = -1;
            if (validDigits) {
                try { parsedPort = std::stol(portStr); }
                catch (...) { validDigits = false; }
            }
            if (!validDigits || parsedPort > 65535)
                throw System::UriFormatException("Invalid URI: Invalid port specified.");
            port_ = static_cast<intcs>(parsedPort);
            host_ = authority.substr(0, colonPos);
        } else {
            // An authority ending in a bare ':' carries no port, so the scheme's default
            // applies exactly as it does when there is no colon at all. Ticket #1989's
            // second, unnamed site: before the repair "http://example.com:/" reported -1
            // while "http://example.com/" reported 80, so two spellings of one URI
            // disagreed (docs/SystemUriNamespaceReviewPlan.md §4.3).
            host_ = authority.substr(0, colonPos);
            port_ = defaultPortForScheme(scheme_);
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
    // Ticket #1992 (SR-AUD-145b): reject a value outside the enum's declared domain BEFORE
    // parsing anything. The two guards below match only Absolute and Relative, so any
    // other value used to fall through both and silently mean RelativeOrAbsolute --
    // measured, static_cast<UriKind>(99) and static_cast<UriKind>(-1) were both accepted.
    // This is the same cause, and reuses the same policy, as System::Runtime's GCSettings
    // setters (#1976) and System::Threading's boundary checks (#1954); the message mirrors
    // SR.Argument_InvalidEnumValue, "The value '{0}' is not valid for this usage of the
    // type {1}.", as Decimal::Round and Math::Round already spell it.
    if (uriKind != UriKind::RelativeOrAbsolute && uriKind != UriKind::Absolute &&
        uriKind != UriKind::Relative)
        throw System::ArgumentException(
            "The value '" + std::to_string(static_cast<int>(uriKind))
                + "' is not valid for this usage of the type UriKind.",
            "uriKind");
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
    // Verified against Uri.cs's CreateUri/ResolveHelper: relativeUri is parsed standalone
    // first, and if IT is itself absolute -- whether hierarchical ("scheme://...") OR opaque
    // ("scheme:rest", e.g. "mailto:x@y.com", "urn:isbn:123") -- the result is built entirely
    // from it, discarding the base entirely. The previous check only recognized the "://"
    // hierarchical form via a raw substring search, so an opaque absolute relativeUri (no "//")
    // was incorrectly treated as a relative path segment to merge with the base instead of
    // being used directly. findSchemeColon() (defined above, already used by parse() for this
    // exact detection) recognizes both forms, since it only requires a valid scheme token
    // followed by ':' -- whatever follows (including "//") doesn't matter to it.
    if (findSchemeColon(relativeUri) != std::string::npos) {
        parse(relativeUri);
        return;
    }
    // RFC 3986 §5.3: "if defined, userinfo, host, port [are] copied from base" into the merged
    // authority. The previous code omitted baseUri.userInfo_ entirely, so combining a base URI
    // that carries embedded credentials (e.g. "http://user:pass@example.com/path/") with a
    // relative reference silently dropped them from the result.
    const auto baseAuthority = [&baseUri]() {
        std::string s = baseUri.scheme_ + "://";
        if (!baseUri.userInfo_.empty())
            s += baseUri.userInfo_ + '@';
        s += baseUri.host_;
        if (baseUri.port_ != defaultPortForScheme(baseUri.scheme_) && baseUri.port_ != -1)
            s += ':' + std::to_string(baseUri.port_);
        return s;
    };

    // RFC 3986 §5.2.2 network-path reference: a reference beginning "//" carries its OWN
    // authority, so only the base's SCHEME survives. Ticket #1990 (SR-AUD-144); before the
    // repair "//other.example/c" was merged under the base authority and produced
    // "http://example.com//other.example/c", a URI pointing at the wrong host.
    if (relativeUri.rfind("//", 0) == 0) {
        parse(baseUri.scheme_ + ':' + relativeUri);
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

    // RFC 3986 §5.2.2, the R.path == "" case: a reference that is ONLY a query and/or a
    // fragment keeps the base's path untouched, and a fragment-only reference additionally
    // keeps the base's query. Ticket #1990 (SR-AUD-144). Before the repair both shapes ran
    // through the path merge below with an EMPTY relative path, which truncated the base
    // path at its last '/': for base "http://example.com/a/b?old#old", "?new" produced
    // "http://example.com/a/?new" and "#new" produced "http://example.com/a/#new", losing
    // the "b" segment in both and the old query in the second.
    if (relativePath.empty()) {
        std::string combined = baseAuthority() + baseUri.path_;
        if (relativeTail.rfind('#', 0) == 0)
            combined += baseUri.query_;
        combined += relativeTail;
        parse(combined);
        return;
    }

    std::string mergedPath;
    if (relativePath[0] == '/') {
        mergedPath = relativePath;
    } else {
        // RFC 3986 §5.3 merge: base path truncated up to and including its last '/'.
        auto lastSlash = baseUri.path_.rfind('/');
        std::string basePrefix = (lastSlash == std::string::npos) ? "/" : baseUri.path_.substr(0, lastSlash + 1);
        mergedPath = basePrefix + relativePath;
    }
    mergedPath = removeDotSegments(mergedPath);

    parse(baseAuthority() + mergedPath + relativeTail);
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
    // Verified against Uri.cs's loopback detection (DomainNameHelper.ParseCanonicalName matches
    // "localhost"/"loopback" via StringComparison.OrdinalIgnoreCase; IPv6AddressHelper
    // recognizes "::1"). The previous `host_ == "::1"` comparison could never match: host_
    // retains its surrounding brackets for a parsed IPv6 literal (e.g. "[::1]", matching .NET's
    // own bracketed Host property for IPv6 -- see parse()'s authority-splitting logic above), so
    // this branch was unreachable dead code. Also made the "localhost" comparison
    // case-insensitive and added the "loopback" alias, matching .NET's actual comparison.
    // (Broader IPv4 127.0.0.0/8 recognition -- .NET treats any address with first octet 127 as
    // loopback, not just 127.0.0.1 -- is intentionally left as a known gap rather than a naive
    // prefix check, which would risk misclassifying a domain name like
    // "127.0.0.1.example.com" as loopback.)
    std::string lowerHost = host_;
    std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowerHost == "localhost" || lowerHost == "loopback" ||
           lowerHost == "127.0.0.1" || lowerHost == "[::1]";
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
