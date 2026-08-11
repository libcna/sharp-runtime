// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System {

    /**
     * @brief Carries .NET's obsolescence metadata — message, error flag,
     * diagnostic id and url format — as ordinary runtime state. **It marks
     * nothing and emits no diagnostic in the C++ port.**
     *
     * C++ counterpart of .NET System.ObsoleteAttribute.
     *
     * @warning **Constructing one of these does not deprecate anything.** In
     * .NET the attribute is metadata attached to a declaration, and the
     * compiler reports every use of that declaration — a warning when
     * `IsError` is false, an error when it is true. C++ has no mechanism by
     * which an object of a class attaches to a declaration: constructing one,
     * storing one, setting `IsError` or naming it in a comment leaves every
     * symbol exactly as usable as it was, and no code in this repository reads
     * any of these four values. That is the whole of SR-AUD-115, and it is not
     * a gap to be filled later — the attachment step has no C++ spelling, so no
     * amount of work inside this class can reach the declaration a caller
     * wanted marked.
     *
     * The C++ facility that produces the diagnostic is the `[[deprecated]]`
     * attribute, written on the declaration itself:
     *
     * @code
     * // .NET:  [Obsolete("Use Modern() instead.")] public static int Legacy();
     * [[deprecated("Use Modern() instead.")]] static intcs Legacy();
     * @endcode
     *
     * Two limits of that substitute are measured rather than assumed, and both
     * outlive this class:
     *
     * - **The warning/error choice is not a property of the declaration.** One
     *   `[[deprecated]]` declaration yields `warning: … is deprecated` normally
     *   and `error: … is deprecated [-Werror=deprecated-declarations]` under
     *   this repository's own `-Werror`. So `IsError` — the one member whose
     *   documented purpose is to pick between the two — has no C++ counterpart
     *   even where `[[deprecated]]` is available. It is stored and returned
     *   faithfully as .NET metadata and consulted by nothing.
     * - **Whether this port marks its own obsolete declarations at all is an
     *   open decision, ticket #2289 (SR-AUD-117).** There is no `[[deprecated]]`
     *   anywhere in this repository, because a use of one stops any consumer
     *   that builds warnings-as-errors. This class is not one of the sites that
     *   decision covers, and neither outcome of it changes anything here: an
     *   `ObsoleteAttribute` object is inert either way.
     *
     * .NET additionally seals the type and restricts, through `AttributeUsage`,
     * which declarations may carry it and whether derived declarations inherit
     * it. C++ has no `AttributeUsage`, and there is no declaration to carry it
     * in the first place, so neither restriction is expressible; this class is
     * left derivable rather than `final` because sealing it would reject code
     * that compiles today.
     *
     * `Message`, `DiagnosticId` and `UrlFormat` are nullable (`string?`) in
     * .NET, so a default-constructed attribute exposes `null` there and a
     * caller can tell that apart from an explicitly supplied `""`. This port
     * stores three non-nullable `std::string`s, so **an absent value and an
     * empty one are the same state here** — SR-AUD-116, whose representation is
     * under decision at ticket #2295 and must not be assumed either way.
     *
     * The class exists so that ported code naming `System::ObsoleteAttribute`
     * still compiles and so that the .NET intent stays readable beside the
     * declaration it was attached to. It has no first-party production
     * consumer.
     *
     * SR-AUD-115, tickets #2293 (review) and #2294.
     */
    class ObsoleteAttribute : public Attribute {
        std::string message_;
        bool isError_ = false;
        std::string diagnosticId_;
        std::string urlFormat_;

    public:
        /**
         * Constructs an ObsoleteAttribute with an empty message and
         * isError = false. .NET's parameterless attribute leaves `Message`
         * **null**, which this port has no state to express (SR-AUD-116).
         */
        ObsoleteAttribute() = default;
        /**
         * @brief Constructs an ObsoleteAttribute with the given informational message.
         * @param message Human-readable explanation of the obsolescence.
         */
        explicit ObsoleteAttribute(const std::string& message) : message_(message) {}
        /**
         * @brief Constructs an ObsoleteAttribute with a message and an error flag.
         * @param message Human-readable explanation of the obsolescence.
         * @param isError The .NET meaning recorded verbatim: true where .NET's
         * compiler would reject a use outright rather than warn about it.
         * **Nothing in the C++ port acts on it** — see the class warning. It is
         * stored and returned unchanged, and no value of it makes any
         * declaration harder or easier to use.
         */
        ObsoleteAttribute(const std::string& message, bool isError) : message_(message), isError_(isError) {}

        /** Returns the informational message describing the obsolescence. */
        [[nodiscard]] const std::string& getMessageProperty()      const { return message_; }
        /**
         * Returns the recorded .NET error flag. It reports what .NET's compiler
         * would do with the declaration this attribute described; it does not
         * report anything about this build, where no declaration is marked.
         */
        [[nodiscard]] bool               getIsErrorProperty()      const { return isError_; }
        /**
         * Returns the diagnostic ID associated with this obsolescence, or an
         * empty string where .NET would return null (SR-AUD-116).
         */
        [[nodiscard]] const std::string& getDiagnosticIdProperty() const { return diagnosticId_; }
        /**
         * Returns the URL format string for further documentation, or an empty
         * string where .NET would return null (SR-AUD-116). Nothing in this
         * port formats it; it is metadata for a human reader.
         */
        [[nodiscard]] const std::string& getUrlFormatProperty()    const { return urlFormat_; }

        /** Sets the diagnostic ID associated with this obsolescence. */
        void setDiagnosticIdProperty(const std::string& v) { diagnosticId_ = v; }
        /** Sets the URL format string pointing to further documentation. */
        void setUrlFormatProperty(const std::string& v)    { urlFormat_ = v; }
    };

} // namespace System
