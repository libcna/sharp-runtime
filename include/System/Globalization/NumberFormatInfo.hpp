// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Globalization/DigitShapes.hpp"

namespace System::Globalization {

/**
 * @brief Provides culture-specific information for formatting and parsing numeric values.
 *
 * C++ counterpart of .NET System.Globalization.NumberFormatInfo.
 * The default constructor initializes invariant (locale-independent) defaults.
 * Only the invariant format is meaningfully implemented; locale-specific formatting
 * is not supported.
 */
class NumberFormatInfo {
    bool isReadOnly_ = false;

public:
    // --- Number format ---
    std::string NumberDecimalSeparator = ".";  ///< String that separates the integer from the fractional part.
    std::string NumberGroupSeparator   = ",";  ///< String that separates groups of digits to the left of the decimal point.
    int NumberDecimalDigits            = 2;    ///< Number of decimal digits in numeric values.
    int NumberNegativePattern          = 1;    ///< Format pattern for negative numeric values (1 = "-n").
    std::vector<int> NumberGroupSizes  = {3};  ///< Number of digits in each group to the left of the decimal point.

    // --- Currency format ---
    std::string CurrencyDecimalSeparator = "."; ///< String that separates the integer from the fractional part in currency values.
    std::string CurrencyGroupSeparator   = ","; ///< String that separates groups of digits to the left of the decimal in currency values.
    std::string CurrencySymbol           = "$"; ///< String used as the currency symbol.
    int CurrencyDecimalDigits            = 2;   ///< Number of decimal digits in currency values.
    int CurrencyNegativePattern          = 0;   ///< Format pattern for negative currency values.
    int CurrencyPositivePattern          = 0;   ///< Format pattern for positive currency values.
    std::vector<int> CurrencyGroupSizes  = {3}; ///< Number of digits in each group to the left of the decimal in currency values.

    // --- Sign and special symbols ---
    std::string NegativeSign            = "-";         ///< String that denotes that the associated number is negative.
    std::string PositiveSign            = "+";         ///< String that denotes that the associated number is positive.
    std::string NaNSymbol               = "NaN";       ///< String that represents the IEEE NaN value.
    std::string PositiveInfinitySymbol  = "Infinity";  ///< String that represents positive infinity.
    std::string NegativeInfinitySymbol  = "-Infinity"; ///< String that represents negative infinity.

    // --- Percent format ---
    std::string PercentSymbol           = "%";  ///< String to use as the percent symbol.
    std::string PerMilleSymbol          = "\xe2\x80\xb0"; ///< String to use as the per mille symbol (‰).
    std::string PercentDecimalSeparator = ".";  ///< String that separates the integer from the fractional part in percent values.
    std::string PercentGroupSeparator   = ",";  ///< String that separates groups of digits to the left of the decimal in percent values.
    int PercentDecimalDigits            = 2;    ///< Number of decimal digits in percent values.
    int PercentNegativePattern          = 0;    ///< Format pattern for negative percent values.
    int PercentPositivePattern          = 0;    ///< Format pattern for positive percent values.
    std::vector<int> PercentGroupSizes  = {3};  ///< Number of digits in each group to the left of the decimal in percent values.

    // --- Digit substitution ---
    DigitShapes DigitSubstitution = DigitShapes::None; ///< Value that specifies how the graphical user interface displays the shape of a digit.
    std::vector<std::string> NativeDigits = {           ///< String array of native digits equivalent to the Western digits 0 through 9.
        "0","1","2","3","4","5","6","7","8","9"};

    /**
     * @brief Constructs a mutable NumberFormatInfo with invariant-culture defaults.
     *
     * C++ counterpart of .NET NumberFormatInfo().
     */
    NumberFormatInfo() = default;

    /**
     * @brief Gets a value indicating whether this instance is read-only.
     *
     * C++ counterpart of .NET NumberFormatInfo.IsReadOnly.
     * @return true if this instance is read-only; otherwise false.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

    /**
     * @brief Returns the culture-independent (invariant) NumberFormatInfo instance.
     *
     * C++ counterpart of .NET NumberFormatInfo.InvariantInfo.
     * @return A const reference to the shared read-only invariant instance.
     */
    [[nodiscard]] static const NumberFormatInfo& InvariantInfo() {
        static NumberFormatInfo info = makeReadOnly();
        return info;
    }

    /**
     * @brief Returns the NumberFormatInfo for the current thread's culture.
     *
     * C++ counterpart of .NET NumberFormatInfo.CurrentInfo.
     * Stub — returns the invariant info.
     * @return A const reference to the invariant NumberFormatInfo.
     */
    [[nodiscard]] static const NumberFormatInfo& CurrentInfo() {
        return InvariantInfo();
    }

    /**
     * @brief Returns a read-only copy of the given NumberFormatInfo.
     *
     * C++ counterpart of .NET NumberFormatInfo.ReadOnly(NumberFormatInfo).
     * @param nfi The source instance.
     * @return A read-only copy.
     */
    static NumberFormatInfo ReadOnly(const NumberFormatInfo& nfi) {
        NumberFormatInfo copy = nfi;
        copy.isReadOnly_ = true;
        return copy;
    }

    /**
     * @brief Returns a mutable copy of this instance.
     *
     * C++ counterpart of .NET NumberFormatInfo.Clone().
     * @return A modifiable copy.
     */
    [[nodiscard]] NumberFormatInfo Clone() const {
        NumberFormatInfo copy = *this;
        copy.isReadOnly_ = false;
        return copy;
    }

private:
    static NumberFormatInfo makeReadOnly() {
        NumberFormatInfo info;
        info.isReadOnly_ = true;
        return info;
    }
};

} // namespace System::Globalization
