// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/IComparer.hpp"
#include "System/Collections/IEqualityComparer.hpp"
#include "System/Collections/IStructuralComparable.hpp"
#include "System/Collections/IStructuralEquatable.hpp"

namespace System::Collections {

using SharpRuntime::intcs;

/**
 * @brief Provides objects for performing a structural comparison of two collection objects.
 *
 * C++ counterpart of .NET System.Collections.StructuralComparisons.
 * Provides singleton instances of IComparer and IEqualityComparer that delegate
 * to IStructuralComparable and IStructuralEquatable when the objects implement them.
 */
class StructuralComparisons {
public:
    StructuralComparisons() = delete;

    /**
     * @brief Gets an IComparer object that performs structural comparison.
     *
     * C++ counterpart of .NET StructuralComparisons.StructuralComparer.
     * Delegates to IStructuralComparable::CompareTo when the left operand implements it;
     * otherwise falls back to pointer ordering.
     */
    static const IComparer& getStructuralComparerProperty();

    /**
     * @brief Gets an IEqualityComparer object that performs structural equality comparison.
     *
     * C++ counterpart of .NET StructuralComparisons.StructuralEqualityComparer.
     * Delegates to IStructuralEquatable::Equals/GetHashCode when the object implements them;
     * otherwise falls back to pointer equality and address-based hash.
     */
    static const IEqualityComparer& getStructuralEqualityComparerProperty();
};

// -----------------------------------------------------------------------
// Implementation detail — private comparers defined in this header
// -----------------------------------------------------------------------

namespace detail {

class StructuralComparerImpl final : public IComparer {
public:
    /**
     * @brief Compares two objects structurally.
     *
     * Delegates to IStructuralComparable::CompareTo if the left object implements
     * IStructuralComparable; otherwise falls back to pointer ordering.
     * @param x Left operand (pointer to the object).
     * @param y Right operand (pointer to the object).
     * @return Negative, zero, or positive.
     */
    [[nodiscard]] intcs Compare(const void* x, const void* y) const override {
        if (x == y) return 0;
        if (!x)     return -1;
        if (!y)     return  1;
        const auto* sc = static_cast<const IStructuralComparable*>(x);
        return sc->CompareTo(y, *this);
    }
};

class StructuralEqualityComparerImpl final : public IEqualityComparer {
public:
    /**
     * @brief Determines structural equality of two objects.
     *
     * Delegates to IStructuralEquatable::Equals if the left object implements
     * IStructuralEquatable; otherwise falls back to pointer equality.
     * @param x Left operand.
     * @param y Right operand.
     * @return true if structurally equal.
     */
    [[nodiscard]] bool Equals(const void* x, const void* y) const override {
        if (x == y) return true;
        if (!x || !y) return false;
        const auto* se = static_cast<const IStructuralEquatable*>(x);
        return se->Equals(y, *this);
    }

    /**
     * @brief Returns a structural hash code for an object.
     *
     * Delegates to IStructuralEquatable::GetHashCode if the object implements
     * IStructuralEquatable; otherwise falls back to address-based hash.
     * @param obj The object to hash.
     * @return Hash code.
     */
    [[nodiscard]] intcs GetHashCode(const void* obj) const override {
        if (!obj) return 0;
        const auto* se = static_cast<const IStructuralEquatable*>(obj);
        return se->GetHashCode(*this);
    }
};

} // namespace detail

inline const IComparer& StructuralComparisons::getStructuralComparerProperty() {
    static detail::StructuralComparerImpl instance;
    return instance;
}

inline const IEqualityComparer& StructuralComparisons::getStructuralEqualityComparerProperty() {
    static detail::StructuralEqualityComparerImpl instance;
    return instance;
}

} // namespace System::Collections
