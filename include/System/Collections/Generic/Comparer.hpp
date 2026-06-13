// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/Collections/Generic/EqualityComparer.hpp"

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
        /// Destroys the comparer.
        virtual ~Comparer() = default;
        /// When overridden in a derived class, compares two objects and returns an ordering integer.
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


} // namespace System::Collections::Generic
