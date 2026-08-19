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
     * Cookie aging and eviction ARE enforced since #2042, with .NET's own limits and its own
     * order; see the capacity note below.
     *
     * <b>Origin check (SR-AUD-305, ticket #2040, landed 2026-08-17).</b> An explicitly supplied
     * `Domain` is validated against the URI the cookie arrives from, and a cookie whose domain
     * does not domain-match that host is rejected with `CookieException` and not stored --
     * transcribed from `Cookie.VerifyAndSetDefaults` (`Cookie.cs:358-424`). A `Domain` the
     * caller did not supply is defaulted from the URI's host and is never validated, because the
     * host it is being set to is by construction its own origin. One rule serves both the
     * validation and the emission match, as it does in .NET.
     *
     * <b>Capacity, aging and eviction (SR-AUD-308, ticket #2042, landed 2026-08-19).</b>
     * Storage used to be unbounded in every direction, and an expired cookie was retained and
     * only hidden from emission, so `Count` grew without limit. The three limits are .NET's, and
     * they are **derived rather than chosen**: `DefaultCookieLimit = 300`,
     * `DefaultPerDomainCookieLimit = 20`, `DefaultCookieLengthLimit = 4096`
     * (`CookieContainer.cs:69-71`).
     *
     * `MaxCookieSize` bounds the cookie's **Value** alone — not the whole cookie — and a value
     * over it is rejected with `CookieException` (`CookieContainer.cs:235-237`), which is the
     * one limit that reports rather than evicting.
     *
     * The two capacities evict instead. On add, expired cookies are purged first; if that does
     * not free room, the oldest are dropped until the affected domain is at
     * `min(PerDomainCapacity, Capacity) - 1` and the container is below `Capacity`; if nothing
     * can be freed the new cookie is **silently rejected**, exactly as .NET's
     * `if (... && !AgeCookies(...)) return;` does.
     *
     * @note <b>One structural difference, stated rather than glossed.</b> .NET evicts from the
     * least-recently-used *path collection* of a domain, because its storage is a table of
     * domains each holding path collections with their own timestamps. This container is a flat
     * list with no collections to time-stamp, so it drops the oldest **stored** cookie in the
     * affected scope. Within a collection .NET does the same thing — `cc.RemoveAt(0)` is its
     * oldest entry — so the difference is only *which* domain loses a cookie when the total
     * limit binds, never whether the bound is enforced.
     */
    class CookieContainer {
    public:
        /** @brief .NET's `CookieContainer.DefaultCookieLimit` (`CookieContainer.cs:69`). */
        static constexpr intcs DefaultCookieLimit = 300;
        /** @brief .NET's `CookieContainer.DefaultPerDomainCookieLimit` (`:70`). */
        static constexpr intcs DefaultPerDomainCookieLimit = 20;
        /** @brief .NET's `CookieContainer.DefaultCookieLengthLimit` (`:71`). */
        static constexpr intcs DefaultCookieLengthLimit = 4096;

        CookieContainer() = default;

        /**
         * @brief Creates a container with the given total capacity.
         * @throws System::ArgumentOutOfRangeException if @p capacity is not positive.
         */
        explicit CookieContainer(intcs capacity) { setCapacityProperty(capacity); }

        /**
         * @brief Creates a container with all three limits set.
         * @throws System::ArgumentOutOfRangeException if any limit is out of range.
         */
        CookieContainer(intcs capacity, intcs perDomainCapacity, intcs maxCookieSize) {
            // The order is .NET's (CookieContainer.cs:88-106): per-domain first, so the
            // capacity setter's `value < perDomainCapacity` test sees the value just set.
            setPerDomainCapacityProperty(perDomainCapacity);
            setCapacityProperty(capacity);
            setMaxCookieSizeProperty(maxCookieSize);
        }

        /** @brief The maximum number of cookies this container holds. Default 300. */
        [[nodiscard]] intcs getCapacityProperty() const { return capacity_; }
        /**
         * @brief Sets the total capacity, evicting immediately if the new value is smaller.
         * @throws System::ArgumentOutOfRangeException if @p value is not positive, or is below
         *         `PerDomainCapacity` (`CookieContainer.cs:117-119`).
         */
        void setCapacityProperty(intcs value);

        /** @brief The maximum number of cookies per domain. Default 20. */
        [[nodiscard]] intcs getPerDomainCapacityProperty() const { return perDomainCapacity_; }
        /**
         * @brief Sets the per-domain capacity, evicting immediately if the new value is smaller.
         * @throws System::ArgumentOutOfRangeException if @p value is not positive or exceeds
         *         `Capacity` (`CookieContainer.cs:163-172`).
         */
        void setPerDomainCapacityProperty(intcs value);

        /** @brief The maximum length of a cookie's Value. Default 4096. */
        [[nodiscard]] intcs getMaxCookieSizeProperty() const { return maxCookieSize_; }
        /** @throws System::ArgumentOutOfRangeException if @p value is not positive. */
        void setMaxCookieSizeProperty(intcs value);

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

        // #2042. Insertion order IS vector order -- Add appends and an identity match is
        // replaced in place -- so "oldest" needs no extra timestamp. sizeof(CookieContainer)
        // grows by the three limits under SA-3; pinned by NetCookieCapacityTests.
        std::vector<Cookie> cookies_;
        intcs               capacity_          = DefaultCookieLimit;
        intcs               perDomainCapacity_ = DefaultPerDomainCookieLimit;
        intcs               maxCookieSize_     = DefaultCookieLengthLimit;

        /** @brief Removes every expired cookie. .NET's `ExpireCollection`, container-wide. */
        intcs purgeExpired();
        /** @brief Frees room for one more cookie in @p domain, or reports that it cannot. */
        bool ageCookies(const std::string& domain);
    };

} // namespace System::Net
