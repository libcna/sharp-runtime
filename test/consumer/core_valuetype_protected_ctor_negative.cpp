// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2322 (SR-AUD-068).
//
// #2322 made System::ValueType's constructor protected. .NET declares
// `public abstract class ValueType`, so C# rejects the equivalent of
// `System::ValueType v;`; this port compiled it.
//
// It is protected rather than abstract on purpose: a C++ class becomes
// abstract only by having a pure virtual, and .NET's ValueType.ToString() has a
// real body, so making one pure here would invent surface the reference does
// not have. The protected constructor gets the property that matters -- the
// type can still be a base, and can no longer be an object -- and the copy and
// move members went protected with it, so the base cannot be sliced out either.
//
// Each spelling is compiled on its own below; the #else branches are the
// migrated ones. Migration: derive.
//
// Records: docs/Migration-ValueTypeProtectedConstructor.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>

#include "System/ValueType.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::ValueType;

namespace {
/// The migrated shape: derive from the base rather than instantiating it.
class MyValue : public ValueType {};
}  // namespace

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(valuetype-direct-instantiation): is protected within this context
    //     | protected
    ValueType direct;
    (void)direct;
#else
    MyValue direct;
    (void)direct;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(valuetype-heap-instantiation): is protected within this context
    //     | protected
    ValueType* heap = new ValueType();
    delete heap;
#else
    ValueType* heap = new MyValue();
    delete heap;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(valuetype-copy-slice): is protected within this context
    //     | protected
    //     | use of deleted function
    MyValue derived;
    ValueType sliced = derived;
    (void)sliced;
#else
    MyValue derived;
    const ValueType& notSliced = derived;
    (void)notSliced;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(valuetype-still-default-constructible): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait query, or a template constrained on one.
    static_assert(std::is_default_constructible_v<ValueType>,
                  "ValueType is expected to be default-constructible");
#else
    static_assert(!std::is_default_constructible_v<ValueType>,
                  "#2322: the constructor is protected");
    static_assert(std::is_default_constructible_v<MyValue>,
                  "#2322: a derived type is still constructible -- that is the point");
    static_assert(!std::is_abstract_v<ValueType>,
                  "#2322 deliberately did NOT invent a pure virtual to force abstractness");
#endif

    return 0;
}
