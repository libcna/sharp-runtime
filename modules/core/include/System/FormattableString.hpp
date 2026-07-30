// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IFormatProvider.hpp"
#include "System/IndexOutOfRangeException.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a composite format string along with the arguments to be formatted.
     *
     * C++ counterpart of .NET System.FormattableString.
     *
     * In C#, instances are produced by the compiler from interpolated string literals
     * (e.g. @c $"Hello, {name}"). In C++, there is no such compiler support; construct
     * instances directly or use FormattableStringFactory::Create().
     *
     * Arguments are stored as std::string (rather than object?) because C++ has no
     * universal base type for arbitrary values.
     */
    class FormattableString {
    protected:
        std::string              format_;
        std::vector<std::string> args_;

    public:
        /**
         * @brief Initializes a new instance with the specified composite format string and arguments.
         * @param format A composite format string (e.g. "{0} and {1}").
         * @param args   Zero or more string arguments substituted for the placeholders.
         */
        explicit FormattableString(std::string format, std::vector<std::string> args = {})
            : format_(std::move(format)), args_(std::move(args)) {}

        /** @brief Virtual destructor for safe polymorphic destruction. */
        virtual ~FormattableString() = default;

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the composite format string.
         *
         * C++ counterpart of .NET FormattableString.Format.
         */
        [[nodiscard]] virtual const std::string& getFormatProperty() const { return format_; }

        /**
         * @brief Gets the number of arguments to be formatted.
         *
         * C++ counterpart of .NET FormattableString.ArgumentCount.
         */
        [[nodiscard]] virtual intcs getArgumentCountProperty() const {
            return static_cast<intcs>(args_.size());
        }

        // -----------------------------------------------------------------------
        // Argument access
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the argument at the specified index.
         *
         * C++ counterpart of .NET FormattableString.GetArgument(int).
         * @param index The zero-based index of the argument.
         * @return The string argument at @p index.
         * @throws System::IndexOutOfRangeException if @p index is out of bounds.
         */
        [[nodiscard]] virtual const std::string& GetArgument(intcs index) const {
            if (index < 0 || static_cast<std::size_t>(index) >= args_.size())
                throw System::IndexOutOfRangeException();
            return args_[static_cast<std::size_t>(index)];
        }

        /**
         * @brief Returns an array containing all format arguments.
         *
         * C++ counterpart of .NET FormattableString.GetArguments().
         * @return A copy of the internal argument vector.
         */
        [[nodiscard]] virtual std::vector<std::string> GetArguments() const { return args_; }

        // -----------------------------------------------------------------------
        // Formatting
        // -----------------------------------------------------------------------

        /**
         * @brief Formats the composite string with arguments substituted.
         *
         * C++ counterpart of .NET FormattableString.ToString().
         * Replaces @c {0}, @c {1}, … with the corresponding stored argument strings.
         *
         * This is **one left-to-right pass over the format string**, appending to a
         * separate output. Substituted argument text is never re-examined, so an
         * argument whose own text contains a format item is emitted verbatim rather
         * than being reinterpreted as syntax.
         *
         * Ticket #1883 (SR-AUD-015, CCF-012) replaced a per-index find/replace sweep
         * that lacked that property: it ran one full pass over the *result* per
         * argument index, so text inserted for index 0 was re-read while index 1 was
         * being substituted. `FormattableString("{0}", {"{1}", "second"}).ToString()`
         * returned `"second"` — argument 1 overwriting argument 0's literal text —
         * where the correct result is `"{1}"`.
         *
         * The **accepted grammar is deliberately unchanged**: `{{`/`}}` are not
         * escapes, a stray `}` is literal text, an index with no matching argument
         * stays literal rather than raising `FormatException`, and `{N,width}` /
         * `{N:spec}` are not recognised as items. Adopting .NET's grammar changes what
         * currently-succeeding calls return and is gated on explicit user approval as
         * ticket #1884 (`docs/CompositeFormatBoundaryPlan.md` §20).
         *
         * @return The formatted result string.
         */
        [[nodiscard]] virtual std::string ToString() const {
            // Beyond this an index cannot address any argument, so the accumulator is
            // capped rather than allowed to overflow on a long digit run.
            constexpr std::size_t kIndexLimit = 1000000u;

            std::string result;
            result.reserve(format_.size());
            const std::size_t n = format_.size();
            std::size_t i = 0;
            while (i < n) {
                if (format_[i] != '{') { result.push_back(format_[i++]); continue; }

                std::size_t j = i + 1;
                std::size_t index = 0;
                bool tooLarge = false;
                while (j < n && format_[j] >= '0' && format_[j] <= '9') {
                    if (index >= kIndexLimit) tooLarge = true;
                    else index = index * 10 + static_cast<std::size_t>(format_[j] - '0');
                    ++j;
                }

                // A resolvable item is "{" digits "}" naming a stored argument. Anything
                // else — no digits, no closing brace, an alignment or specifier
                // component, or an index with no argument — stays literal, exactly as
                // before.
                if (j > i + 1 && j < n && format_[j] == '}' && !tooLarge && index < args_.size()) {
                    result += args_[index];
                    i = j + 1;
                    continue;
                }
                result.push_back('{');
                ++i;
            }
            return result;
        }

        /**
         * @brief Formats the composite string using the specified format provider.
         *
         * C++ counterpart of .NET FormattableString.ToString(IFormatProvider).
         * The provider is accepted for API compatibility but ignored in this port
         * (no locale-aware formatting is implemented).
         * @param formatProvider An IFormatProvider (may be nullptr; ignored).
         * @return The formatted result string.
         */
        [[nodiscard]] virtual std::string ToString(const IFormatProvider* /*formatProvider*/) const {
            return ToString();
        }

        // -----------------------------------------------------------------------
        // Static helpers
        // -----------------------------------------------------------------------

        /**
         * @brief Formats the formattable string using the invariant culture.
         *
         * C++ counterpart of .NET FormattableString.Invariant(FormattableString).
         * In this port, culture has no effect, so the result equals ToString().
         * @param formattable The FormattableString to format.
         * @return The formatted string.
         */
        static std::string Invariant(const FormattableString& formattable) {
            return formattable.ToString();
        }

        /**
         * @brief Formats the formattable string using the current culture.
         *
         * C++ counterpart of .NET FormattableString.CurrentCulture(FormattableString).
         * In this port, culture has no effect, so the result equals ToString().
         * @param formattable The FormattableString to format.
         * @return The formatted string.
         */
        static std::string CurrentCulture(const FormattableString& formattable) {
            return formattable.ToString();
        }
    };

} // namespace System
