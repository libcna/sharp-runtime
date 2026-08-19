// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <optional>
#include <string>
#include "System/Uri.hpp"

namespace System {

    /**
     * @brief Converts a string to a Uri and vice versa.
     *
     * Minimal C++ counterpart of .NET System.UriTypeConverter.
     * The .NET class derives from TypeConverter (System.ComponentModel), which
     * is not ported to sharp-runtime.  This class provides the minimum API
     * needed to convert between strings and System::Uri objects.
     */
    class UriTypeConverter {
    public:
        /** @brief Default constructor. */
        UriTypeConverter() = default;

        /** @brief Virtual destructor. */
        virtual ~UriTypeConverter() = default;

        /**
         * @brief Returns true if conversion from a string is supported.
         *
         * C++ counterpart of .NET UriTypeConverter.CanConvertFrom(ITypeDescriptorContext, Type).
         * Always returns true (only string→Uri is supported).
         */
        [[nodiscard]] virtual bool CanConvertFrom() const noexcept { return true; }

        /**
         * @brief Returns true if conversion to a string is supported.
         *
         * C++ counterpart of .NET UriTypeConverter.CanConvertTo(ITypeDescriptorContext, Type).
         * Always returns true (only Uri→string is supported).
         */
        [[nodiscard]] virtual bool CanConvertTo() const noexcept { return true; }

        /**
         * @brief Converts a string to a Uri.
         *
         * C++ counterpart of .NET UriTypeConverter.ConvertFrom(ITypeDescriptorContext,
         * CultureInfo, object).
         *
         * @param text The string representation of a URI.
         * @return A Uri constructed from @p text, or **`std::nullopt` when @p text is empty**.
         * @throws System::UriFormatException if @p text is malformed.
         *
         * Ticket #1999 / SR-AUD-148 (U-I). The return type was a by-value `Uri`, which **cannot
         * express .NET's `null`**, so an empty string was forwarded straight to the `Uri`
         * constructor and threw `UriFormatException`. .NET returns null:
         * @code
         * if (value is string uriString)
         * {
         *     if (string.IsNullOrEmpty(uriString))
         *     {
         *         return null;
         *     }
         *     // Let the Uri constructor throw any informative exceptions
         *     return new Uri(uriString, UriKind.RelativeOrAbsolute);
         * }                                       // UriTypeConverter.cs:40-51
         * @endcode
         * The empty case is the ONLY one .NET short-circuits -- its comment says outright that a
         * malformed string is left to the constructor -- so `std::optional` widens exactly one
         * input and nothing else.
         *
         * @note The kind is spelled **explicitly** as `RelativeOrAbsolute`, matching the
         * reference. It is behaviourally identical to the one-argument `Uri(text)` this used to
         * call -- with that kind the two guards in `Uri(string, UriKind)` are both inert -- so
         * the change is documentation rather than behaviour.
         */
        [[nodiscard]] virtual std::optional<Uri> ConvertFrom(const std::string& text) const {
            if (text.empty()) return std::nullopt;
            return Uri(text, UriKind::RelativeOrAbsolute);
        }

        /**
         * @brief Converts a Uri to its string representation.
         *
         * C++ counterpart of .NET UriTypeConverter.ConvertTo(ITypeDescriptorContext, CultureInfo, object, Type).
         * Real .NET's implementation returns uri.OriginalString, not uri.AbsoluteUri --
         * verified against UriTypeConverter.cs, which explicitly uses OriginalString so the
         * round-trip works for relative Uris too (AbsoluteUri throws for those). This
         * previously called getAbsoluteUriProperty() instead -- fixed to match, though in this
         * port's current implementation the two properties hold the same underlying value (see
         * Uri::getOriginalStringProperty()'s doc-comment), so this is a forward-looking
         * correctness fix rather than one with an observable behavior difference today.
         * @param uri The Uri to convert.
         * @return The original URI string.
         */
        [[nodiscard]] virtual std::string ConvertTo(const Uri& uri) const {
            return uri.getOriginalStringProperty();
        }
    };

} // namespace System
