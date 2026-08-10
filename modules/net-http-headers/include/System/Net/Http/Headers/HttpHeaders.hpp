// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Collections/Specialized/NameValueCollection.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Net::Http::Headers {

    /**
     * @brief Base class for a collection of protocol headers, as exposed by
     * HttpRequestMessage.Headers/HttpResponseMessage.Headers/HttpContent.Headers.
     *
     * C++ counterpart of .NET System.Net.Http.Headers.HttpHeaders.
     *
     * @note This is a **separate, standalone** header collection design — it is not integrated
     * with the plain `unordered_map<string,string>` already used internally by
     * HttpRequestMessage/HttpResponseMessage, nor with WebHeaderCollection (a different,
     * already-ported header bag used for the legacy WebRequest-era surface). Three
     * simplified header-bag designs now coexist in this codebase by design; see NEXT.md for the
     * rationale (introducing this fourth one was a deliberate low-risk choice over migrating the
     * existing two).
     * @note .NET's HttpHeaders maintains a lazily-parsed-and-validated value cache keyed by a
     * per-header-name parser lookup table (~50 known header names, each with its own
     * `HttpHeaderParser`). That table is not reproduced here — this is a simplified, generic,
     * multi-value string bag (composing NameValueCollection, same as WebHeaderCollection). Typed
     * access to well-known headers is provided by the derived HttpContentHeaders/
     * HttpRequestHeaders/HttpResponseHeaders classes, which parse/format through the individual
     * *HeaderValue types already ported in this namespace on each access, rather than caching a
     * parsed representation.
     * @note NonValidated (HttpHeadersNonValidated) is, as a consequence, functionally identical to
     * the validated accessors here — there is no separate raw/parsed distinction to preserve.
     */
    class HttpHeaders {
        System::Collections::Specialized::NameValueCollection headers_;

    protected:
        HttpHeaders() = default;

        /** @return The single string value stored for @p name, or "" if not present (for derived typed-property getters). */
        [[nodiscard]] std::string getRawValue(const std::string& name) const;
        /** @brief Sets (or removes, if empty) the single string value for @p name (for derived typed-property setters). */
        void setRawValue(const std::string& name, const std::string& value);

    public:
        virtual ~HttpHeaders() = default;

        /**
         * @brief Adds a header value, validating the header name is a token and the value
         * contains no bare CR/LF/NUL.
         * @throws System::ArgumentException if @p name is empty.
         * @throws System::FormatException if @p name is not a valid HTTP token, or @p value
         * contains invalid control characters.
         */
        void Add(const std::string& name, const std::string& value);
        /** @brief Adds each of @p values under @p name (see the single-value Add() for validation). */
        void Add(const std::string& name, const std::vector<std::string>& values);

        /**
         * @brief Adds @p value under @p name, bypassing validation of the **value only**.
         * @return false if @p name is not a valid HTTP token — including an empty name, or one
         * containing CR, LF, NUL, a space, or any RFC 9110 separator. The collection is left
         * completely unchanged when this returns false.
         *
         * @note **"Without validation" governs the value, never the name.** Ticket #2123
         * (SR-AUD-322): this used to reject only an empty name, so
         * `TryAddWithoutValidation("X-Bad\r\nInjected: yes", "v")` returned **true** and
         * `ToString()` emitted two header fields where the caller supplied one. The name is now
         * checked with the same `isToken` predicate `Add` has always used. The **value** is still
         * deliberately unvalidated — a CR-bearing *value* is still accepted through this door and
         * rejected by `Add`, which is the entire difference between them.
         */
        bool TryAddWithoutValidation(const std::string& name, const std::string& value);
        /**
         * @brief Adds each of @p values under @p name, bypassing validation of the values only.
         * @return false if @p name is not a valid HTTP token; see the single-value overload. The
         * name is checked once **before** anything is stored, so a rejected call cannot leave the
         * collection partially populated.
         */
        bool TryAddWithoutValidation(const std::string& name, const std::vector<std::string>& values);

        /**
         * @brief Returns the values for @p name.
         * @throws System::InvalidOperationException if @p name is not present.
         */
        [[nodiscard]] std::vector<std::string> GetValues(const std::string& name) const;
        /** @brief Attempts to retrieve the values for @p name. */
        [[nodiscard]] bool TryGetValues(const std::string& name, std::vector<std::string>& values) const;
        /** @return true if @p name is present in the collection. */
        [[nodiscard]] bool Contains(const std::string& name) const;
        /** @brief Removes @p name from the collection. @return true if it was present. */
        bool Remove(const std::string& name);
        /** @brief Removes all headers. */
        void Clear();

        /** @return The number of distinct header names. */
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const;
        /** @return All header names, in insertion order. */
        [[nodiscard]] const std::vector<std::string>& getHeaderNamesProperty() const;

        /** @return "Name: v1, v2\r\n..." for each header, followed by a final "\r\n". */
        [[nodiscard]] std::string ToString() const;
    };

} // namespace System::Net::Http::Headers
