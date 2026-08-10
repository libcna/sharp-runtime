// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1798
// (REMED-COLL-LISTDICTINTERNAL-PARITY), design record
// docs/ListDictionaryInternalSetterDesign.md.
//
// WHY IT EXISTS, given that design section 28 deliberately proposed NO negative
// fixture. That section's reasoning was about the CopyTo representation change:
// nothing there fails at compile time, because `std::any_cast<void*>` on a key
// slot keeps compiling and becomes a run-time `std::bad_any_cast`, so a fixture
// asserting a compile rejection of it "would be theatre". That reasoning is
// correct and is unchanged -- the run-time rejection is pinned in the permanent
// suite (ListDictionarySetterContractTests.cpp) and in the positive fixture
// (collections_dictionary_views.cpp), not here.
//
// This fixture pins a DIFFERENT and genuinely compile-time claim that design
// sections 13.2 and 14.1 make and that section 28 did not consider: that the
// null-key validation is STRUCTURALLY UNSKIPPABLE rather than conventional.
// The design chose the private `ValidatedKey` boundary over a `Hashtable`-style
// `toKey()` helper (alternative A) precisely because a helper "is a convention;
// a future sixth entry point that forgets the call compiles and silently
// reopens the defect". That distinction is only real if the compiler enforces
// it, and this file is where that is asserted.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by
// the compiler; with no site selected the whole file compiles cleanly, which is
// what lets ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, attribute every diagnostic to the
// one enabled site; the record is
// docs/NegativeConsumerFixtureValidation.md.
//
// THE CI GAP THIS FILE USED TO RECORD IS CLOSED. Between #1798 and #1801 the
// per-site checker for this file lived under the gitignored `build-probe/` and
// no tracked job compiled the fixture at all. #1801 replaced that checker with
// a tracked one and put it in scripts/local_ci_check.sh.
//
// Migration for a caller that hits one of these: there is none, and that is the
// point. A key is validated by passing it to a public entry point, which throws
// System::ArgumentNullException("key") for a null key before it looks at
// storage. A consumer cannot reach the locator with an unvalidated key, cannot
// manufacture a ValidatedKey out of a null pointer, and cannot bypass the
// boundary by naming any of its parts.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <any>
#include <list>

#include "System/Collections/ListDictionaryInternal.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace NG = System::Collections;

namespace {

int gKey = 1;
int gValue = 2;

// 1. A consumer cannot construct the validated-key type at all, so it cannot
//    manufacture one and hand it to the locator.
void cannotConstructAValidatedKey() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(validated-key-construct): is private within this context
    //     | 'ValidatedKey' is private
    NG::ListDictionaryInternal::ValidatedKey key(&gKey);
    (void)key;
#endif
}

// 2. Above all, it cannot manufacture one from a NULL pointer -- which is the
//    exact bypass the boundary exists to make inexpressible. Even if the
//    constructor's null check were removed, this line would still not compile.
void cannotManufactureAValidatedNullKey() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(validated-key-construct-null): is private within this context
    //     | 'ValidatedKey' is private
    NG::ListDictionaryInternal::ValidatedKey key(nullptr);
    (void)key;
#endif
}

// 3. The type cannot even be named in a declaration, so no consumer-side alias,
//    typedef or function signature can traffic in it.
#if SHARP_RUNTIME_NEGATIVE_SITE == 3
// NEGATIVE(validated-key-alias): is private within this context
//     | 'ValidatedKey' is private
using SmuggledKey = NG::ListDictionaryInternal::ValidatedKey;
#endif

// 4. The single lookup path is unreachable, so a consumer cannot search storage
//    while skipping the boundary.
void cannotCallTheLocator() {
    NG::ListDictionaryInternal dictionary;
    dictionary.Add(&gKey, &gValue);
    const NG::ListDictionaryInternal& readOnly = dictionary;
    (void)readOnly;
#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(find-node-const): is private within this context
    //     | 'findNode' is private
    (void)readOnly.findNode(NG::ListDictionaryInternal::ValidatedKey(&gKey));
#endif
}

// 5. Nor through the non-const overload.
void cannotCallTheMutatingLocator() {
    NG::ListDictionaryInternal dictionary;
    (void)dictionary.getCountProperty();
#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(find-node-mutable): is private within this context
    //     | 'findNode' is private
    (void)dictionary.findNode(NG::ListDictionaryInternal::ValidatedKey(&gKey));
#endif
}

// 6. The node type stays private too, so a consumer cannot construct storage
//    entries directly and splice them past every check.
void cannotNameTheNodeType() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(node-type-name): is private within this context
    //     | 'Node' is private
    NG::ListDictionaryInternal::Node node{&gKey, &gValue};
    (void)node;
#endif
}

} // namespace

int main() {
    cannotConstructAValidatedKey();
    cannotManufactureAValidatedNullKey();
    cannotCallTheLocator();
    cannotCallTheMutatingLocator();
    cannotNameTheNodeType();
    return 0;
}
