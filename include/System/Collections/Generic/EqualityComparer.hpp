// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include "System/Collections/Generic/IEqualityComparer.hpp"

namespace System::Collections::Generic {

    template<typename T>
    class EqualityComparer : public IEqualityComparer<T> {
    protected:
        EqualityComparer() = default;
    public:
        virtual ~EqualityComparer() = default;

        virtual bool Equals(const T* x, const T* y) const = 0;
        virtual size_t GetHashCode(const T& obj) const = 0;

        static std::shared_ptr<EqualityComparer<T>> Default() {
            static auto instance = std::make_shared<DefaultImpl>();
            return instance;
        }

        static std::shared_ptr<EqualityComparer<T>> Create(
            std::function<bool(const T*, const T*)> equals,
            std::function<size_t(const T&)> getHashCode = nullptr)
        {
            return std::make_shared<DelegateImpl>(equals, getHashCode);
        }

    private:
        struct DefaultImpl : EqualityComparer<T> {
            bool Equals(const T* x, const T* y) const override {
                if (x == nullptr && y == nullptr) return true;
                if (x == nullptr || y == nullptr) return false;
                return *x == *y;
            }
            size_t GetHashCode(const T& obj) const override {
                return std::hash<T>{}(obj);
            }
        };

        struct DelegateImpl : EqualityComparer<T> {
            std::function<bool(const T*, const T*)> equals_;
            std::function<size_t(const T&)> getHashCode_;
            DelegateImpl(std::function<bool(const T*, const T*)> eq,
                         std::function<size_t(const T&)> hc)
                : equals_(eq), getHashCode_(hc) {}
            bool Equals(const T* x, const T* y) const override { return equals_(x, y); }
            size_t GetHashCode(const T& obj) const override {
                if (!getHashCode_) throw std::runtime_error("GetHashCode not supported.");
                return getHashCode_(obj);
            }
        };
    };

} // namespace System::Collections::Generic
