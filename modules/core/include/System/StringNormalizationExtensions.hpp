// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/ArgumentException.hpp"
#include "System/Text/NormalizationForm.hpp"

namespace System {

    /**
     * @brief Provides extension-like static methods for Unicode string normalization.
     *
     * C++ counterpart of .NET System.StringNormalizationExtensions.
     *
     * @note <b>`IsNormalized` returns true and `Normalize` returns its argument unchanged for
     *       every input, and that is .NET's own behaviour in invariant globalization mode</b>
     *       rather than a stub this port invented. Measured on ticket #2386 against
     *       `Normalization.cs:11-40`:
     *
     *       @code
     *       // In Invariant mode we assume all characters are normalized because we don't
     *       // support any linguistic operations on strings.
     *       if (GlobalizationMode.Invariant || Ascii.IsValid(source)) { return true; }
     *       @endcode
     *
     *       .NET has <b>no normalization tables of its own</b> — it delegates to ICU on Unix and
     *       NLS on Windows (`Normalization.Icu.cs`, `Normalization.Nls.cs`), and
     *       `CharUnicodeInfoData.cs`, the source of record `docs/StandingApprovals.md` SA-4
     *       names, contains zero decomposition, combining-class, composition-exclusion or
     *       quick-check data. So reproducing .NET's <i>non</i>-invariant behaviour needs a
     *       Unicode data source SA-4 does not grant plus a UAX #15 implementation.
     *
     *       <b>Ticket #2338 DECIDED this on 2026-08-19, and the decision is to keep the invariant
     *       behaviour and declare it.</b> Two alternatives were offered and declined:
     *       - <b>own UCD tables plus UAX #15</b> — the decomposition mappings, canonical combining
     *         classes, composition exclusions and quick-check properties, none of which is present
     *         here, plus canonical ordering, recursive decomposition and canonical composition.
     *         Size L-to-XL, and it would introduce a <i>second</i> Unicode version to keep in step
     *         with SA-4's 16.0;
     *       - <b>an ICU dependency</b>, as .NET takes — which is the shape `CLAUDE.md` already
     *         declined for `System.Security.Cryptography` (<i>"a large new external dependency"</i>),
     *         so taking it here would reverse a standing decision rather than make a new one.
     *
     *       Measured at the time of the decision: <b>zero call sites in `cna` and zero in
     *       `mobile-eggbert`</b>, and the only in-repository uses are this type's own tests. So
     *       the capability being declined is one no caller currently has.
     *
     *       <b>This is a declaration, not a stub.</b> The port is not diverging from .NET; it is
     *       matching .NET under .NET's own stated conditions. Pinned by
     *       `StringNormalizationTests.Decl2338_*`.
     *
     *       What this means for a caller today: a true from `IsNormalized` means "this runtime
     *       performs no linguistic normalization", exactly as it does for a .NET application
     *       built with `InvariantGlobalization=true`. It is <b>not</b> a claim that the string
     *       is in the requested form.
     *
     * @note <b>The normalization form is validated, and that half is not invariant-mode
     *       dependent.</b> `CheckNormalizationForm` runs <i>before</i> the invariant shortcut
     *       (`Normalization.cs:13,29`), so .NET rejects an undefined form on every platform and
     *       in every mode. This port now does the same (#2386).
     */
    struct StringNormalizationExtensions {
        StringNormalizationExtensions() = delete;

        /**
         * @brief Determines whether the string is in Unicode NFC form.
         * @param str The string to check.
         * @return true — see the class note.
         */
        static bool IsNormalized(const std::string& str) {
            return IsNormalized(str, System::Text::NormalizationForm::FormC);
        }

        /**
         * @brief Determines whether the string is in the specified normalization form.
         * @param str  The string to check.
         * @param form The normalization form.
         * @return true — see the class note.
         * @throws System::ArgumentException with `paramName == "normalizationForm"` if @p form
         *         is not one of the four defined values (#2386).
         */
        static bool IsNormalized(const std::string& /*str*/,
                                 System::Text::NormalizationForm form) {
            CheckNormalizationForm(form);
            return true;
        }

        /**
         * @brief Returns the string normalized to NFC.
         * @param str The string to normalize.
         * @return The input string unchanged — see the class note.
         */
        static std::string Normalize(const std::string& str) {
            return Normalize(str, System::Text::NormalizationForm::FormC);
        }

        /**
         * @brief Returns the string normalized to the specified form.
         * @param str  The string to normalize.
         * @param form The normalization form.
         * @return The input string unchanged — see the class note.
         * @throws System::ArgumentException with `paramName == "normalizationForm"` if @p form
         *         is not one of the four defined values (#2386).
         */
        static std::string Normalize(const std::string& str,
                                     System::Text::NormalizationForm form) {
            CheckNormalizationForm(form);
            return str;
        }

    private:
        /**
         * @brief `Normalization.CheckNormalizationForm` (`Normalization.cs:88-97`), transcribed.
         *
         * The enum's four values are 1, 2, 5 and 6 — <b>3 and 4 are holes</b>, so an undefined
         * value is not merely one outside the range and a bounds check would accept two of them.
         * .NET enumerates the four, and so does this.
         *
         * The message is .NET's verbatim (`Strings.resx:1324-1326`,
         * `Argument_InvalidNormalizationForm`), and the parameter name is `nameof(
         * normalizationForm)` — <b>not</b> this port's own parameter spelling `form`, because a
         * caller catching `ArgumentException` reads `ParamName` and .NET's answer is the one
         * worth matching.
         *
         * .NET's second clause — a `PlatformNotSupportedException` for FormKC/FormKD on Browser
         * and WASI, where ICU ships without compatibility data — is deliberately NOT reproduced.
         * It is conditioned on `!GlobalizationMode.Invariant`, and this runtime is always in the
         * invariant case, so the branch is unreachable in .NET under this port's own conditions.
         */
        static void CheckNormalizationForm(System::Text::NormalizationForm form) {
            if (form != System::Text::NormalizationForm::FormC
                && form != System::Text::NormalizationForm::FormD
                && form != System::Text::NormalizationForm::FormKC
                && form != System::Text::NormalizationForm::FormKD) {
                throw System::ArgumentException("Invalid or unsupported normalization form.",
                                                "normalizationForm");
            }
        }
    };

} // namespace System
