// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Globalization/CultureNotFoundException.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"

namespace System::Globalization {

/**
 * @brief Provides information about a specific culture (locale).
 *
 * C++ counterpart of .NET System.Globalization.CultureInfo.
 * This port has no real locale/ICU database (unlike real .NET's CultureData), so only two
 * cultures are meaningfully modeled: the invariant culture (name "") and "en-US" (the fixed
 * result of the LCID constructor). For any other culture name, name-derived properties
 * (EnglishName, NativeName, DisplayName) fall back to the name itself, and
 * TwoLetterISOLanguageName is derived heuristically from the name's leading subtag --
 * documented on each property rather than silently claiming full parity.
 * **CurrentCulture and CurrentUICulture are per-thread since #2409**, with .NET's process-wide
 * `DefaultThreadCurrentCulture` fallback beneath them and the invariant culture last. They used
 * to be process-wide statics -- a set on one thread changed what every other thread read, and a
 * concurrent get/set was a data race -- while this very comment and the accessors' own
 * doc-comments already said *thread*.
 */
class CultureInfo {
    std::string name_;
    bool isNeutral_;
    bool isReadOnly_;
    NumberFormatInfo numberFormat_;
    DateTimeFormatInfo dateTimeFormat_;

    CultureInfo(const std::string& name, bool neutral, bool readOnly)
        : name_(name), isNeutral_(neutral), isReadOnly_(readOnly),
          numberFormat_(readOnly ? NumberFormatInfo::ReadOnly(NumberFormatInfo::getInvariantInfoProperty())
                                  : NumberFormatInfo::getInvariantInfoProperty().Clone()),
          dateTimeFormat_(readOnly ? DateTimeFormatInfo::ReadOnly(DateTimeFormatInfo::getInvariantInfoProperty())
                                    : DateTimeFormatInfo::getInvariantInfoProperty().Clone()) {}

    /** @brief Throws InvalidOperationException if this instance is read-only. */
    void VerifyWritable() const {
        if (isReadOnly_) throw System::InvalidOperationException("Instance is read-only.");
    }

    /**
     * @brief Rejects the LCIDs .NET's CultureInfo(int) always rejects, and maps any other
     * value to "en-US" (this port has no real LCID-to-culture database).
     *
     * C++ counterpart of the validation portion of .NET CultureInfo(int, bool) (CultureInfo.cs):
     * negative values throw ArgumentOutOfRangeException; LOCALE_NEUTRAL (0x0000),
     * LOCALE_USER_DEFAULT (0x0400), LOCALE_SYSTEM_DEFAULT (0x0800),
     * LOCALE_CUSTOM_DEFAULT (0x0C00), and LOCALE_CUSTOM_UNSPECIFIED (0x1000) all throw
     * CultureNotFoundException.
     */
    static const std::string& ValidateLcidStub(int culture) {
        if (culture < 0) throw System::ArgumentOutOfRangeException("culture");
        constexpr int LocaleNeutral = 0x0000;
        constexpr int LocaleUserDefault = 0x0400;
        constexpr int LocaleSystemDefault = 0x0800;
        constexpr int LocaleCustomDefault = 0x0C00;
        constexpr int LocaleCustomUnspecified = 0x1000;
        if (culture == LocaleNeutral || culture == LocaleUserDefault || culture == LocaleSystemDefault ||
            culture == LocaleCustomDefault || culture == LocaleCustomUnspecified)
            throw CultureNotFoundException("culture", culture, "Culture is not supported.");
        static const std::string enUS = "en-US";
        return enUS;
    }

public:
    /**
     * @brief Default-constructs a CultureInfo with an empty culture name.
     *
     * C++ counterpart of the invariant-culture-like default state.
     */
    CultureInfo() : CultureInfo("", false, false) {}

    /**
     * @brief Constructs a CultureInfo for the given culture name.
     *
     * C++ counterpart of .NET CultureInfo(string).
     * @param name The culture name (e.g. "en-US").
     */
    explicit CultureInfo(const std::string& name) : CultureInfo(name, false, false) {}

    /**
     * @brief Constructs a CultureInfo for the given LCID.
     *
     * C++ counterpart of .NET CultureInfo(int).
     * This port has no real LCID-to-culture database; any LCID not rejected by
     * ValidateLcidStub() always builds "en-US" (see that helper's doc comment for exactly
     * which LCIDs are rejected and why).
     * @param culture The locale identifier.
     * @throws System::ArgumentOutOfRangeException if @p culture is negative.
     * @throws CultureNotFoundException if @p culture is a neutral, default, or
     *         unspecified-custom LCID.
     */
    explicit CultureInfo(int culture) : CultureInfo(ValidateLcidStub(culture), false, false) {}

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
     * @brief Gets the full localized culture name.
     *
     * C++ counterpart of .NET CultureInfo.DisplayName. This port has no real locale
     * database (CultureData); the invariant culture and "en-US" (the two cultures this
     * port meaningfully models) get their real .NET values, and any other culture name
     * falls back to Name itself rather than a fabricated display string.
     * @return The display name.
     */
    [[nodiscard]] std::string getDisplayNameProperty() const { return getEnglishNameProperty(); }

    /**
     * @brief Gets the culture name in English.
     *
     * C++ counterpart of .NET CultureInfo.EnglishName. See getDisplayNameProperty()'s doc
     * comment for the no-locale-database caveat.
     * @return The English name of the culture.
     */
    [[nodiscard]] std::string getEnglishNameProperty() const {
        if (name_.empty()) return "Invariant Language (Invariant Country)";
        if (name_ == "en-US") return "English (United States)";
        return name_;
    }

    /**
     * @brief Gets the culture name in its native language.
     *
     * C++ counterpart of .NET CultureInfo.NativeName. See getDisplayNameProperty()'s doc
     * comment for the no-locale-database caveat.
     * @return The native name of the culture.
     */
    [[nodiscard]] std::string getNativeNameProperty() const { return getEnglishNameProperty(); }

    /**
     * @brief Gets the ISO 639-1 two-letter language code.
     *
     * C++ counterpart of .NET CultureInfo.TwoLetterISOLanguageName. This port has no real
     * ISO-639 lookup table; the invariant culture and "en-US" get their real .NET values
     * ("iv" / "en"), and any other culture name is derived heuristically as the subtag
     * before the first '-' or '_', lowercased -- correct for standard "xx" / "xx-YY" BCP-47
     * names, but not guaranteed for exotic forms (e.g. script-subtag names like "zh-Hans-CN").
     * @return The two-letter ISO language code.
     */
    [[nodiscard]] std::string getTwoLetterISOLanguageNameProperty() const {
        if (name_.empty()) return "iv";
        if (name_ == "en-US") return "en";
        size_t sep = name_.find_first_of("-_");
        std::string lang = sep == std::string::npos ? name_ : name_.substr(0, sep);
        for (char& c : lang) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return lang;
    }

    /**
     * @brief Gets the ISO 639-2 three-letter language code.
     *
     * C++ counterpart of .NET CultureInfo.ThreeLetterISOLanguageName. Unlike the two-letter
     * code, the three-letter form cannot be heuristically derived from the culture name (it
     * isn't a prefix or transform of it) -- this port has real values only for the invariant
     * culture and "en-US"; any other culture name returns an empty string rather than a
     * fabricated code.
     * @return The three-letter ISO language code, or an empty string if not known.
     */
    [[nodiscard]] std::string getThreeLetterISOLanguageNameProperty() const {
        if (name_.empty()) return "ivl";
        if (name_ == "en-US") return "eng";
        return "";
    }

    /**
     * @brief Gets the NumberFormatInfo that defines number formatting for this culture.
     *
     * C++ counterpart of .NET CultureInfo.NumberFormat. This port has no real per-culture
     * formatting data, so every CultureInfo starts with a copy of the invariant
     * NumberFormatInfo (read-only if this culture is read-only); it can be replaced via the
     * setter like real .NET's mutable instance property.
     * @return A const reference to this culture's NumberFormatInfo.
     */
    [[nodiscard]] const NumberFormatInfo& getNumberFormatProperty() const { return numberFormat_; }

    /**
     * @brief Sets the NumberFormatInfo that defines number formatting for this culture.
     *
     * C++ counterpart of .NET CultureInfo.NumberFormat (setter).
     * @param value The new NumberFormatInfo.
     * @throws System::InvalidOperationException if this instance is read-only.
     */
    void setNumberFormatProperty(const NumberFormatInfo& value) {
        VerifyWritable();
        numberFormat_ = value;
    }

    /**
     * @brief Gets the DateTimeFormatInfo that defines date/time formatting for this culture.
     *
     * C++ counterpart of .NET CultureInfo.DateTimeFormat. See getNumberFormatProperty()'s
     * doc comment for the no-per-culture-data caveat.
     * @return A const reference to this culture's DateTimeFormatInfo.
     */
    [[nodiscard]] const DateTimeFormatInfo& getDateTimeFormatProperty() const { return dateTimeFormat_; }

    /**
     * @brief Sets the DateTimeFormatInfo that defines date/time formatting for this culture.
     *
     * C++ counterpart of .NET CultureInfo.DateTimeFormat (setter).
     * @param value The new DateTimeFormatInfo.
     * @throws System::InvalidOperationException if this instance is read-only.
     */
    void setDateTimeFormatProperty(const DateTimeFormatInfo& value) {
        VerifyWritable();
        dateTimeFormat_ = value;
    }

    /**
     * @brief Determines whether this CultureInfo is equal to another.
     *
     * C++ counterpart of .NET CultureInfo.Equals(object). Real .NET also compares
     * CompareInfo; this port doesn't model per-culture CompareInfo data, so Name equality
     * alone is used (documented deviation -- two CultureInfo instances with the same name
     * are for all practical purposes in this port identical anyway).
     * @param other The CultureInfo to compare.
     * @return true if both instances have the same Name.
     */
    [[nodiscard]] bool Equals(const CultureInfo& other) const { return name_ == other.name_; }

    /** @brief Equality operator; see Equals(). */
    bool operator==(const CultureInfo& other) const { return Equals(other); }

    /**
     * @brief Returns a hash code for this CultureInfo.
     *
     * C++ counterpart of .NET CultureInfo.GetHashCode(). Real .NET combines Name's hash
     * with CompareInfo's hash; this port has no per-culture CompareInfo, so only Name is
     * hashed (see Equals()'s doc comment for the same deviation).
     * @return A hash code derived from Name.
     */
    [[nodiscard]] int GetHashCode() const {
        return static_cast<int>(std::hash<std::string>{}(name_));
    }

    /**
     * @brief Returns the culture name.
     *
     * C++ counterpart of .NET CultureInfo.ToString(). Returns the same value as Name.
     * @return The culture name string.
     */
    [[nodiscard]] const std::string& ToString() const { return name_; }

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
     * @brief Gets the CultureInfo representing the current **thread's** culture.
     *
     * C++ counterpart of .NET `CultureInfo.CurrentCulture` (`CultureInfo.cs:358-366`), whose
     * resolution chain is transcribed here:
     * `s_currentThreadCulture ?? s_DefaultThreadCurrentCulture ?? s_userDefaultCulture`.
     *
     * @note **Until #2409 this was process-wide, and the doc-comment already said otherwise.** The
     * two current-culture members were plain `static`, so a set on one thread changed what every
     * other thread read, and a concurrent get/set was an unsynchronised read/write of a non-atomic
     * object -- a data race rather than merely a surprising value. .NET's is `[ThreadStatic]`
     * (`CultureInfo.cs:111-112`).
     *
     * @note **Why the process-wide setting did not simply go away.** Making the members
     * `thread_local` alone would have fixed the race and **silently removed** the process-wide
     * behaviour this port accidentally had, with no replacement and no diagnostic. .NET's own
     * answer is the second property, `DefaultThreadCurrentCulture` (`:407-413`), so that is what
     * this port grew: a thread with no setting of its own falls back to it, and only then to the
     * invariant culture.
     *
     * @return A const reference to the current culture. The reference stays valid even if another
     *         thread swaps the process-wide default immediately afterwards -- see
     *         `currentCultureHold_`.
     */
    [[nodiscard]] static const CultureInfo& getCurrentCultureProperty() {
        if (currentCulture_.has_value()) return *currentCulture_;
        // Park the loaded pointer in this thread's own holder before dereferencing it: another
        // thread may store a different default the instant after the load, and the caller holds a
        // REFERENCE. The holder keeps the object alive for as long as this thread can use it.
        currentCultureHold_ = defaultThreadCurrentCulture_.load(std::memory_order_acquire);
        if (currentCultureHold_) return *currentCultureHold_;
        return getInvariantCultureProperty();
    }

    /**
     * @brief The culture a thread uses when it has set none of its own.
     *
     * C++ counterpart of .NET `CultureInfo.DefaultThreadCurrentCulture` (`CultureInfo.cs:407-413`),
     * added by #2409. `std::nullopt` means "no process-wide default", in which case
     * `getCurrentCultureProperty()` falls through to the invariant culture, exactly as .NET's
     * `null` does.
     */
    [[nodiscard]] static std::optional<CultureInfo> getDefaultThreadCurrentCultureProperty() {
        const std::shared_ptr<const CultureInfo> value =
            defaultThreadCurrentCulture_.load(std::memory_order_acquire);
        if (!value) return std::nullopt;
        return *value;
    }

    /** @brief Sets the process-wide fallback culture. `CultureInfo.cs:407-413`. */
    static void setDefaultThreadCurrentCultureProperty(const std::optional<CultureInfo>& value) {
        defaultThreadCurrentCulture_.store(
            value.has_value() ? std::make_shared<const CultureInfo>(*value)
                              : std::shared_ptr<const CultureInfo>{},
            std::memory_order_release);
    }

    /**
     * @brief Sets the CultureInfo representing the current thread's culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentCulture (setter).
     * @param value The culture to make current.
     */
    /**
     * @brief Sets **this thread's** culture. `CultureInfo.cs:367-374`.
     *
     * @note Since #2409 this is visible to the calling thread only. A caller that wants every
     *       thread to see it wants `setDefaultThreadCurrentCultureProperty` instead -- which is
     *       .NET's answer to the same question, not this port's invention.
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
    /** @copydoc getCurrentCultureProperty */
    [[nodiscard]] static const CultureInfo& getCurrentUICultureProperty() {
        if (currentUICulture_.has_value()) return *currentUICulture_;
        currentUICultureHold_ = defaultThreadCurrentUICulture_.load(std::memory_order_acquire);
        if (currentUICultureHold_) return *currentUICultureHold_;
        return getInvariantCultureProperty();
    }

    /**
     * @brief The UI culture a thread uses when it has set none of its own.
     * C++ counterpart of .NET `CultureInfo.DefaultThreadCurrentUICulture`.
     */
    [[nodiscard]] static std::optional<CultureInfo> getDefaultThreadCurrentUICultureProperty() {
        const std::shared_ptr<const CultureInfo> value =
            defaultThreadCurrentUICulture_.load(std::memory_order_acquire);
        if (!value) return std::nullopt;
        return *value;
    }

    /** @brief Sets the process-wide fallback UI culture. */
    static void setDefaultThreadCurrentUICultureProperty(const std::optional<CultureInfo>& value) {
        defaultThreadCurrentUICulture_.store(
            value.has_value() ? std::make_shared<const CultureInfo>(*value)
                              : std::shared_ptr<const CultureInfo>{},
            std::memory_order_release);
    }

    /**
     * @brief Sets the CultureInfo representing the current thread's UI culture.
     *
     * C++ counterpart of .NET CultureInfo.CurrentUICulture (setter).
     * @param value The UI culture to make current.
     */
    /** @brief Sets **this thread's** UI culture. See `setCurrentCultureProperty`. */
    static void setCurrentUICultureProperty(const CultureInfo& value) {
        currentUICulture_ = value;
    }

    /**
     * @brief Gets a read-only CultureInfo for the given culture name.
     *
     * C++ counterpart of .NET CultureInfo.GetCultureInfo(string). Unlike the public
     * CultureInfo(string) constructor, the returned instance is read-only, matching real
     * .NET's cached-and-frozen result. Real .NET also caches by name so repeated calls
     * return the identical object; this port has value semantics throughout (no object
     * identity), so this returns an equal but distinct instance each call -- a deliberate,
     * documented simplification, not a bug.
     * @param name The culture name (e.g. "en-US").
     * @return A read-only CultureInfo for @p name.
     */
    [[nodiscard]] static CultureInfo GetCultureInfo(const std::string& name) {
        CultureInfo ci(name, false, true);
        return ci;
    }

    /**
     * @brief Gets a read-only CultureInfo for the given LCID.
     *
     * C++ counterpart of .NET CultureInfo.GetCultureInfo(int). See the CultureInfo(int)
     * constructor's doc comment for which LCIDs are rejected and the no-LCID-database
     * caveat for any other value.
     * @param culture The locale identifier.
     * @return A read-only CultureInfo for @p culture.
     * @throws System::ArgumentOutOfRangeException if @p culture is negative.
     * @throws CultureNotFoundException if @p culture is a neutral, default, or
     *         unspecified-custom LCID.
     */
    [[nodiscard]] static CultureInfo GetCultureInfo(int culture) {
        return CultureInfo(ValidateLcidStub(culture), false, true);
    }

    /**
     * @brief Gets a read-only CultureInfo for the given culture name.
     *
     * C++ counterpart of .NET CultureInfo.GetCultureInfo(string, bool). Real .NET validates
     * @p name against a predefined-locale table when @p predefinedOnly is true; this port
     * has no such table, so @p predefinedOnly is accepted but not enforced (documented
     * deviation -- it never rejects a name it wouldn't already accept).
     * @param name           The culture name (e.g. "en-US").
     * @param predefinedOnly Accepted for API compatibility; not enforced in this port.
     * @return A read-only CultureInfo for @p name.
     */
    [[nodiscard]] static CultureInfo GetCultureInfo(const std::string& name, bool predefinedOnly) {
        (void)predefinedOnly;
        return GetCultureInfo(name);
    }

private:
    // #2409. THREE PIECES OF STATE PER CULTURE, AND EACH ONE EARNS ITS PLACE.
    //
    //  * `current*_`      -- `thread_local`, and ABSENT by default. This is .NET's
    //                        `[ThreadStatic] s_currentThreadCulture`. Absent rather than
    //                        invariant-valued because "this thread has not chosen" and "this
    //                        thread chose the invariant culture" are different facts, and only
    //                        the first may fall through to the process-wide default.
    //  * `defaultThread*_` -- the process-wide fallback, .NET's `DefaultThreadCurrentCulture`.
    //                        An `atomic<shared_ptr<const CultureInfo>>` rather than a plain
    //                        static: it is genuinely shared, so it must be swapped atomically,
    //                        and `CultureInfo` holds `NumberFormatInfo` and `DateTimeFormatInfo`
    //                        BY VALUE, so a per-read deep copy would be paid on every formatting
    //                        call. A pointer swap costs a refcount bump.
    //  * `*Hold_`          -- `thread_local`. The getters return `const CultureInfo&`, so the
    //                        loaded pointer must outlive the load: without this holder another
    //                        thread's store could drop the last reference while the caller still
    //                        held the reference, which would have moved the race one level down
    //                        rather than removing it.
    static thread_local std::optional<CultureInfo> currentCulture_;
    static thread_local std::optional<CultureInfo> currentUICulture_;
    static std::atomic<std::shared_ptr<const CultureInfo>> defaultThreadCurrentCulture_;
    static std::atomic<std::shared_ptr<const CultureInfo>> defaultThreadCurrentUICulture_;
    static thread_local std::shared_ptr<const CultureInfo> currentCultureHold_;
    static thread_local std::shared_ptr<const CultureInfo> currentUICultureHold_;
};

// Absent by default: a thread that has chosen nothing falls through to the process-wide default
// and then to the invariant culture, which is .NET's chain
// (`s_currentThreadCulture ?? s_DefaultThreadCurrentCulture ?? s_userDefaultCulture`).
inline thread_local std::optional<CultureInfo> CultureInfo::currentCulture_{};
inline thread_local std::optional<CultureInfo> CultureInfo::currentUICulture_{};
inline std::atomic<std::shared_ptr<const CultureInfo>> CultureInfo::defaultThreadCurrentCulture_{};
inline std::atomic<std::shared_ptr<const CultureInfo>> CultureInfo::defaultThreadCurrentUICulture_{};
inline thread_local std::shared_ptr<const CultureInfo> CultureInfo::currentCultureHold_{};
inline thread_local std::shared_ptr<const CultureInfo> CultureInfo::currentUICultureHold_{};

} // namespace System::Globalization
