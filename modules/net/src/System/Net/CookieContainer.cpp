// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/CookieContainer.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/CookieException.hpp"
#include "System/Net/IPAddress.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace System::Net {

std::string CookieContainer::toLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

namespace {

// Cookie.IsValidDomainName (`Cookie.cs:489-491`): non-empty, and built only from the domain
// character set. .NET's s_domainChars is the ASCII letters, digits, '.', '-' and '_', which also
// covers the leading dot a Domain attribute may carry.
bool isValidDomainName(const std::string& domain) {
    if (domain.empty()) return false;
    for (const char c : domain) {
        const unsigned char u = static_cast<unsigned char>(c);
        const bool ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') ||
                        c == '.' || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

} // namespace

// RFC 6265 5.1.3 domain-match, transcribed from Cookie.HostMatchesDomain (`Cookie.cs:331-350`)
// so that ONE rule serves both jobs it has: deciding whether an explicitly supplied Domain may
// be accepted for a given request URI (CookieContainer::Add) and deciding whether a stored
// cookie may be emitted for one (GetCookies). .NET uses the single function for both, and two
// functions that were meant to agree is the shape this repository keeps having to repair.
//
// A host domain-matches a domain when either:
//   - they are identical after stripping the domain's optional leading dot; or
//   - the domain is a suffix of the host, the character of the host immediately before it is a
//     '.', the DOMAIN ITSELF CONTAINS A DOT, and the HOST IS NOT AN IP LITERAL.
//
// Ticket #2040 added the last two conditions, which .NET states as compatibility rules on top
// of RFC 6265: a single-label domain ("com", "localhost") and an IP-literal host both require an
// exact match, so `Domain=com` cannot be attached to `example.com` and `Domain=1.2.3` cannot be
// attached to `1.2.3.4`. Both are narrowings and both are .NET's.
bool CookieContainer::domainMatches(const std::string& cookieDomain, const std::string& host) {
    if (cookieDomain.empty()) return true; // host-only cookie stored without an explicit domain
    std::string domain = toLowerAscii(cookieDomain);
    std::string h      = toLowerAscii(host);
    if (!domain.empty() && domain[0] == '.') domain = domain.substr(1);
    if (domain.empty()) return false;
    if (h == domain) return true;
    if (h.size() <= domain.size()) return false;
    if (h.compare(h.size() - domain.size(), domain.size(), domain) != 0) return false;
    if (h[h.size() - domain.size() - 1] != '.') return false;
    if (domain.find('.') == std::string::npos) return false;   // single-label: exact match only
    System::Net::IPAddress parsed;
    if (System::Net::IPAddress::TryParse(host, parsed)) return false;  // IP literal: exact only
    return true;
}

// RFC 6265 5.1.4 path-match: identical, a proper "/"-terminated prefix, or the cookie path is "/".
bool CookieContainer::pathMatches(const std::string& cookiePath, const std::string& requestPath) {
    std::string cp = cookiePath.empty() ? "/" : cookiePath;
    if (requestPath == cp) return true;
    if (requestPath.size() > cp.size() && requestPath.compare(0, cp.size(), cp) == 0) {
        if (cp.back() == '/') return true;
        if (requestPath[cp.size()] == '/') return true;
    }
    return false;
}

// Ticket #2040 / SR-AUD-305 and SR-AUD-306, which are one decision because the implicit flags
// are the INPUT to the domain rule. Transcribed from Cookie.VerifyAndSetDefaults
// (`Cookie.cs:358-424`), whose domain half is exactly:
//
//     if (m_domain_implicit) { SetDomainAndKey(host); }
//     else if (!IsValidDomainName(m_domainKey) || !HostMatchesDomain(host, m_domainKey))
//         throw new CookieException(SR.Format(SR.net_cookie_attribute, "Domain", m_domain));
//
// Before this ticket there was no origin check at all: a cookie added from origin.invalid
// carrying Domain=.unrelated.invalid was stored and later handed to unrelated.invalid by
// GetCookieHeader. Note the ORDER, which is .NET's and matters: the implicit case is defaulted
// and never validated, because the host it is being set to is by construction its own origin.
void CookieContainer::Add(const System::Uri& uri, const Cookie& cookie) {
    Cookie stored = cookie;
    const std::string host = uri.getHostProperty();

    if (stored.getDomainImplicitProperty()) {
        // applyOriginDomain rather than the setter: .NET's SetDomainAndKey leaves
        // m_domain_implicit alone, because the flag records where the value came from.
        stored.applyOriginDomain(host);
    } else if (!isValidDomainName(stored.getDomainProperty()) ||
               !domainMatches(stored.getDomainProperty(), host)) {
        throw System::Net::CookieException("The 'Domain'='" + stored.getDomainProperty() +
                                           "' part of the cookie is invalid.");
    }

    if (stored.getPathImplicitProperty()) stored.applyOriginPath(uri.getAbsolutePathProperty());

    // #2042: the size limit bounds the VALUE alone, not the whole cookie, and it REPORTS rather
    // than evicting -- CookieContainer.cs:235-237, message from Strings.resx:87-89. It runs
    // before anything is stored, and before the capacity checks, exactly as .NET's does.
    if (static_cast<intcs>(stored.getValueProperty().size()) > maxCookieSize_)
        throw System::Net::CookieException(
            "The value size of the cookie is '" +
            std::to_string(stored.getValueProperty().size()) +
            "'. This exceeds the configured maximum size, which is '" +
            std::to_string(maxCookieSize_) + "'.");

    // Replace an existing cookie with the same Name/Domain/Path (matches real .NET's
    // "adding a cookie with the same identity overwrites/refreshes it" semantics).
    // A replacement consumes no new slot, so it precedes the capacity check as it does in .NET,
    // where InternalAdd's `m_count += cookies.InternalAdd(cookie, true)` adds zero for an
    // overwrite.
    for (auto& existing : cookies_) {
        if (existing.getNameProperty() == stored.getNameProperty() &&
            existing.getDomainProperty() == stored.getDomainProperty() &&
            existing.getPathProperty() == stored.getPathProperty()) {
            existing = stored;
            return;
        }
    }

    // #2042: an EXPIRED cookie is an explicit removal command, not an insertion
    // (CookieContainer.cs:263-275) -- so it must not be stored, and must not evict anything.
    if (stored.getExpiredProperty()) return;

    const std::string domain = stored.getDomainProperty();
    const auto inDomain = static_cast<intcs>(std::count_if(cookies_.begin(), cookies_.end(),
        [&domain](const Cookie& c) { return c.getDomainProperty() == domain; }));
    if ((inDomain >= perDomainCapacity_ || static_cast<intcs>(cookies_.size()) >= capacity_)
        && !ageCookies(domain))
        return;  // Cannot age: reject the new cookie, silently, as .NET does.

    cookies_.push_back(stored);
}


// ---------------------------------------------------------------------------
// #2042 (SR-AUD-308) -- capacity, aging and eviction.
//
// The three limits are .NET's own constants, so nothing here is a number somebody chose:
// DefaultCookieLimit = 300, DefaultPerDomainCookieLimit = 20, DefaultCookieLengthLimit = 4096
// (CookieContainer.cs:69-71). The ticket was gated on "every bound is a number somebody must
// choose" AND on ".NET's exact default capacities cannot be established here"; the reference
// establishes them, which dissolves both halves at once.
// ---------------------------------------------------------------------------

void CookieContainer::setCapacityProperty(intcs value) {
    // CookieContainer.cs:117-119. The lower bound is PerDomainCapacity, not zero, because a
    // total below the per-domain limit is unsatisfiable.
    if (value <= 0 || value < perDomainCapacity_)
        throw System::ArgumentOutOfRangeException(
            "value", "'Capacity' has to be greater than '0' and less than '" +
                         std::to_string(perDomainCapacity_) + "'.");
    capacity_ = value;
    // .NET ages immediately when the new value is smaller (`:121-124`), rather than waiting for
    // the next Add -- otherwise Count could exceed a limit the caller has already set.
    while (static_cast<intcs>(cookies_.size()) > capacity_) {
        if (purgeExpired() == 0) cookies_.erase(cookies_.begin());
    }
}

void CookieContainer::setPerDomainCapacityProperty(intcs value) {
    // CookieContainer.cs:163-172.
    if (value <= 0 || value > capacity_)
        throw System::ArgumentOutOfRangeException(
            "value", "'PerDomainCapacity' has to be greater than '0' and less than '" +
                         std::to_string(capacity_) + "'.");
    perDomainCapacity_ = value;
}

void CookieContainer::setMaxCookieSizeProperty(intcs value) {
    // CookieContainer.cs:147-151.
    if (value <= 0)
        throw System::ArgumentOutOfRangeException("value", "'MaxCookieSize' has to be greater than '0'.");
    maxCookieSize_ = value;
}

intcs CookieContainer::purgeExpired() {
    // .NET's ExpireCollection, applied container-wide. This is the half the FINDING DOES NOT
    // NAME and #2047's second pin recorded: there was no expiry cleanup at all, so an expired
    // cookie was retained and merely hidden from emission, and Count grew for ever.
    const auto before = cookies_.size();
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(),
                                  [](const Cookie& c) { return c.getExpiredProperty(); }),
                   cookies_.end());
    return static_cast<intcs>(before - cookies_.size());
}

bool CookieContainer::ageCookies(const std::string& domain) {
    // AgeCookies(domain) then AgeCookies(null), in .NET's order and with .NET's target:
    // min_count = min(domain_count * fraction, min(perDomain, capacity) - 1)  (:441).
    // With the container not over its total limit the fraction is 1, so a domain sitting at the
    // per-domain limit is cut to exactly one below it -- freeing one slot, not emptying it.
    purgeExpired();

    const auto countIn = [this](const std::string& d) {
        return static_cast<intcs>(std::count_if(cookies_.begin(), cookies_.end(),
            [&d](const Cookie& c) { return c.getDomainProperty() == d; }));
    };

    const intcs domainTarget = std::min(perDomainCapacity_, capacity_) - 1;
    while (countIn(domain) > domainTarget) {
        // Oldest first: vector order is insertion order. .NET removes cc.RemoveAt(0) from the
        // least-recently-used PATH COLLECTION; this container has no collections to time-stamp,
        // so it drops the oldest stored cookie of the domain. See the header note.
        auto oldest = std::find_if(cookies_.begin(), cookies_.end(),
            [&domain](const Cookie& c) { return c.getDomainProperty() == domain; });
        if (oldest == cookies_.end()) break;
        cookies_.erase(oldest);
    }

    while (static_cast<intcs>(cookies_.size()) >= capacity_) {
        if (cookies_.empty()) return false;
        cookies_.erase(cookies_.begin());
    }
    return countIn(domain) <= domainTarget;
}

void CookieContainer::Add(const System::Uri& uri, const CookieCollection& cookies) {
    for (const auto& c : cookies) Add(uri, c);
}

namespace {
    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }
}

void CookieContainer::SetCookies(const System::Uri& uri, const std::string& cookieHeader) {
    // Split on ';' into "attribute[=value]" segments. The first segment is always the
    // required Name=Value pair; subsequent segments are optional cookie attributes.
    std::vector<std::string> parts;
    {
        std::stringstream ss(cookieHeader);
        std::string part;
        while (std::getline(ss, part, ';')) parts.push_back(trim(part));
    }
    if (parts.empty() || parts[0].empty())
        throw CookieException("SetCookies: empty Set-Cookie header value.");

    size_t eq = parts[0].find('=');
    if (eq == std::string::npos)
        throw CookieException("SetCookies: missing '=' in cookie Name=Value pair: '" + parts[0] + "'.");

    Cookie cookie(trim(parts[0].substr(0, eq)), trim(parts[0].substr(eq + 1)));

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& attr = parts[i];
        if (attr.empty()) continue;
        size_t aeq = attr.find('=');
        std::string attrName  = toLowerAscii(trim(aeq == std::string::npos ? attr : attr.substr(0, aeq)));
        std::string attrValue = aeq == std::string::npos ? "" : trim(attr.substr(aeq + 1));

        if (attrName == "domain") {
            cookie.setDomainProperty(attrValue);
        } else if (attrName == "path") {
            cookie.setPathProperty(attrValue);
        } else if (attrName == "secure") {
            cookie.setSecureProperty(true);
        } else if (attrName == "httponly") {
            cookie.setHttpOnlyProperty(true);
        } else if (attrName == "expires") {
            DateTime parsed;
            if (DateTime::TryParse(attrValue, parsed)) cookie.setExpiresProperty(parsed);
        } else if (attrName == "max-age") {
            try {
                int seconds = std::stoi(attrValue);
                cookie.setExpiresProperty(
                    seconds <= 0 ? DateTime(1601, 1, 1) : DateTime::getNowProperty().AddSeconds(seconds));
            } catch (...) {
                // Malformed Max-Age: ignore the attribute, matching real .NET's lenient parser.
            }
        }
        // Version/Comment/CommentUri/Port/SameSite attributes are accepted but not modeled.
    }

    Add(uri, cookie);
}

std::string CookieContainer::GetCookieHeader(const System::Uri& uri) const {
    CookieCollection matching = GetCookies(uri);
    std::string result;
    for (intcs i = 0; i < matching.getCountProperty(); ++i) {
        if (!result.empty()) result += "; ";
        result += matching[i].ToString();
    }
    return result;
}

CookieCollection CookieContainer::GetCookies(const System::Uri& uri) const {
    CookieCollection result;
    bool isSecureRequest = toLowerAscii(uri.getSchemeProperty()) == "https";
    std::string path = uri.getAbsolutePathProperty();
    if (path.empty()) path = "/";

    for (const auto& c : cookies_) {
        if (c.getExpiredProperty()) continue;
        if (c.getSecureProperty() && !isSecureRequest) continue;
        if (!domainMatches(c.getDomainProperty(), uri.getHostProperty())) continue;
        if (!pathMatches(c.getPathProperty(), path)) continue;
        result.Add(c);
    }
    return result;
}

} // namespace System::Net
