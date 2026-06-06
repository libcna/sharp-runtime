// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Globalization {

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
        explicit RegionInfo(const std::string& name) : name_(name),
            englishName_(name), nativeName_(name),
            twoLetterISORegionName_(name.substr(0, 2)),
            threeLetterISORegionName_(name.substr(0, 3)),
            currencySymbol_("$"), isoCurrencySymbol_("USD") {}

        [[nodiscard]] const std::string& getNameProperty()                const { return name_; }
        [[nodiscard]] const std::string& getEnglishNameProperty()         const { return englishName_; }
        [[nodiscard]] const std::string& getNativeNameProperty()          const { return nativeName_; }
        [[nodiscard]] const std::string& getTwoLetterISORegionNameProperty()   const { return twoLetterISORegionName_; }
        [[nodiscard]] const std::string& getThreeLetterISORegionNameProperty() const { return threeLetterISORegionName_; }
        [[nodiscard]] const std::string& getCurrencySymbolProperty()      const { return currencySymbol_; }
        [[nodiscard]] const std::string& getISOCurrencySymbolProperty()   const { return isoCurrencySymbol_; }
        [[nodiscard]] bool getIsMetricProperty() const { return isMetric_; }

        static const RegionInfo& CurrentRegion() {
            static RegionInfo r("US");
            return r;
        }
    };

} // namespace System::Globalization
