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

    class StringComparer {
    public:
        virtual ~StringComparer() = default;
        [[nodiscard]] virtual int  Compare(const std::string& x, const std::string& y) const = 0;
        [[nodiscard]] virtual bool Equals(const std::string& x, const std::string& y)  const = 0;
        [[nodiscard]] virtual std::size_t GetHashCode(const std::string& s) const = 0;

        // Pre-built instances
        static std::shared_ptr<StringComparer> Ordinal();
        static std::shared_ptr<StringComparer> OrdinalIgnoreCase();
        static std::shared_ptr<StringComparer> InvariantCulture();
        static std::shared_ptr<StringComparer> InvariantCultureIgnoreCase();
        // CurrentCulture falls back to Ordinal in this port
        static std::shared_ptr<StringComparer> CurrentCulture();
        static std::shared_ptr<StringComparer> CurrentCultureIgnoreCase();
    };

    namespace detail {
        class OrdinalStringComparer final : public StringComparer {
        public:
            int Compare(const std::string& x, const std::string& y) const override {
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            bool Equals(const std::string& x, const std::string& y) const override { return x == y; }
            std::size_t GetHashCode(const std::string& s) const override { return std::hash<std::string>{}(s); }
        };

        inline std::string toLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            return s;
        }

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
