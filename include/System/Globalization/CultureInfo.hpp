// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Globalization {

/**
 * @brief Provides information about a specific culture (locale).
 *
 * C++ counterpart of .NET System.Globalization.CultureInfo.
 * Only InvariantCulture is meaningfully implemented; it is used for
 * locale-independent number formatting in ported C# code.
 * CurrentCulture and CurrentUICulture return InvariantCulture as stubs.
 */
class CultureInfo {
    std::string name_;
    bool isNeutral_;
    bool isReadOnly_;

    CultureInfo(const std::string& name, bool neutral, bool readOnly)
        : name_(name), isNeutral_(neutral), isReadOnly_(readOnly) {}

public:
    /**
     * @brief Default-constructs a CultureInfo with an empty culture name.
     *
     * C++ counterpart of the invariant-culture-like default state.
     */
    CultureInfo() : name_(""), isNeutral_(false), isReadOnly_(false) {}

    /**
     * @brief Constructs a CultureInfo for the given culture name.
     *
     * C++ counterpart of .NET CultureInfo(string).
     * @param name The culture name (e.g. "en-US").
     */
    explicit CultureInfo(const std::string& name)
        : name_(name), isNeutral_(false), isReadOnly_(false) {}

    /**
     * @brief Constructs a CultureInfo for the given LCID.
     *
     * C++ counterpart of .NET CultureInfo(int).
     * The LCID is accepted for API compatibility but ignored.
     * @param culture The locale identifier; ignored in this implementation.
     */
    explicit CultureInfo(int /*culture*/)
        : name_("en-US"), isNeutral_(false), isReadOnly_(false) {}

    /**
     * @brief Gets the culture name (e.g. "en-US").
     *
     * C++ counterpart of .NET CultureInfo.Name.
     * @return The culture name string.
     */
    [[nodiscard]] const std::string& getNameProperty() const { return name_; }

    /**
     * @brief Gets a value indicating whether this is a neutral culture (no region).
     *
     * C++ counterpart of .NET CultureInfo.IsNeutralCulture.
     * @return true if this is a neutral culture; otherwise false.
     */
    [[nodiscard]] bool getIsNeutralCultureProperty() const { return isNeutral_; }

    /**
     * @brief Gets a value indicating whether this CultureInfo is read-only.
     *
     * C++ counterpart of .NET CultureInfo.IsReadOnly.
     * @return true if the instance is read-only; otherwise false.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

    /**
     * @brief Gets the culture-independent (invariant) CultureInfo instance.
     *
     * C++ counterpart of .NET CultureInfo.InvariantCulture.
     * Use this for locale-independent parsing and formatting.
     * @return A const reference to the shared invariant instance.
     */
    [[nodiscard]] static const CultureInfo& getInvariantCultureProperty() {
        // Real .NET's invariant culture has IsNeutralCulture == false (CultureData.cs:
        // `invariant._bNeutral = false;`) -- it's read-only, but not "neutral" (a neutral
        // culture is a language without a specific region, e.g. "en"; invariant is neither
        // neutral nor specific).
        static CultureInfo instance("", false, true);
        return instance;
    }

    /**
     * @brief Gets the CultureInfo representing the current thread's culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentCulture. Defaults to InvariantCulture
     * (this runtime has no OS-locale detection); use setCurrentCultureProperty() to
     * override, matching .NET's settable CurrentCulture property.
     * @return A const reference to the current culture.
     */
    [[nodiscard]] static const CultureInfo& getCurrentCultureProperty() {
        return currentCulture_;
    }

    /**
     * @brief Sets the CultureInfo representing the current thread's culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentCulture (setter).
     * @param value The culture to make current.
     */
    static void setCurrentCultureProperty(const CultureInfo& value) {
        currentCulture_ = value;
    }

    /**
     * @brief Gets the CultureInfo representing the current thread's UI culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentUICulture. Defaults to InvariantCulture;
     * use setCurrentUICultureProperty() to override.
     * @return A const reference to the current UI culture.
     */
    [[nodiscard]] static const CultureInfo& getCurrentUICultureProperty() {
        return currentUICulture_;
    }

    /**
     * @brief Sets the CultureInfo representing the current thread's UI culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentUICulture (setter).
     * @param value The UI culture to make current.
     */
    static void setCurrentUICultureProperty(const CultureInfo& value) {
        currentUICulture_ = value;
    }

private:
    static CultureInfo currentCulture_;
    static CultureInfo currentUICulture_;
};

// Default value matches InvariantCulture: name "", not neutral (real .NET's invariant
// culture has IsNeutralCulture == false, CultureData.cs), read-only until explicitly set.
inline CultureInfo CultureInfo::currentCulture_{"", false, true};
inline CultureInfo CultureInfo::currentUICulture_{"", false, true};

} // namespace System::Globalization
