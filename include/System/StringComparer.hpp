// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>

namespace System {

    /// <summary>Represents a string comparison operation that uses specific case and culture-based rules.</summary>
    class StringComparer {
    public:
        virtual ~StringComparer() = default;

        /// Compares two strings and returns negative, zero, or positive.
        /// @param x First string.
        /// @param y Second string.
        /// @return Negative if x < y, zero if equal, positive if x > y.
        [[nodiscard]] virtual int  Compare(const std::string& x, const std::string& y) const = 0;

        /// Returns true if @p x and @p y are considered equal by this comparer.
        [[nodiscard]] virtual bool Equals(const std::string& x, const std::string& y)  const = 0;

        /// Returns a hash code for @p s consistent with this comparer's equality semantics.
        [[nodiscard]] virtual std::size_t GetHashCode(const std::string& s) const = 0;

        /// Returns a comparer that performs case-sensitive ordinal string comparisons.
        static std::shared_ptr<StringComparer> Ordinal();
        /// Returns a comparer that performs case-insensitive ordinal string comparisons.
        static std::shared_ptr<StringComparer> OrdinalIgnoreCase();
        /// Returns a comparer that uses invariant-culture rules (maps to Ordinal in this port).
        static std::shared_ptr<StringComparer> InvariantCulture();
        /// Returns a comparer that uses case-insensitive invariant-culture rules.
        static std::shared_ptr<StringComparer> InvariantCultureIgnoreCase();
        /// Returns a comparer for the current culture (falls back to Ordinal in this port).
        static std::shared_ptr<StringComparer> CurrentCulture();
        /// Returns a case-insensitive comparer for the current culture.
        static std::shared_ptr<StringComparer> CurrentCultureIgnoreCase();
    };

    namespace detail {
        /// Ordinal (byte-by-byte) case-sensitive string comparer.
        class OrdinalStringComparer final : public StringComparer {
        public:
            int Compare(const std::string& x, const std::string& y) const override {
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            bool Equals(const std::string& x, const std::string& y) const override { return x == y; }
            std::size_t GetHashCode(const std::string& s) const override { return std::hash<std::string>{}(s); }
        };

        /// Helper: returns a lower-cased copy of @p s using the C locale.
        inline std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            return s;
        }

        /// Ordinal case-insensitive string comparer.
        class OrdinalIgnoreCaseStringComparer final : public StringComparer {
        public:
            int Compare(const std::string& x, const std::string& y) const override {
                auto lx = toLower(x), ly = toLower(y);
                return lx < ly ? -1 : (lx > ly ? 1 : 0);
            }
            bool Equals(const std::string& x, const std::string& y) const override {
                return toLower(x) == toLower(y);
            }
            std::size_t GetHashCode(const std::string& s) const override {
                return std::hash<std::string>{}(toLower(s));
            }
        };
    } // namespace detail

    inline std::shared_ptr<StringComparer> StringComparer::Ordinal() {
        static auto inst = std::make_shared<detail::OrdinalStringComparer>();
        return inst;
    }
    inline std::shared_ptr<StringComparer> StringComparer::OrdinalIgnoreCase() {
        static auto inst = std::make_shared<detail::OrdinalIgnoreCaseStringComparer>();
        return inst;
    }
    inline std::shared_ptr<StringComparer> StringComparer::InvariantCulture() { return Ordinal(); }
    inline std::shared_ptr<StringComparer> StringComparer::InvariantCultureIgnoreCase() { return OrdinalIgnoreCase(); }
    inline std::shared_ptr<StringComparer> StringComparer::CurrentCulture() { return Ordinal(); }
    inline std::shared_ptr<StringComparer> StringComparer::CurrentCultureIgnoreCase() { return OrdinalIgnoreCase(); }

} // namespace System
