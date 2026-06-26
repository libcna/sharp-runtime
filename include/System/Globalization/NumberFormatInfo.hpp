// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Globalization {

    /**
     * @brief Provides culture-specific information for formatting and parsing numeric values.
     *
     * Partial C++ counterpart of .NET System.Globalization.NumberFormatInfo.
     * Only the invariant (locale-independent) format is meaningfully implemented.
     *
     * @note Status: Partial — invariant culture only.
     */
    class NumberFormatInfo {
        bool isReadOnly_ = false;
    public:
        std::string NumberDecimalSeparator   = ".";
        std::string NumberGroupSeparator     = ",";
        std::string CurrencyDecimalSeparator = ".";
        std::string CurrencyGroupSeparator   = ",";
        std::string CurrencySymbol           = "$";
        std::string NegativeSign             = "-";
        std::string PositiveSign             = "+";
        std::string PercentSymbol            = "%";
        std::string NaNSymbol                = "NaN";
        std::string PositiveInfinitySymbol   = "Infinity";
        std::string NegativeInfinitySymbol   = "-Infinity";
        int NumberDecimalDigits              = 2;

        /** Constructs a mutable NumberFormatInfo with invariant defaults. */
        NumberFormatInfo() = default;

        /** Returns true if this NumberFormatInfo is read-only. */
        [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

        /** @brief Returns the invariant (culture-independent) NumberFormatInfo. */
        [[nodiscard]] static const NumberFormatInfo& InvariantInfo() {
            static NumberFormatInfo info;
            info.isReadOnly_ = true;
            return info;
        }

        /** @brief Returns the NumberFormatInfo for the current thread culture. Stub — returns InvariantInfo. */
        [[nodiscard]] static const NumberFormatInfo& CurrentInfo() {
            return InvariantInfo();
        }
    };

} // namespace System::Globalization
