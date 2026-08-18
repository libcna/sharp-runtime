// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2339 (SR-AUD-114).
//
// #2339 made System::Attribute's constructor protected, matching .NET's
// `protected Attribute()`. It used to be public, with a doc-comment asking
// callers to "treat it as logically abstract" -- a comment asking callers to
// behave, where .NET spells the same intent in the language. C# rejects the
// equivalent with CS0144; this port now rejects it too.
//
// The copy and move members went protected with it, so the base cannot be
// reached by a slice either. Each spelling is compiled on its own below; the
// #else branches are the migrated ones, which are what every one of the
// repository's forty-six Attribute subclasses already does.
//
// Migration: derive. That is the only thing the base was ever for.
//
// Records: docs/Migration-AttributeProtectedConstructor.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <type_traits>
#include <utility>

#include "System/Attribute.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Attribute;

namespace {
/// The migrated shape: derive from the base rather than instantiating it.
class MyAttribute : public Attribute {};
}  // namespace

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(attribute-direct-instantiation): is protected within this context
    //     | protected
    //     | Attribute::Attribute
    Attribute direct;
    (void)direct;
#else
    MyAttribute direct;
    (void)direct;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(attribute-heap-instantiation): is protected within this context
    //     | protected
    //     | Attribute::Attribute
    Attribute* heap = new Attribute();
    delete heap;
#else
    Attribute* heap = new MyAttribute();
    delete heap;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(attribute-copy-slice): is protected within this context
    //     | protected
    //     | use of deleted function
    MyAttribute derived;
    Attribute sliced = derived;   // slicing the base out of a subclass
    (void)sliced;
#else
    MyAttribute derived;
    const Attribute& notSliced = derived;   // a reference does not slice
    (void)notSliced;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(attribute-still-default-constructible): static assertion failed
    //     | static_assert
    // The shape that breaks a consumer SILENTLY: a trait query, or a template constrained on one.
    static_assert(std::is_default_constructible_v<Attribute>,
                  "Attribute is expected to be default-constructible");
#else
    static_assert(!std::is_default_constructible_v<Attribute>,
                  "#2339: the constructor is protected, as .NET's is");
    static_assert(std::is_default_constructible_v<MyAttribute>,
                  "#2339: a derived type is still constructible -- that is the point");
#endif

    return 0;
}
