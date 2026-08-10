// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include "System/ArgumentException.hpp"

namespace System {

/**
 * @brief The exception that is thrown when the value of an argument is outside
 *        the allowable range of values as defined by the invoked method.
 *
 * C++ counterpart of .NET System.ArgumentOutOfRangeException.
 * Derives from ArgumentException and adds an optional actual-value string that
 * identifies the out-of-range value.
 */
class ArgumentOutOfRangeException : public ArgumentException {
public:
    /**
     * @brief Initializes a new instance with the default error message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException().
     */
    ArgumentOutOfRangeException();

    /**
     * @brief Initializes a new instance with the name of the parameter that caused the exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string paramName).
     * @param paramName The name of the parameter that caused the exception.
     */
    explicit ArgumentOutOfRangeException(const char* paramName);

    /**
     * @brief Initializes a new instance with the name of the parameter that caused the exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string paramName).
     * @param paramName The name of the parameter that caused the exception.
     */
    explicit ArgumentOutOfRangeException(const std::string& paramName);

    /**
     * @brief Initializes a new instance with a message and an inner exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string message, Exception innerException).
     * @param message The error message.
     * @param inner   The exception that is the cause of the current exception.
     */
    ArgumentOutOfRangeException(const std::string& message,
                                std::exception_ptr inner);

    /**
     * @brief Initializes a new instance with the parameter name and a custom message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string paramName, string message).
     * @param paramName The name of the parameter that caused the exception.
     * @param message   A message that describes the error.
     */
    ArgumentOutOfRangeException(const std::string& paramName,
                                const std::string& message);

    /**
     * @brief Initializes a new instance with a parameter name, actual value, and message.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException(string, object, string).
     * Matches .NET's Message property override, which appends "Actual value was X."
     * after the "(Parameter 'x')" marker whenever an actual value is supplied.
     * @param paramName   The name of the parameter that caused the exception.
     * @param actualValue The value of the argument that caused the exception (as string).
     * @param message     The error message.
     */
    ArgumentOutOfRangeException(const std::string& paramName,
                                const std::string& actualValue,
                                const std::string& message);

    /**
     * @brief Gets a string representation of the actual value that caused the exception.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ActualValue.
     * Returns an empty string if no actual value was provided.
     * @return The actual value, or an empty string.
     */
    [[nodiscard]] virtual std::string getActualValueProperty() const { return actualValue_; }

    // -----------------------------------------------------------------------
    // Static guard helpers  (C++ counterparts of .NET ThrowIf* methods)
    // -----------------------------------------------------------------------
    //
    // THE COMPILE-TIME DOMAIN OF THE NINE GUARDS BELOW IS DECLARED, NOT IMPLIED (ticket #2254,
    // finding SR-AUD-091). Each guard needs two things from T: the ONE comparison operator it
    // evaluates, and a way to render a value as diagnostic text. Both are now stated by a
    // static_assert in the guard itself, so a type outside the contract is rejected by a
    // sharp-runtime sentence naming the contract rather than by nine libstdc++ candidates for
    // std::to_string. See FormatGuardValue below for the rendering domain, and
    // docs/CoreArgumentOutOfRangeGuardDomainPlan.md for the measured before/after matrix.
    //
    // KNOWN LIMITATION, recorded rather than repaired (#2253 §7): all nine guards take T BY
    // VALUE, so a non-copyable comparable type can never bind to them, whatever its rendering.
    // Repairing that means changing nine public template signatures to const T&, which is a
    // public API change outside SR-AUD-091's premise.

    // KNOWN MINOR GAP (audited, not fixed): real .NET's ThrowIfXxx helpers embed the actual
    // value directly in the PRIMARY message text too (e.g. ThrowIfZero's exact resource string
    // is "{paramName} ('{value}') must be a non-zero value."), and ThrowIfNegativeOrZero/
    // ThrowIfEqual/ThrowIfNotEqual use noticeably different prose than the messages below (e.g.
    // "must be a non-negative and non-zero value." vs this port's "must be a positive value.").
    // The information itself is NOT missing here -- paramName is present via
    // getParamNameProperty(), the value via getActualValueProperty() and (after this ticket's
    // fix) the "Actual value was X." message suffix -- this is a pure wording/paraphrase
    // difference in the primary sentence, not a structural gap, so left as-is matching this
    // project's established practice of not chasing verbatim message-text parity for its own
    // sake (see CLAUDE.md's parity philosophy).

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is zero.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfZero.
     * @tparam T  A default-constructible, equality-comparable, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfZero(T value, const std::string& paramName = "")
    {
        static_assert(requires(T a) { a == T{}; },
            "System::ArgumentOutOfRangeException::ThrowIfZero evaluates 'value == T{}', so T must "
            "be default-constructible and equality-comparable.");
        if (value == T{})
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be a non-zero value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is negative.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNegative.
     * @tparam T  A default-constructible, ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNegative(T value, const std::string& paramName = "")
    {
        static_assert(requires(T a) { a < T{}; },
            "System::ArgumentOutOfRangeException::ThrowIfNegative evaluates 'value < T{}', so T "
            "must be default-constructible and less-than-comparable.");
        if (value < T{})
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be a non-negative value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is negative or zero.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNegativeOrZero.
     * @tparam T  A default-constructible, ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNegativeOrZero(T value, const std::string& paramName = "")
    {
        static_assert(requires(T a) { a <= T{}; },
            "System::ArgumentOutOfRangeException::ThrowIfNegativeOrZero evaluates 'value <= T{}', "
            "so T must be default-constructible and less-or-equal-comparable.");
        if (value <= T{})
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be a positive value.");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is greater than @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfGreaterThan.
     * @tparam T  A totally ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The upper bound (exclusive).
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfGreaterThan(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a > b; },
            "System::ArgumentOutOfRangeException::ThrowIfGreaterThan evaluates 'value > other', so T "
            "must be greater-than-comparable.");
        if (value > other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be less than or equal to " + FormatGuardValue(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is greater than or equal to @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfGreaterThanOrEqual.
     * @tparam T  A totally ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The exclusive upper bound.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfGreaterThanOrEqual(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a >= b; },
            "System::ArgumentOutOfRangeException::ThrowIfGreaterThanOrEqual evaluates 'value >= other', so T "
            "must be greater-or-equal-comparable.");
        if (value >= other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be less than " + FormatGuardValue(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is less than @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfLessThan.
     * @tparam T  A totally ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The lower bound (exclusive).
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfLessThan(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a < b; },
            "System::ArgumentOutOfRangeException::ThrowIfLessThan evaluates 'value < other', so T "
            "must be less-than-comparable.");
        if (value < other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be greater than or equal to " + FormatGuardValue(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value is less than or equal to @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfLessThanOrEqual.
     * @tparam T  A totally ordered, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The inclusive lower bound.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfLessThanOrEqual(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a <= b; },
            "System::ArgumentOutOfRangeException::ThrowIfLessThanOrEqual evaluates 'value <= other', so T "
            "must be less-or-equal-comparable.");
        if (value <= other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must be greater than " + FormatGuardValue(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value equals @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfEqual.
     * @tparam T  An equality-comparable, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The value it must not equal.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfEqual(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a == b; },
            "System::ArgumentOutOfRangeException::ThrowIfEqual evaluates 'value == other', so T "
            "must be equality-comparable.");
        if (value == other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must not equal " + FormatGuardValue(other) + ".");
    }

    /**
     * @brief Throws ArgumentOutOfRangeException if @p value does not equal @p other.
     *
     * C++ counterpart of .NET ArgumentOutOfRangeException.ThrowIfNotEqual.
     * @tparam T  An equality-comparable, renderable type
     *            (see FormatGuardValue for the rendering domain).
     * @param value     The value to check.
     * @param other     The value it must equal.
     * @param paramName The name of the parameter.
     */
    template<typename T>
    static void ThrowIfNotEqual(T value, T other, const std::string& paramName = "")
    {
        static_assert(requires(T a, T b) { a != b; },
            "System::ArgumentOutOfRangeException::ThrowIfNotEqual evaluates 'value != other', so T "
            "must be inequality-comparable.");
        if (value != other)
            throw ArgumentOutOfRangeException(paramName, FormatGuardValue(value),
                "'" + paramName + "' must equal " + FormatGuardValue(other) + ".");
    }

private:
    /** @brief A `false` that depends on `T`, so a `static_assert` in a discarded
     *         `if constexpr` branch does not fire at template definition time. */
    template<typename>
    static constexpr bool kUnrenderable = false;

    /**
     * @brief Renders a guard's operand as the diagnostic text that becomes the exception's
     *        actual value and its message operand.
     *
     * Added by ticket #2254 for finding SR-AUD-091. Every guard used to call
     * `std::to_string(value)` directly, which imposed an *undeclared* formatting requirement on
     * top of each guard's declared comparison contract: half of the advertised generic surface
     * (99 of 198 measured `(type, guard)` pairs) failed to compile, and it failed inside
     * `<bits/basic_string.h>` with nine standard-library candidates rather than at any stated
     * sharp-runtime constraint. Real .NET leaves `ThrowIfEqual<T>` unconstrained and constrains
     * the ordering helpers only to `IComparable<T>`; neither requires a formatting interface in
     * order to compare.
     *
     * The rendering domain is an ordered chain, **first match wins**:
     *
     *  1. anything `std::to_string` accepts — every arithmetic type, and (through integral
     *     promotion) every unscoped enumeration;
     *  2. a `ToString()` member convertible to `std::string` — this repository's own rendering
     *     convention (`System::Object::ToString()`, `Decimal`, `TimeSpan`, `DateTime`, `Guid`),
     *     and what .NET itself uses for the actual value;
     *  3. any enumeration, rendered as its underlying integer;
     *  4. a non-pointer type convertible to `std::string_view` — `std::string`,
     *     `std::string_view`, and user types with `operator std::string_view`;
     *  5. a non-pointer type convertible to `std::string`;
     *  6. an unqualified `to_string(value)` found by argument-dependent lookup — the documented
     *     extension point for any other user type.
     *
     * **Branch order is a correctness property, not a style choice.** Branch 1 is the call the
     * guards made before this ticket, so every type that compiled before takes the identical path
     * and emits byte-identical text; no later branch is reachable for it. Branch 3 sits above
     * branch 6 so that a scoped enumeration renders as its integer exactly as an unscoped one
     * already does — the cost, recorded deliberately, is that an enumeration cannot override its
     * own rendering through ADL.
     *
     * **Raw pointers are excluded on purpose** and reach the `static_assert`. Constructing a
     * `std::string_view` from a null pointer is undefined behaviour, and `ThrowIfEqual` over two
     * `const char*` compares addresses rather than text — `ThrowIfEqual("a", "b", "p")` deduces
     * `T = const char*` by array-to-pointer decay. Refusing both is the repair, not a gap.
     *
     * `operator<<` is deliberately **not** a branch: supporting it forces `<sstream>` into a
     * header that 404 translation units in this build depend on, measured at +2,819 preprocessed
     * lines (+4.6 %) each, where branch 6 gives any such type the same reach for +0. See
     * `docs/CoreArgumentOutOfRangeGuardDomainPlan.md` §4.3.
     *
     * @tparam T    The operand type.
     * @param value The operand to render.
     * @return The diagnostic text for @p value.
     */
    template<typename T>
    static std::string FormatGuardValue(const T& value)
    {
        if constexpr (requires { std::to_string(value); }) {
            return std::to_string(value);
        } else if constexpr (requires { std::string(value.ToString()); }) {
            return std::string(value.ToString());
        } else if constexpr (std::is_enum_v<T>) {
            return std::to_string(static_cast<std::underlying_type_t<T>>(value));
        } else if constexpr (!std::is_pointer_v<T> &&
                             std::is_convertible_v<const T&, std::string_view>) {
            const std::string_view text = value;
            return std::string(text);
        } else if constexpr (!std::is_pointer_v<T> &&
                             std::is_convertible_v<const T&, std::string>) {
            return static_cast<std::string>(value);
        } else if constexpr (requires { std::string(to_string(value)); }) {
            return std::string(to_string(value));
        } else {
            static_assert(kUnrenderable<T>,
                "System::ArgumentOutOfRangeException's ThrowIf* guards cannot render this type as "
                "diagnostic text. Supported, in order: (1) any type std::to_string accepts; "
                "(2) a type with a ToString() member convertible to std::string; (3) any "
                "enumeration; (4) a non-pointer type convertible to std::string_view; (5) a "
                "non-pointer type convertible to std::string; (6) a type with an ADL-visible "
                "to_string(value). Raw pointers are excluded deliberately: std::string_view from "
                "a null pointer is undefined behaviour, and comparing two const char* compares "
                "addresses rather than text -- pass std::string_view instead.");
            return {};
        }
    }

    std::string actualValue_;
};

} // namespace System
