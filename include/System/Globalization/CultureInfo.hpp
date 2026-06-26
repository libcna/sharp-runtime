// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Globalization {

    /**
     * @brief Provides information about a specific culture (locale).
     *
     * Partial C++ counterpart of .NET System.Globalization.CultureInfo.
     * Only InvariantCulture is meaningfully implemented; it is used for
     * locale-independent number formatting in ported C# code.
     *
     * @note Status: Stub — only InvariantCulture and Name provided.
     */
    class CultureInfo {
    private:
        std::string name_;
        bool isNeutral_;
        bool isReadOnly_;

        CultureInfo(const std::string& name, bool neutral, bool readOnly)
            : name_(name), isNeutral_(neutral), isReadOnly_(readOnly) {}

    public:
        /** Constructs a default (empty-name) CultureInfo. */
        CultureInfo() : name_(""), isNeutral_(false), isReadOnly_(false) {}
        /** Constructs a CultureInfo for the given culture name. */
        explicit CultureInfo(const std::string& name) : name_(name), isNeutral_(false), isReadOnly_(false) {}

        /** Gets the culture name (e.g. "en-US"). */
        [[nodiscard]] const std::string& getNameProperty()        const { return name_; }
        /** Returns true if this is a neutral culture (no region). */
        [[nodiscard]] bool               getIsNeutralCultureProperty() const { return isNeutral_; }
        /** Returns true if this CultureInfo is read-only. */
        [[nodiscard]] bool               getIsReadOnlyProperty()  const { return isReadOnly_; }

        /**
         * @brief Gets the CultureInfo object that is culture-independent (invariant).
         * Use this when you need locale-independent parsing/formatting.
         */
        [[nodiscard]] static const CultureInfo& InvariantCulture() {
            static CultureInfo instance("", true, true);
            return instance;
        }

        /** @brief Gets the CultureInfo representing the current thread culture. Stub — returns InvariantCulture. */
        [[nodiscard]] static const CultureInfo& CurrentCulture() {
            return InvariantCulture();
        }
    };

} // namespace System::Globalization
