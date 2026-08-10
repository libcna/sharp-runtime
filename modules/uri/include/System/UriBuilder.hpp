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
            setFieldsFromUri(Uri(uri));
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
        void setSchemeProperty(const std::string& v)                          { scheme_ = v; }

        /** @brief Returns the host component. */
        [[nodiscard]] const std::string& getHostProperty()     const noexcept { return host_; }
        /** @brief Sets the host component. */
        void setHostProperty(const std::string& v)                            { host_ = v; }

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
         * @brief Returns true if @p other builds an equal URI string.
         * C++ counterpart of .NET UriBuilder.Equals(object).
         */
        [[nodiscard]] bool Equals(const UriBuilder& other) const {
            return ToString() == other.ToString();
        }

        /**
         * @brief Returns a hash code based on the built URI string.
         *
         * C++ counterpart of .NET UriBuilder.GetHashCode().
         *
         * Ticket #2004: this used to be `getUriProperty().GetHashCode()`, which built a whole
         * `Uri` from `ToString()` purely to hash it. Equals() compares the rendered strings
         * directly and never parses, so for any builder whose rendering is not a parseable
         * URI the two disagreed at the worst possible place: `b.Equals(b)` returned `true`
         * while `b.GetHashCode()` **threw**, leaving an object that compares equal to itself
         * with no obtainable hash. Measured routes, all through ordinary setters:
         *   setHostProperty("h:abc")  -> "http://h:abc/"  -> throws (invalid port)
         *   setHostProperty("h:99999")-> "http://h:99999/"-> throws (port out of range)
         *   setHostProperty("[::1")   -> "http://[::1/"   -> throws (invalid IP literal)
         *   setHostProperty("")       -> "http:///"       -> throws (no host, #2000)
         *
         * Hashing the rendered string directly is **value-identical** wherever the old code
         * returned at all: `Uri::parse` assigns `absoluteUri_ = uriString` on every branch it
         * accepts, and `Uri::GetHashCode` hashes exactly that, so
         * `Uri(s).GetHashCode() == std::hash<std::string>{}(s)` for every `s` a `Uri` accepts.
         * The only change is that the throw is gone, which is what restores
         * `a.Equals(b) => a.GetHashCode() == b.GetHashCode()` for the whole domain of
         * Equals(). Identity itself is unchanged and stays raw rendered text — narrowing it
         * to URI semantics is SR-AUD-142/SR-AUD-140, approval-gated as ticket #1995.
         *
         * @throws System::UriFormatException only where ToString() itself throws, i.e. when
         *         UserName is empty and Password is not — the same condition, and the same
         *         exception, that Equals() raises. The two are now total on exactly the same
         *         set of objects.
         */
        [[nodiscard]] intcs GetHashCode() const {
            return static_cast<intcs>(std::hash<std::string>{}(ToString()));
        }
    };

} // namespace System
