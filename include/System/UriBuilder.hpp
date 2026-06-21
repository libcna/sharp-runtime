// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Uri.hpp"

namespace System {

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
        std::string host_;
        int         port_     = -1;
        std::string path_     = "/";
        std::string query_;
        std::string fragment_;
        std::string userName_;
        std::string password_;

    public:
        /** @brief Initializes a new empty UriBuilder instance. */
        UriBuilder() = default;

        /**
         * @brief Initializes a new UriBuilder with the specified URI string.
         * @param uri URI string to parse into components.
         */
        explicit UriBuilder(const std::string& uri) {
            Uri u(uri);
            scheme_   = u.getSchemeProperty();
            host_     = u.getHostProperty();
            port_     = u.getPortProperty();
            path_     = u.getAbsolutePathProperty();
            query_    = u.getQueryProperty();
            fragment_ = u.getFragmentProperty();
            userName_ = u.getUserInfoProperty();
        }

        /**
         * @brief Initializes a new UriBuilder from an existing Uri.
         * @param uri Source Uri to copy components from.
         */
        explicit UriBuilder(const Uri& u) {
            scheme_   = u.getSchemeProperty();
            host_     = u.getHostProperty();
            port_     = u.getPortProperty();
            path_     = u.getAbsolutePathProperty();
            query_    = u.getQueryProperty();
            fragment_ = u.getFragmentProperty();
            userName_ = u.getUserInfoProperty();
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
        [[nodiscard]] int getPortProperty()                     const noexcept { return port_; }
        /** @brief Sets the port number. */
        void setPortProperty(int v)                                            { port_ = v; }

        /** @brief Returns the path component. */
        [[nodiscard]] const std::string& getPathProperty()     const noexcept { return path_; }
        /** @brief Sets the path component. */
        void setPathProperty(const std::string& v)                            { path_ = v; }

        /** @brief Returns the query string (includes leading '?' if non-empty). */
        [[nodiscard]] const std::string& getQueryProperty()    const noexcept { return query_; }
        /** @brief Sets the query string (with or without leading '?'). */
        void setQueryProperty(const std::string& v)                           { query_ = v; }

        /** @brief Returns the fragment (includes leading '#' if non-empty). */
        [[nodiscard]] const std::string& getFragmentProperty() const noexcept { return fragment_; }
        /** @brief Sets the fragment (with or without leading '#'). */
        void setFragmentProperty(const std::string& v)                        { fragment_ = v; }

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
         */
        [[nodiscard]] std::string ToString() const {
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
         * @throws std::invalid_argument if the resulting string is not a valid URI.
         */
        [[nodiscard]] Uri getUriProperty() const { return Uri(ToString()); }
    };

} // namespace System
