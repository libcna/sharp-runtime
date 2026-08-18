// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <optional>
#include <string>
#include "System/Exception.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include "System/Net/Http/HttpRequestError.hpp"

namespace System::Net::Http {

    /**
     * @brief The exception thrown by HttpClient/HttpMessageInvoker to indicate an error
     * with the HTTP request or response.
     *
     * C++ counterpart of .NET System.Net.Http.HttpRequestException.
     */
    class HttpRequestException : public System::Exception {
        HttpRequestError httpRequestError_ = HttpRequestError::Unknown;
        std::optional<System::Net::HttpStatusCode> statusCode_;

    public:
        /** Creates a new instance with no message. */
        /**
         * @brief Constructs an exception with .NET's fallback message for this type.
         *
         * Ticket #2323. .NET's `HttpRequestException()` is `{ }` (HttpRequestException.cs:10-11),
         * so `Message` falls through to `Exception`'s
         * `SR.Format(SR.Exception_WasThrown, GetClassName())` and names THIS type. `{0}` is
         * reflection, which this port does not have, so it is resolved statically here -- by the
         * one entity that knows the answer. Inheriting the base's string would have named
         * `System.Exception`, which is a lie rather than an absence.
         */
        HttpRequestException()
            : System::Exception(
                  "Exception of type 'System.Net.Http.HttpRequestException' was thrown.") {}

        /** Creates a new instance with the specified message. */
        explicit HttpRequestException(const std::string& message) : System::Exception(message) {}

        /**
         * @brief Creates a new instance with a message and inner exception.
         * @note Copies the HResult of a non-null System::Exception inner exception.
         */
        HttpRequestException(const std::string& message, std::exception_ptr inner)
            : System::Exception(message, inner) {
            if (inner) {
                try {
                    std::rethrow_exception(inner);
                } catch (const System::Exception& exception) {
                    setHResultProperty(exception.getHResultProperty());
                } catch (...) {
                    // A non-System exception retains the outer base HResult.
                }
            }
        }

        /** Creates a new instance with a message, inner exception, and HTTP status code. */
        HttpRequestException(const std::string& message, std::exception_ptr inner, System::Net::HttpStatusCode statusCode)
            : HttpRequestException(message, inner) {
            statusCode_ = statusCode;
        }

        /** Creates a new instance with an HttpRequestError, message, inner exception, and HTTP status code. */
        explicit HttpRequestException(HttpRequestError httpRequestError, const std::string& message = "",
                                      std::exception_ptr inner = nullptr,
                                      std::optional<System::Net::HttpStatusCode> statusCode = std::nullopt)
            : HttpRequestException(message, inner) {
            httpRequestError_ = httpRequestError;
            statusCode_ = statusCode;
        }

        /** @return The HttpRequestError that caused the exception. */
        [[nodiscard]] HttpRequestError getHttpRequestErrorProperty() const { return httpRequestError_; }

        /** @return The HTTP status code to be returned with the exception, or empty if not applicable. */
        [[nodiscard]] std::optional<System::Net::HttpStatusCode> getStatusCodeProperty() const { return statusCode_; }
    };

} // namespace System::Net::Http
