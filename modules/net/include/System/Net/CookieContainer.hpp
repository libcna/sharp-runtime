// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieCollection.hpp"
#include "System/Uri.hpp"

namespace System::Net {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a container for a collection of Cookie objects, associates them by
     * domain/path, and generates the Cookie request header / consumes the Set-Cookie
     * response header for a given request URI.
     *
     * C++ counterpart of .NET System.Net.CookieContainer. Domain-match and path-match follow
     * RFC 6265 (a cookie's domain matches the request host if equal, or if the host ends with
     * "." + domain; a cookie's path matches the request path if equal, a prefix followed by
     * '/', or the cookie path is "/"). Storage is a flat vector scanned linearly on every
     * lookup -- adequate for the handful-of-cookies-per-session a game client accumulates;
     * real .NET's server-oriented hash-table-of-domains implementation is not replicated.
     * Cookie aging/eviction policies (MaxCookieSize, PerDomainCapacity, Capacity) are not
     * enforced -- this container never evicts cookies on its own.
     *
     * <b>Origin check (SR-AUD-305, ticket #2040, landed 2026-08-17).</b> An explicitly supplied
     * `Domain` is validated against the URI the cookie arrives from, and a cookie whose domain
     * does not domain-match that host is rejected with `CookieException` and not stored --
     * transcribed from `Cookie.VerifyAndSetDefaults` (`Cookie.cs:358-424`). A `Domain` the
     * caller did not supply is defaulted from the URI's host and is never validated, because the
     * host it is being set to is by construction its own origin. One rule serves both the
     * validation and the emission match, as it does in .NET.
     *
     * @warning KNOWN GAP, measured and pinned, not yet repaired (SR-AUD-308, ticket #2042,
     * blocked on approval -- plan §14.2). Storage is **unbounded in every direction**: no
     * capacity, no per-domain capacity, no maximum cookie size, and **no expiry cleanup** -- an
     * expired cookie is retained and only hidden from emission, so `Count` keeps growing.
     * Pinned by `NetGatedBehaviourPinTests.Pin2042_*`.
     */
    class CookieContainer {
    public:
        CookieContainer() = default;

        /**
         * @brief Adds a Cookie for the given URI, applying the URI's host as the cookie's domain
         * if unset.
         * An explicitly supplied `Domain` must domain-match @p uri's host; one that does not is
         * rejected rather than stored. A `Path` or `Domain` the caller supplied — through a
         * setter **or** through a constructor, which agree since #2040 — is kept as given and
         * never replaced by @p uri's.
         *
         * @throws System::Net::CookieException if @p cookie carries an explicit `Domain` that is
         *         not a valid domain name or does not domain-match @p uri's host.
         */
        void Add(const System::Uri& uri, const Cookie& cookie);

        /** @brief Adds every Cookie in the collection for the given URI. */
        void Add(const System::Uri& uri, const CookieCollection& cookies);

        /**
         * @brief Parses one Set-Cookie response header value and stores the resulting cookie(s),
         * applying @p uri as the default domain/path when the header doesn't specify them.
         * @throws CookieException if @p cookieHeader has no "Name=Value" pair.
         */
        void SetCookies(const System::Uri& uri, const std::string& cookieHeader);

        /**
         * @brief Returns the value to send in a Cookie request header for @p uri: all stored,
         * non-expired cookies whose domain/path match, joined as "Name1=Value1; Name2=Value2".
         * Cookies marked Secure are only included when @p uri's scheme is "https". Returns an
         * empty string if no cookies match.
         */
        [[nodiscard]] std::string GetCookieHeader(const System::Uri& uri) const;

        /** @brief Returns the CookieCollection of cookies that match @p uri (same filtering as GetCookieHeader). */
        [[nodiscard]] CookieCollection GetCookies(const System::Uri& uri) const;

        /** @brief Gets the total number of cookies currently stored (including expired ones not yet purged). */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(cookies_.size()); }

    private:
        static bool domainMatches(const std::string& cookieDomain, const std::string& host);
        static bool pathMatches(const std::string& cookiePath, const std::string& requestPath);
        static std::string toLowerAscii(std::string s);

        std::vector<Cookie> cookies_;
    };

} // namespace System::Net
