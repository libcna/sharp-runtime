// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Collections::Generic {

    /**
     * @brief Provides a base class for comparison implementations.
     *
     * Partial C++ counterpart of .NET System.Collections.Generic.Comparer<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class Comparer {
    public:
        virtual ~Comparer() = default;
        virtual int Compare(const T& x, const T& y) const = 0;

        /** @brief Returns a default comparer that uses operator<. */
        static const Comparer<T>& Default() {
            static struct DefaultComparer : Comparer<T> {
                int Compare(const T& x, const T& y) const override {
                    if (x < y) return -1;
                    if (y < x) return  1;
                    return 0;
                }
            } instance;
            return instance;
        }
    };

    /**
     * @brief Provides a base class for equality comparison implementations.
     *
     * Partial C++ counterpart of .NET System.Collections.Generic.EqualityComparer<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class EqualityComparer {
    public:
        virtual ~EqualityComparer() = default;
        virtual bool Equals(const T& x, const T& y) const = 0;
        virtual int  GetHashCode(const T& obj) const = 0;

        /** @brief Returns a default equality comparer that uses operator== and std::hash. */
        static const EqualityComparer<T>& Default() {
            static struct DefaultEqualityComparer : EqualityComparer<T> {
                bool Equals(const T& x, const T& y) const override { return x == y; }
                int  GetHashCode(const T& obj) const override {
                    return static_cast<int>(std::hash<T>{}(obj));
                }
            } instance;
            return instance;
        }
    };

} // namespace System::Collections::Generic
