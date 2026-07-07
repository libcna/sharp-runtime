// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/InvalidOperationException.hpp"
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

    std::string numberDecimalSeparator_ = ".";
    std::string numberGroupSeparator_   = ",";
    int numberDecimalDigits_            = 2;
    int numberNegativePattern_          = 1;
    std::vector<int> numberGroupSizes_  = {3};

    std::string currencyDecimalSeparator_ = ".";
    std::string currencyGroupSeparator_   = ",";
    std::string currencySymbol_           = "$";
    int currencyDecimalDigits_            = 2;
    int currencyNegativePattern_          = 0;
    int currencyPositivePattern_          = 0;
    std::vector<int> currencyGroupSizes_  = {3};

    std::string negativeSign_           = "-";
    std::string positiveSign_           = "+";
    std::string naNSymbol_              = "NaN";
    std::string positiveInfinitySymbol_ = "Infinity";
    std::string negativeInfinitySymbol_ = "-Infinity";

    std::string percentSymbol_           = "%";
    std::string perMilleSymbol_          = "\xe2\x80\xb0"; // ‰
    std::string percentDecimalSeparator_ = ".";
    std::string percentGroupSeparator_   = ",";
    int percentDecimalDigits_            = 2;
    int percentNegativePattern_          = 0;
    int percentPositivePattern_          = 0;
    std::vector<int> percentGroupSizes_  = {3};

    DigitShapes digitSubstitution_ = DigitShapes::None;
    std::vector<std::string> nativeDigits_ = {"0","1","2","3","4","5","6","7","8","9"};

    /** @brief Throws InvalidOperationException if this instance is read-only. */
    void VerifyWritable() const {
        if (isReadOnly_) throw System::InvalidOperationException("Instance is read-only.");
    }

public:
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
    [[nodiscard]] static const NumberFormatInfo& getInvariantInfoProperty() {
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
    [[nodiscard]] static const NumberFormatInfo& getCurrentInfoProperty() {
        return getInvariantInfoProperty();
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

    // --- Number format ---
    /** @brief String that separates the integer from the fractional part. C++ counterpart of .NET NumberFormatInfo.NumberDecimalSeparator. */
    [[nodiscard]] const std::string& getNumberDecimalSeparatorProperty() const { return numberDecimalSeparator_; }
    void setNumberDecimalSeparatorProperty(const std::string& value) { VerifyWritable(); numberDecimalSeparator_ = value; }

    /** @brief String that separates groups of digits left of the decimal point. C++ counterpart of .NET NumberFormatInfo.NumberGroupSeparator. */
    [[nodiscard]] const std::string& getNumberGroupSeparatorProperty() const { return numberGroupSeparator_; }
    void setNumberGroupSeparatorProperty(const std::string& value) { VerifyWritable(); numberGroupSeparator_ = value; }

    /** @brief Number of decimal digits in numeric values. C++ counterpart of .NET NumberFormatInfo.NumberDecimalDigits. */
    [[nodiscard]] int getNumberDecimalDigitsProperty() const { return numberDecimalDigits_; }
    void setNumberDecimalDigitsProperty(int value) { VerifyWritable(); numberDecimalDigits_ = value; }

    /** @brief Format pattern for negative numeric values. C++ counterpart of .NET NumberFormatInfo.NumberNegativePattern. */
    [[nodiscard]] int getNumberNegativePatternProperty() const { return numberNegativePattern_; }
    void setNumberNegativePatternProperty(int value) { VerifyWritable(); numberNegativePattern_ = value; }

    /** @brief Number of digits in each group left of the decimal point. C++ counterpart of .NET NumberFormatInfo.NumberGroupSizes. Returns a copy, matching .NET. */
    [[nodiscard]] std::vector<int> getNumberGroupSizesProperty() const { return numberGroupSizes_; }
    void setNumberGroupSizesProperty(const std::vector<int>& value) { VerifyWritable(); numberGroupSizes_ = value; }

    // --- Currency format ---
    /** @brief String that separates the integer from the fractional part in currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyDecimalSeparator. */
    [[nodiscard]] const std::string& getCurrencyDecimalSeparatorProperty() const { return currencyDecimalSeparator_; }
    void setCurrencyDecimalSeparatorProperty(const std::string& value) { VerifyWritable(); currencyDecimalSeparator_ = value; }

    /** @brief String that separates groups of digits left of the decimal in currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyGroupSeparator. */
    [[nodiscard]] const std::string& getCurrencyGroupSeparatorProperty() const { return currencyGroupSeparator_; }
    void setCurrencyGroupSeparatorProperty(const std::string& value) { VerifyWritable(); currencyGroupSeparator_ = value; }

    /** @brief String used as the currency symbol. C++ counterpart of .NET NumberFormatInfo.CurrencySymbol. */
    [[nodiscard]] const std::string& getCurrencySymbolProperty() const { return currencySymbol_; }
    void setCurrencySymbolProperty(const std::string& value) { VerifyWritable(); currencySymbol_ = value; }

    /** @brief Number of decimal digits in currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyDecimalDigits. */
    [[nodiscard]] int getCurrencyDecimalDigitsProperty() const { return currencyDecimalDigits_; }
    void setCurrencyDecimalDigitsProperty(int value) { VerifyWritable(); currencyDecimalDigits_ = value; }

    /** @brief Format pattern for negative currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyNegativePattern. */
    [[nodiscard]] int getCurrencyNegativePatternProperty() const { return currencyNegativePattern_; }
    void setCurrencyNegativePatternProperty(int value) { VerifyWritable(); currencyNegativePattern_ = value; }

    /** @brief Format pattern for positive currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyPositivePattern. */
    [[nodiscard]] int getCurrencyPositivePatternProperty() const { return currencyPositivePattern_; }
    void setCurrencyPositivePatternProperty(int value) { VerifyWritable(); currencyPositivePattern_ = value; }

    /** @brief Number of digits in each group left of the decimal in currency values. C++ counterpart of .NET NumberFormatInfo.CurrencyGroupSizes. Returns a copy, matching .NET. */
    [[nodiscard]] std::vector<int> getCurrencyGroupSizesProperty() const { return currencyGroupSizes_; }
    void setCurrencyGroupSizesProperty(const std::vector<int>& value) { VerifyWritable(); currencyGroupSizes_ = value; }

    // --- Sign and special symbols ---
    /** @brief String that denotes a negative number. C++ counterpart of .NET NumberFormatInfo.NegativeSign. */
    [[nodiscard]] const std::string& getNegativeSignProperty() const { return negativeSign_; }
    void setNegativeSignProperty(const std::string& value) { VerifyWritable(); negativeSign_ = value; }

    /** @brief String that denotes a positive number. C++ counterpart of .NET NumberFormatInfo.PositiveSign. */
    [[nodiscard]] const std::string& getPositiveSignProperty() const { return positiveSign_; }
    void setPositiveSignProperty(const std::string& value) { VerifyWritable(); positiveSign_ = value; }

    /** @brief String that represents the IEEE NaN value. C++ counterpart of .NET NumberFormatInfo.NaNSymbol. */
    [[nodiscard]] const std::string& getNaNSymbolProperty() const { return naNSymbol_; }
    void setNaNSymbolProperty(const std::string& value) { VerifyWritable(); naNSymbol_ = value; }

    /** @brief String that represents positive infinity. C++ counterpart of .NET NumberFormatInfo.PositiveInfinitySymbol. */
    [[nodiscard]] const std::string& getPositiveInfinitySymbolProperty() const { return positiveInfinitySymbol_; }
    void setPositiveInfinitySymbolProperty(const std::string& value) { VerifyWritable(); positiveInfinitySymbol_ = value; }

    /** @brief String that represents negative infinity. C++ counterpart of .NET NumberFormatInfo.NegativeInfinitySymbol. */
    [[nodiscard]] const std::string& getNegativeInfinitySymbolProperty() const { return negativeInfinitySymbol_; }
    void setNegativeInfinitySymbolProperty(const std::string& value) { VerifyWritable(); negativeInfinitySymbol_ = value; }

    // --- Percent format ---
    /** @brief String to use as the percent symbol. C++ counterpart of .NET NumberFormatInfo.PercentSymbol. */
    [[nodiscard]] const std::string& getPercentSymbolProperty() const { return percentSymbol_; }
    void setPercentSymbolProperty(const std::string& value) { VerifyWritable(); percentSymbol_ = value; }

    /** @brief String to use as the per mille symbol (‰). C++ counterpart of .NET NumberFormatInfo.PerMilleSymbol. */
    [[nodiscard]] const std::string& getPerMilleSymbolProperty() const { return perMilleSymbol_; }
    void setPerMilleSymbolProperty(const std::string& value) { VerifyWritable(); perMilleSymbol_ = value; }

    /** @brief String that separates the integer from the fractional part in percent values. C++ counterpart of .NET NumberFormatInfo.PercentDecimalSeparator. */
    [[nodiscard]] const std::string& getPercentDecimalSeparatorProperty() const { return percentDecimalSeparator_; }
    void setPercentDecimalSeparatorProperty(const std::string& value) { VerifyWritable(); percentDecimalSeparator_ = value; }

    /** @brief String that separates groups of digits left of the decimal in percent values. C++ counterpart of .NET NumberFormatInfo.PercentGroupSeparator. */
    [[nodiscard]] const std::string& getPercentGroupSeparatorProperty() const { return percentGroupSeparator_; }
    void setPercentGroupSeparatorProperty(const std::string& value) { VerifyWritable(); percentGroupSeparator_ = value; }

    /** @brief Number of decimal digits in percent values. C++ counterpart of .NET NumberFormatInfo.PercentDecimalDigits. */
    [[nodiscard]] int getPercentDecimalDigitsProperty() const { return percentDecimalDigits_; }
    void setPercentDecimalDigitsProperty(int value) { VerifyWritable(); percentDecimalDigits_ = value; }

    /** @brief Format pattern for negative percent values. C++ counterpart of .NET NumberFormatInfo.PercentNegativePattern. */
    [[nodiscard]] int getPercentNegativePatternProperty() const { return percentNegativePattern_; }
    void setPercentNegativePatternProperty(int value) { VerifyWritable(); percentNegativePattern_ = value; }

    /** @brief Format pattern for positive percent values. C++ counterpart of .NET NumberFormatInfo.PercentPositivePattern. */
    [[nodiscard]] int getPercentPositivePatternProperty() const { return percentPositivePattern_; }
    void setPercentPositivePatternProperty(int value) { VerifyWritable(); percentPositivePattern_ = value; }

    /** @brief Number of digits in each group left of the decimal in percent values. C++ counterpart of .NET NumberFormatInfo.PercentGroupSizes. Returns a copy, matching .NET. */
    [[nodiscard]] std::vector<int> getPercentGroupSizesProperty() const { return percentGroupSizes_; }
    void setPercentGroupSizesProperty(const std::vector<int>& value) { VerifyWritable(); percentGroupSizes_ = value; }

    // --- Digit substitution ---
    /** @brief Specifies how a GUI displays the shape of a digit. C++ counterpart of .NET NumberFormatInfo.DigitSubstitution. */
    [[nodiscard]] DigitShapes getDigitSubstitutionProperty() const { return digitSubstitution_; }
    void setDigitSubstitutionProperty(DigitShapes value) { VerifyWritable(); digitSubstitution_ = value; }

    /** @brief Native digits equivalent to Western digits 0-9. C++ counterpart of .NET NumberFormatInfo.NativeDigits. Returns a copy, matching .NET. */
    [[nodiscard]] std::vector<std::string> getNativeDigitsProperty() const { return nativeDigits_; }
    void setNativeDigitsProperty(const std::vector<std::string>& value) { VerifyWritable(); nativeDigits_ = value; }

private:
    static NumberFormatInfo makeReadOnly() {
        NumberFormatInfo info;
        info.isReadOnly_ = true;
        return info;
    }
};

} // namespace System::Globalization
