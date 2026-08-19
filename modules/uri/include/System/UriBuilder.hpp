// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Uri.hpp"
#include "System/UriFormatException.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a custom constructor for uniform resource identifiers (URIs)
     * and modifies URIs for the System::Uri class.
     *
     * Partial C++ counterpart of .NET System.UriBuilder.
     * Allows building or modifying URI components individually before constructing
     * a final System::Uri.
     */
    class UriBuilder {
        /**
         * @brief `ToLowerInvariant` over ASCII only. Ticket #1996 group G-2.
         *
         * Invariant means invariant: `std::tolower` consults the global C locale, so a process
         * that installed a Turkish one would fold `I` to a dotless `i` and change the scheme.
         * This repository has removed that hazard from `CharUnicodeInfo` already (#2316).
         *
         * HONEST NOTE: a mutation replacing this with `std::tolower` is NOT caught, and it is an
         * equivalence **in the "C" locale** rather than a gap -- which is the locale the test
         * binary runs in, so the two agree on every byte there. Distinguishing them needs the
         * global locale changed inside a shared binary, which #2174 considered and declined for
         * exactly this reason. The explicit fold is kept because the guarantee is about what
         * happens when a process does install another locale, and that is not something a test
         * in this binary can observe.
         */
        static std::string toLowerAsciiInvariant(std::string text) {
            for (char& c : text) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
            return text;
        }

        std::string scheme_   = "http";
        std::string host_     = "localhost";
        intcs       port_     = -1;
        std::string path_     = "/";
        std::string query_;
        std::string fragment_;
        std::string userName_;
        std::string password_;

        void setFieldsFromUri(const Uri& u) {
            scheme_   = u.getSchemeProperty();
            host_     = u.getHostProperty();
            port_     = u.getPortProperty();
            path_     = u.getAbsolutePathProperty();
            query_    = u.getQueryProperty();
            fragment_ = u.getFragmentProperty();
            // Ticket #1993 (SR-AUD-138): the whole user-info used to land in userName_ with
            // password_ never populated, although this type publishes UserName and Password
            // as separate properties. Copying "http://user:pass@example.com/path" therefore
            // reported UserName == "user:pass" and Password == "", and a later
            // setPasswordProperty("replacement") serialised
            // "http://user:pass:replacement@example.com:80/path" -- the credentials the
            // caller replaced were still in the URI. The split is at the FIRST colon,
            // matching .NET's UriBuilder.SetFieldsFromUri.
            const std::string& info = u.getUserInfoProperty();
            const auto colon = info.find(':');
            if (colon == std::string::npos) {
                userName_ = info;
                password_.clear();
            } else {
                userName_ = info.substr(0, colon);
                password_ = info.substr(colon + 1);
            }
        }

    public:
        /** @brief Initializes a new empty UriBuilder instance. */
        UriBuilder() = default;

        /**
         * @brief Initializes a new UriBuilder with the specified URI string.
         * @param uri URI string to parse into components.
         */
        explicit UriBuilder(const std::string& uri) {
            // Ticket #1996 group G-4, landed 2026-08-19. `UriBuilder.cs:29-40`:
            //     _uri = new Uri(uri, UriKind.RelativeOrAbsolute);
            //     if (!_uri.IsAbsoluteUri)
            //         _uri = new Uri(Uri.UriSchemeHttp + Uri.SchemeDelimiter + uri);
            //     SetFieldsFromUri();
            // and .NET's own comment above it says why: "setting allowRelative=true for a string
            // like www.acme.org".
            //
            // Before this, `UriBuilder("www.example.com/path")` rendered `:///www.example.com/path`
            // -- an unparseable string with an empty scheme and empty host.
            //
            // THE TICKET'S SUMMARY IS WRONG ABOUT THE HOST. §14.2 says the string is promoted "to
            // the default `http` scheme and `localhost` host"; the reference prefixes `http://`
            // and REPARSES, so the host comes out of the string itself --
            // `www.example.com/path` yields host `www.example.com`, not `localhost`. A localhost
            // host appears only where the string supplies none, which is the pre-existing
            // default-field behaviour and not this promotion.
            Uri parsed(uri, UriKind::RelativeOrAbsolute);
            if (!parsed.getIsAbsoluteUriProperty()) {
                setFieldsFromUri(Uri("http://" + uri));
                return;
            }
            setFieldsFromUri(parsed);
        }

        /**
         * @brief Initializes a new UriBuilder from an existing Uri.
         * @param uri Source Uri to copy components from.
         */
        explicit UriBuilder(const Uri& u) {
            setFieldsFromUri(u);
        }

        /**
         * @brief Initializes a new UriBuilder with the specified scheme and host.
         * C++ counterpart of .NET UriBuilder(string, string).
         */
        UriBuilder(const std::string& schemeName, const std::string& hostName)
            : scheme_(schemeName), host_(hostName) {}

        /**
         * @brief Initializes a new UriBuilder with the specified scheme, host, and port.
         * C++ counterpart of .NET UriBuilder(string, string, int).
         */
        UriBuilder(const std::string& schemeName, const std::string& hostName, intcs portNumber)
            : scheme_(schemeName), host_(hostName) {
            setPortProperty(portNumber);
        }

        /**
         * @brief Initializes a new UriBuilder with the specified scheme, host, port, and path.
         * C++ counterpart of .NET UriBuilder(string, string, int, string).
         */
        UriBuilder(const std::string& schemeName, const std::string& hostName, intcs portNumber,
                   const std::string& pathValue)
            : scheme_(schemeName), host_(hostName) {
            setPortProperty(portNumber);
            setPathProperty(pathValue);
        }

        /**
         * @brief Initializes a new UriBuilder with the specified scheme, host, port, path,
         * and an extra query-or-fragment value.
         *
         * C++ counterpart of .NET UriBuilder(string, string, int, string, string).
         * @param extraValue A string starting with '?' (query) or '#' (fragment).
         * @throws System::ArgumentException if @p extraValue is non-empty and starts with
         *         neither '?' nor '#'.
         */
        UriBuilder(const std::string& schemeName, const std::string& hostName, intcs portNumber,
                   const std::string& pathValue, const std::string& extraValue)
            : scheme_(schemeName), host_(hostName) {
            setPortProperty(portNumber);
            setPathProperty(pathValue);
            if (!extraValue.empty()) {
                if (extraValue[0] == '#') {
                    fragment_ = extraValue.size() > 1 ? extraValue : std::string();
                } else if (extraValue[0] == '?') {
                    auto fragPos = extraValue.find('#');
                    if (fragPos == std::string::npos) {
                        query_ = extraValue.size() > 1 ? extraValue : std::string();
                    } else {
                        std::string q = extraValue.substr(0, fragPos);
                        std::string f = extraValue.substr(fragPos);
                        query_    = q.size() > 1 ? q : std::string();
                        fragment_ = f.size() > 1 ? f : std::string();
                    }
                } else {
                    throw System::ArgumentException("extraValue must start with '?' or '#'.");
                }
            }
        }

        // -----------------------------------------------------------------------
        // Property getters / setters
        // -----------------------------------------------------------------------

        /** @brief Returns the scheme component. */
        [[nodiscard]] const std::string& getSchemeProperty()   const noexcept { return scheme_; }
        /** @brief Sets the scheme component. */
        /**
         * @brief Sets the scheme, lower-casing it. Ticket #1996 group G-2.
         *
         * `UriBuilder.Scheme`'s setter ends in `value = value.ToLowerInvariant();`
         * (`UriBuilder.cs:180`), so `setSchemeProperty("HTTP")` renders `http://…` where it used
         * to render `HTTP://…`. Invariant, not locale-aware: an ASCII fold, so a Turkish locale
         * cannot turn `I` into a dotless one.
         *
         * @note <b>The validation is group G-3, landed 2026-08-19</b>, and it is transcribed
         *       whole rather than summarised. `UriBuilder.cs:108-134` is:
         * @code
         * if (value.Length != 0)
         * {
         *     if (!Uri.CheckSchemeName(value))
         *     {
         *         int index = value.IndexOf(':');
         *         if (index != -1) value = value.Substring(0, index);
         *         if (!Uri.CheckSchemeName(value))
         *             throw new ArgumentException(SR.net_uri_BadScheme, nameof(value));
         *     }
         *     value = value.ToLowerInvariant();
         * }
         * @endcode
         *
         * @note <b>The truncate-at-colon retry is the half the ticket's summary omitted.</b>
         *       §14.2 says only "throw for an invalid one", which would reject `"http:"` --
         *       .NET <i>accepts</i> it and stores `http`, because a scheme that fails the check
         *       is retried after cutting at the first `:`. `"http://"` is accepted the same way.
         *       Only text that is still not a scheme after truncation is refused.
         *
         * @note An <b>empty</b> scheme is accepted and stored empty; the whole block is guarded
         *       on `value.Length != 0`.
         *
         * @param v The scheme, optionally with a trailing `:` or `://`.
         * @throws System::ArgumentException if @p v is non-empty and is not a valid scheme even
         *         after truncation at the first `:` (parameter name `value`, as .NET's
         *         `nameof(value)`).
         */
        void setSchemeProperty(const std::string& v) {
            std::string value = v;
            if (!value.empty()) {
                if (!Uri::CheckSchemeName(value)) {
                    const std::size_t index = value.find(':');
                    if (index != std::string::npos) value = value.substr(0, index);
                    if (!Uri::CheckSchemeName(value)) {
                        throw System::ArgumentException(
                            "Invalid URI: The URI scheme is not valid.", "value");
                    }
                }
                value = toLowerAsciiInvariant(value);
            }
            scheme_ = value;
        }

        /** @brief Returns the host component. */
        [[nodiscard]] const std::string& getHostProperty()     const noexcept { return host_; }
        /** @brief Sets the host component. */
        /**
         * @brief Sets the host, bracketing an IPv6 literal. Ticket #1996 group G-1.
         *
         * `UriBuilder.Host`'s setter wraps a value containing `:` in `[...]` unless it is already
         * bracketed (`UriBuilder.cs:167-197`), so `setHostProperty("::1")` renders
         * `http://[::1]/` where it used to render the unparseable `http://::1/`.
         *
         * The trigger is .NET's own: the value must contain one of `s_hostReservedChars`,
         * `":/\?#@[]"` (`:164`), and then a `:` specifically. A plain DNS name touches none of
         * them and is stored unchanged.
         *
         * @note <b>The rejection in the same block is deliberately NOT taken.</b> .NET also
         *       throws `ArgumentException(net_uri_BadHostName)` for a bracketed value whose
         *       inside holds a reserved character other than `:`, and for any value with a
         *       reserved character but no `:` -- "contoso.com/path", "user@contoso.com". Those
         *       make a setter that never threw start throwing, which #1996's own note reserves
         *       for a group this one is not. Only the bracketing lands here.
         */
        void setHostProperty(const std::string& v) {
            // Nothing to bracket.
            if (v.find(':') == std::string::npos) { host_ = v; return; }
            // ALREADY CARRIES A BRACKET: left exactly as given, and this is a deliberate
            // divergence forced by taking G-1 without G-3's rejection. .NET wraps first and
            // THEN throws for a half-bracketed value -- "[::1" becomes "[[::1]" and is refused
            // because the inside holds a '[' (UriBuilder.cs:178-187). Without that throw the
            // wrap would leave the nonsense "[[::1]" stored, turning a value this port's Uri
            // already refuses (#1991) into one it might not. Leaving it untouched keeps that
            // refusal exactly where it was.
            if (v.find('[') != std::string::npos || v.find(']') != std::string::npos) {
                host_ = v;
                return;
            }
            host_ = "[" + v + "]";
        }

        /** @brief Returns the port number, or -1 if not set. */
        [[nodiscard]] intcs getPortProperty()                   const noexcept { return port_; }
        /**
         * @brief Sets the port number.
         * C++ counterpart of .NET UriBuilder.Port.
         * @throws ArgumentOutOfRangeException if @p v is less than -1 or greater than 65535.
         */
        void setPortProperty(intcs v) {
            if (v < -1 || v > 0xFFFF)
                throw ArgumentOutOfRangeException("value");
            port_ = v;
        }

        /** @brief Returns the path component. */
        [[nodiscard]] const std::string& getPathProperty()     const noexcept { return path_; }
        /** @brief Sets the path component. Empty/null values normalize to "/". */
        void setPathProperty(const std::string& v)                            { path_ = v.empty() ? "/" : v; }

        /** @brief Returns the query string (includes leading '?' if non-empty). */
        [[nodiscard]] const std::string& getQueryProperty()    const noexcept { return query_; }
        /**
         * @brief Sets the query string (with or without leading '?').
         *
         * C++ counterpart of .NET UriBuilder.Query. Real .NET's setter normalizes by
         * prepending '?' when the supplied value is non-empty and doesn't already start with
         * one, so a subsequent getQueryProperty() always returns the '?'-prefixed form --
         * this port previously stored the raw value verbatim, so
         * setQueryProperty("foo=bar")/getQueryProperty() round-tripped to "foo=bar" instead of
         * "?foo=bar" (confirmed via a standalone repro before fixing). ToString() itself was
         * unaffected since it already compensated by conditionally prepending '?' at render
         * time, but the property getter's return value diverged from .NET's actual contract.
         */
        void setQueryProperty(const std::string& v) {
            query_ = (!v.empty() && v[0] != '?') ? '?' + v : v;
        }

        /** @brief Returns the fragment (includes leading '#' if non-empty). */
        [[nodiscard]] const std::string& getFragmentProperty() const noexcept { return fragment_; }
        /**
         * @brief Sets the fragment (with or without leading '#').
         *
         * C++ counterpart of .NET UriBuilder.Fragment. Same normalize-on-set fix as
         * setQueryProperty() above, mirroring real .NET's Fragment setter.
         */
        void setFragmentProperty(const std::string& v) {
            fragment_ = (!v.empty() && v[0] != '#') ? '#' + v : v;
        }

        /** @brief Returns the user-name component of the user-info. */
        [[nodiscard]] const std::string& getUserNameProperty() const noexcept { return userName_; }
        /** @brief Sets the user-name component. */
        void setUserNameProperty(const std::string& v)                        { userName_ = v; }

        /** @brief Returns the password component of the user-info. */
        [[nodiscard]] const std::string& getPasswordProperty() const noexcept { return password_; }
        /** @brief Sets the password component. */
        void setPasswordProperty(const std::string& v)                        { password_ = v; }

        // -----------------------------------------------------------------------
        // Conversion
        // -----------------------------------------------------------------------

        /**
         * @brief Builds and returns the URI string from current components.
         * @throws UriFormatException if UserName is empty but Password is not,
         *         matching .NET's UriBuilder.ToString().
         */
        [[nodiscard]] std::string ToString() const {
            if (userName_.empty() && !password_.empty())
                throw UriFormatException("The format of the UserInfo is invalid, username can't be empty when password is not empty.");
            std::string result = scheme_ + "://";
            if (!userName_.empty()) {
                result += userName_;
                if (!password_.empty()) result += ':' + password_;
                result += '@';
            }
            result += host_;
            if (port_ >= 0) result += ':' + std::to_string(port_);
            if (!path_.empty() && path_[0] != '/') result += '/';
            result += path_;
            if (!query_.empty()) {
                if (query_[0] != '?') result += '?';
                result += query_;
            }
            if (!fragment_.empty()) {
                if (fragment_[0] != '#') result += '#';
                result += fragment_;
            }
            return result;
        }

        /**
         * @brief Constructs and returns a Uri from the current components.
         * @throws System::UriFormatException if the resulting string is not a valid URI.
         */
        [[nodiscard]] Uri getUriProperty() const { return Uri(ToString()); }

        /**
         * @brief Returns true if @p other builds an equal URI.
         *
         * C++ counterpart of .NET `UriBuilder.Equals(object)`, which is
         * `rparam is not null && Uri.Equals(rparam.ToString())` (`UriBuilder.cs:277`).
         *
         * **Ticket #2391 (2026-08-19) adopted .NET's delegation and, in doing so, WITHDREW the
         * non-throwing guarantee ticket #2004 gave this pair.** The reasoning is recorded because
         * the trade was stated before the decision and must not be quietly softened:
         *
         * - .NET compares **canonically**, through a built `Uri`, not by rendered text. Two
         *   builders differing only in scheme case, or in a default port written explicitly, are
         *   equal in .NET and were **not** equal here.
         * - The price is that a builder whose rendering does not parse now **throws** from
         *   `Equals` -- including `b.Equals(b)` -- because the `Uri` property throws. That is
         *   .NET's behaviour too. The four routes are ordinary setters:
         *     `setHostProperty("h:abc")`, `("h:99999")`, `("[::1")` and `("")`.
         *
         * @note **The asymmetry is .NET's and is deliberate.** *This* builder is built through the
         * `Uri` property, so an unparseable *self* throws; the *other* is compared as a **string**,
         * and `Uri.Equals(string)` runs `TryCreate(s, UriKind.RelativeOrAbsolute, out _)` and
         * **returns false** when it fails (`Uri.cs`, the `comparand is string` branch). So an
         * unparseable *other* is merely unequal. Reproducing only one half would be tidier and
         * would not be .NET.
         *
         * @note `UriKind::RelativeOrAbsolute`, not `Absolute` -- .NET's comparand branch uses it,
         * so a relative other is parsed rather than rejected.
         *
         * @throws System::UriFormatException if *this* builder's rendering is not a valid URI, or
         *         if `ToString()` itself throws (empty UserName with a non-empty Password).
         */
        [[nodiscard]] bool Equals(const UriBuilder& other) const {
            const Uri self = getUriProperty();
            std::shared_ptr<Uri> parsed;
            if (!Uri::TryCreate(other.ToString(), UriKind::RelativeOrAbsolute, parsed) || !parsed) {
                return false;
            }
            return self == *parsed;
        }

        /**
         * @brief Returns a hash code for the built URI.
         *
         * C++ counterpart of .NET `UriBuilder.GetHashCode()`, which is `Uri.GetHashCode()`
         * (`UriBuilder.cs:279`).
         *
         * **Ticket #2391 restored this delegation, reversing ticket #2004.** #2004 removed it for
         * a real reason -- the pair disagreed at the worst possible place, `b.Equals(b)` returning
         * `true` while `b.GetHashCode()` threw -- and the repair it chose was to make *hashing*
         * stop parsing. #2391 closes the same gap from the other side: **both** members now go
         * through the built `Uri`, so they are total on exactly the same set of objects and the
         * `Equals => same hash` implication holds over the whole of that set. What changed is
         * which set: it is now the parseable builders rather than all of them.
         *
         * @note **#2004's stated justification became false earlier the same session and this is
         * the correction.** Its doc-comment argued that hashing the rendered string was
         * *value-identical* to delegating, because "`Uri::parse` assigns `absoluteUri_ = uriString`
         * on every branch it accepts, and `Uri::GetHashCode` hashes exactly that". **Ticket #1995
         * made `Uri::GetHashCode` hash the canonical identity key instead** -- folded scheme,
         * folded host, resolved default port, path and query, with fragment and user-info excluded
         * -- so the two stopped agreeing. Keeping #2004 would therefore have meant a builder and
         * the `Uri` it builds hashing differently, which is precisely the defect #2004 existed to
         * prevent, one level up.
         *
         * @throws System::UriFormatException under exactly the same conditions as Equals().
         */
        [[nodiscard]] intcs GetHashCode() const {
            return getUriProperty().GetHashCode();
        }
    };

} // namespace System
