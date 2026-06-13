// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Globalization {

/// <summary>Provides information about a specific region (country/area).</summary>
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
    /// Constructs a RegionInfo for the region identified by @p name (e.g. "US").
    explicit RegionInfo(const std::string& name) : name_(name),
        englishName_(name), nativeName_(name),
        twoLetterISORegionName_(name.substr(0, 2)),
        threeLetterISORegionName_(name.substr(0, 3)),
        currencySymbol_("$"), isoCurrencySymbol_("USD") {}

    /// @return The region identifier (e.g. "US").
    [[nodiscard]] const std::string& getNameProperty()                const { return name_; }
    /// @return The region name in English.
    [[nodiscard]] const std::string& getEnglishNameProperty()         const { return englishName_; }
    /// @return The region name in the native language.
    [[nodiscard]] const std::string& getNativeNameProperty()          const { return nativeName_; }
    /// @return The two-letter ISO 3166 region code (e.g. "US").
    [[nodiscard]] const std::string& getTwoLetterISORegionNameProperty()   const { return twoLetterISORegionName_; }
    /// @return The three-letter ISO 3166 region code (e.g. "USA").
    [[nodiscard]] const std::string& getThreeLetterISORegionNameProperty() const { return threeLetterISORegionName_; }
    /// @return The currency symbol for this region (e.g. "$").
    [[nodiscard]] const std::string& getCurrencySymbolProperty()      const { return currencySymbol_; }
    /// @return The ISO 4217 currency code (e.g. "USD").
    [[nodiscard]] const std::string& getISOCurrencySymbolProperty()   const { return isoCurrencySymbol_; }
    /// @return True if the region uses the metric system.
    [[nodiscard]] bool getIsMetricProperty() const { return isMetric_; }

    /// @return A RegionInfo for the default region ("US").
    static const RegionInfo& CurrentRegion() {
        static RegionInfo r("US");
        return r;
    }
};

} // namespace System::Globalization
