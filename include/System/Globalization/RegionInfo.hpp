// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>

namespace System::Globalization {

/**
 * @brief Provides information about a specific region (country/area).
 *
 * C++ counterpart of .NET System.Globalization.RegionInfo.
 * Only the US region is fully initialised; all other names are passed through
 * from the constructor argument. Currency and metric defaults are for the US.
 */
class RegionInfo {
    std::string name_;
    std::string englishName_;
    std::string nativeName_;
    std::string twoLetterISORegionName_;
    std::string threeLetterISORegionName_;
    std::string currencySymbol_;
    std::string isoCurrencySymbol_;
    bool isMetric_ = true;

public:
    /**
     * @brief Constructs a RegionInfo for the region identified by @p name.
     *
     * C++ counterpart of .NET RegionInfo(string).
     * Only minimal initialisation is performed; all names are derived from
     * the supplied identifier string.
     * @param name The region name or identifier (e.g. "US").
     */
    explicit RegionInfo(const std::string& name)
        : name_(name), englishName_(name), nativeName_(name),
          twoLetterISORegionName_(name.size() >= 2 ? name.substr(0, 2) : name),
          threeLetterISORegionName_(name.size() >= 3 ? name.substr(0, 3) : name),
          currencySymbol_("$"), isoCurrencySymbol_("USD") {}

    /**
     * @brief Constructs a RegionInfo from a Windows LCID.
     *
     * C++ counterpart of .NET RegionInfo(int).
     * Stub — always creates a US-region instance.
     * @param culture The Windows locale identifier (ignored in this stub).
     */
    explicit RegionInfo(int /*culture*/)
        : RegionInfo("US") {}

    /**
     * @brief Gets the region identifier (e.g. "US").
     *
     * C++ counterpart of .NET RegionInfo.Name.
     * @return The region name string.
     */
    [[nodiscard]] const std::string& getNameProperty() const { return name_; }

    /**
     * @brief Gets the region name in English.
     *
     * C++ counterpart of .NET RegionInfo.EnglishName.
     * @return The English name of the region.
     */
    [[nodiscard]] const std::string& getEnglishNameProperty() const { return englishName_; }

    /**
     * @brief Gets the full region name in English, suitable for display.
     *
     * C++ counterpart of .NET RegionInfo.DisplayName.
     * Stub — returns the same value as EnglishName.
     * @return The display name of the region.
     */
    [[nodiscard]] const std::string& getDisplayNameProperty() const { return englishName_; }

    /**
     * @brief Gets the region name in the native language.
     *
     * C++ counterpart of .NET RegionInfo.NativeName.
     * @return The native name of the region.
     */
    [[nodiscard]] const std::string& getNativeNameProperty() const { return nativeName_; }

    /**
     * @brief Gets the two-letter ISO 3166 region code.
     *
     * C++ counterpart of .NET RegionInfo.TwoLetterISORegionName.
     * @return The two-letter region code (e.g. "US").
     */
    [[nodiscard]] const std::string& getTwoLetterISORegionNameProperty() const {
        return twoLetterISORegionName_;
    }

    /**
     * @brief Gets the three-letter ISO 3166 region code.
     *
     * C++ counterpart of .NET RegionInfo.ThreeLetterISORegionName.
     * @return The three-letter ISO region code (e.g. "USA").
     */
    [[nodiscard]] const std::string& getThreeLetterISORegionNameProperty() const {
        return threeLetterISORegionName_;
    }

    /**
     * @brief Gets the three-letter Windows region name.
     *
     * C++ counterpart of .NET RegionInfo.ThreeLetterWindowsRegionName.
     * Stub — returns the same value as ThreeLetterISORegionName.
     * @return The three-letter Windows region name.
     */
    [[nodiscard]] const std::string& getThreeLetterWindowsRegionNameProperty() const {
        return threeLetterISORegionName_;
    }

    /**
     * @brief Gets the currency symbol for this region.
     *
     * C++ counterpart of .NET RegionInfo.CurrencySymbol.
     * @return The currency symbol string (e.g. "$").
     */
    [[nodiscard]] const std::string& getCurrencySymbolProperty() const { return currencySymbol_; }

    /**
     * @brief Gets the ISO 4217 currency code for this region.
     *
     * C++ counterpart of .NET RegionInfo.ISOCurrencySymbol.
     * @return The ISO currency code (e.g. "USD").
     */
    [[nodiscard]] const std::string& getISOCurrencySymbolProperty() const { return isoCurrencySymbol_; }

    /**
     * @brief Gets the English name of the currency used in this region.
     *
     * C++ counterpart of .NET RegionInfo.CurrencyEnglishName.
     * Stub — always returns "US Dollar".
     * @return The English name of the currency.
     */
    [[nodiscard]] std::string getCurrencyEnglishNameProperty() const { return "US Dollar"; }

    /**
     * @brief Gets the native name of the currency used in this region.
     *
     * C++ counterpart of .NET RegionInfo.CurrencyNativeName.
     * Stub — always returns "Dollar".
     * @return The native name of the currency.
     */
    [[nodiscard]] std::string getCurrencyNativeNameProperty() const { return "Dollar"; }

    /**
     * @brief Gets the geography identifier for this region.
     *
     * C++ counterpart of .NET RegionInfo.GeoId.
     * Stub — always returns -1.
     * @return Always -1.
     */
    [[nodiscard]] int getGeoIdProperty() const { return -1; }

    /**
     * @brief Gets a value indicating whether the region uses the metric system.
     *
     * C++ counterpart of .NET RegionInfo.IsMetric.
     * @return true if the region uses the metric system; otherwise false.
     */
    [[nodiscard]] bool getIsMetricProperty() const { return isMetric_; }

    /**
     * @brief Determines whether this RegionInfo is equal to another.
     *
     * C++ counterpart of .NET RegionInfo.Equals(object).
     * @param other The RegionInfo to compare.
     * @return true if both instances have the same region name.
     */
    [[nodiscard]] bool Equals(const RegionInfo& other) const {
        return name_ == other.name_;
    }

    /**
     * @brief Returns a hash code for this RegionInfo.
     *
     * C++ counterpart of .NET RegionInfo.GetHashCode().
     * @return A hash based on the region name.
     */
    [[nodiscard]] int GetHashCode() const {
        return static_cast<int>(std::hash<std::string>{}(name_));
    }

    /**
     * @brief Returns a string representation of this RegionInfo.
     *
     * C++ counterpart of .NET RegionInfo.ToString().
     * @return The region name string.
     */
    [[nodiscard]] std::string ToString() const { return name_; }

    /**
     * @brief Returns a RegionInfo representing the default region (US).
     *
     * C++ counterpart of .NET RegionInfo.CurrentRegion.
     * Stub — always returns the US region.
     * @return A const reference to the US RegionInfo instance.
     */
    static const RegionInfo& getCurrentRegionProperty() {
        static RegionInfo r("US");
        return r;
    }
};

} // namespace System::Globalization
