// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include "System/FormattableString.hpp"

namespace System::Runtime::CompilerServices {

    /**
     * @brief Provides a static method to create a FormattableString object from a
     * composite format string and its arguments.
     *
     * C++ counterpart of .NET System.Runtime.CompilerServices.FormattableStringFactory.
     * In C#, the compiler calls this factory when it lowers an interpolated string
     * literal to a FormattableString. In C++, callers must invoke Create() directly.
     */
    class FormattableStringFactory {
    public:
        FormattableStringFactory() = delete;

        /**
         * @brief Creates a FormattableString from a composite format string and
         * zero or more string arguments.
         *
         * C++ counterpart of .NET FormattableStringFactory.Create(string, params object[]).
         *
         * @param format    A composite format string (e.g. "{0} and {1}"). An **empty** format
         *                  is valid and produces an empty FormattableString; only a *null*
         *                  reference is rejected by .NET, and `std::string` has no null state,
         *                  so there is nothing for this overload to reject.
         * @param arguments Zero or more string arguments substituted for the placeholders. They
         *                  are stored whether or not the format references them.
         * @return A FormattableString wrapping @p format and @p arguments.
         *
         * @note This method throws nothing of its own. An earlier version of this comment
         * claimed `@throws std::invalid_argument if format is empty`, which was never true of
         * the body and does not match .NET either — .NET's factory null-checks `format` and
         * `arguments` and has no empty-string rule. The behaviour was correct and the *claim*
         * was the defect, so the claim was removed rather than an empty-string rejection added
         * (ticket #1978 / SR-AUD-059). The accepted-empty behaviour is pinned by
         * `FormattableStringFactoryTests.EmptyFormat_IsAcceptedAndYieldsAnEmptyResult`.
         */
        static System::FormattableString Create(std::string format,
                                                std::vector<std::string> arguments = {}) {
            return System::FormattableString(std::move(format), std::move(arguments));
        }
    };

} // namespace System::Runtime::CompilerServices
