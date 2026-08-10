// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <memory>

#include "System/Net/ICredentials.hpp"
#include "System/Uri.hpp"

namespace System::Net {

    /**
     * Provides the base interface for Web proxy implementations.
     *
     * C++ counterpart of .NET System.Net.IWebProxy.
     *
     * @note A C# nullable Uri return is represented by std::shared_ptr<System::Uri>.
     * Parameters are references because C++ callers cannot pass null references.
     */
    class IWebProxy {
    public:
        virtual ~IWebProxy() = default;

        /**
         * Returns the proxy URI for the specified destination.
         *
         * @param destination The destination to access.
         * @return The proxy URI, or a URI equivalent to @p destination when it is bypassed.
         */
        [[nodiscard]] virtual std::shared_ptr<System::Uri> GetProxy(const System::Uri& destination) = 0;

        /**
         * Indicates whether the proxy server should be bypassed for the specified host.
         *
         * @param host The destination to check.
         * @return @c true when the proxy should not be used.
         */
        [[nodiscard]] virtual bool IsBypassed(const System::Uri& host) = 0;

        /** Returns the credentials submitted to the proxy server for authentication. */
        [[nodiscard]] virtual std::shared_ptr<ICredentials> getCredentialsProperty() const = 0;

        /** Sets the credentials submitted to the proxy server for authentication. */
        virtual void setCredentialsProperty(std::shared_ptr<ICredentials> value) = 0;
    };

} // namespace System::Net
