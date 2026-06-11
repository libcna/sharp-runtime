// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <stdexcept>

namespace System::Collections::Generic {

    /**
     * @brief Provides a base class for equality comparison implementations.
     *
     * Partial C++ counterpart of .NET System.Collections.Generic.EqualityComparer<T>.
     *
     * @note Status: Partial — value-based Equals/GetHashCode; Default() by const-ref,
     *       Create() by shared_ptr.
     */
    template<typename T>
    class EqualityComparer {
    public:
        virtual ~EqualityComparer() = default;

        virtual bool   Equals(const T& x, const T& y) const = 0;
        virtual size_t GetHashCode(const T& obj) const = 0;

        /** @brief Returns a default equality comparer using operator== and std::hash. */
        static const EqualityComparer<T>& Default() {
            static struct DefaultImpl : EqualityComparer<T> {
                bool   Equals(const T& x, const T& y) const override { return x == y; }
                size_t GetHashCode(const T& obj) const override { return std::hash<T>{}(obj); }
            } instance;
            return instance;
        }

        /** @brief Creates a custom equality comparer from lambda functions. */
        static std::shared_ptr<EqualityComparer<T>> Create(
            std::function<bool(const T&, const T&)> equals,
            std::function<size_t(const T&)> getHashCode = nullptr)
        {
            struct DelegateImpl : EqualityComparer<T> {
                std::function<bool(const T&, const T&)> equals_;
                std::function<size_t(const T&)> getHashCode_;
                DelegateImpl(std::function<bool(const T&, const T&)> eq,
                             std::function<size_t(const T&)> hc)
                    : equals_(eq), getHashCode_(hc) {}
                bool Equals(const T& x, const T& y) const override { return equals_(x, y); }
                size_t GetHashCode(const T& obj) const override {
                    if (!getHashCode_) throw std::runtime_error("GetHashCode not provided.");
                    return getHashCode_(obj);
                }
            };
            return std::make_shared<DelegateImpl>(equals, getHashCode);
        }
    };

} // namespace System::Collections::Generic
