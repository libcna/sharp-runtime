// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1798
// (REMED-COLL-LISTDICTINTERNAL-PARITY), design record
// docs/ListDictionaryInternalSetterDesign.md.
//
// This file must NEVER compile successfully.
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
// it, and this file is where that is asserted. Without it the difference
// between the selected design and the rejected one is a comment.
//
// It is deliberately excluded from every normal build target -- following the
// collections_enumerator_current_negative.cpp,
// collections_dictionary_enumerator_negative.cpp and
// collections_hashtable_value_access_negative.cpp precedent -- and is compiled
// on purpose by this ticket's validation step, which asserts that EVERY marked
// site below produces a diagnostic. A fixture that merely fails to compile
// proves nothing: one broken line would hide the other five.
//
// KNOWN CI GAP, recorded rather than fixed here: that per-site checker lives
// under the gitignored `build-probe/` and is therefore NOT run by normal CI, so
// this committed file is not compiled by any tracked job. That is pre-existing
// inactive ticket #1801 (REMED-TOOLING-NEGATIVE-FIXTURE-CI) and applies equally
// to the three earlier negative fixtures. #1798 does not widen the gap and does
// not close it.
//
// Expected diagnostics (GCC 14), one per marked line:
//   error: 'class System::Collections::ListDictionaryInternal::ValidatedKey' is
//          private within this context                              (x3)
//   error: 'std::__cxx11::list<...>::const_iterator
//          System::Collections::ListDictionaryInternal::findNode(...) const'
//          is private within this context                           (x2)
//   error: 'struct System::Collections::ListDictionaryInternal::Node' is
//          private within this context
//
// Migration for a caller that hits one of these: there is none, and that is the
// point. A key is validated by passing it to a public entry point, which throws
// System::ArgumentNullException("key") for a null key before it looks at
// storage. A consumer cannot reach the locator with an unvalidated key, cannot
// manufacture a ValidatedKey out of a null pointer, and cannot bypass the
// boundary by naming any of its parts.
#include <any>
#include <list>

#include "System/Collections/ListDictionaryInternal.hpp"

namespace NG = System::Collections;

namespace {

int gKey = 1;
int gValue = 2;

// 1. A consumer cannot construct the validated-key type at all, so it cannot
//    manufacture one and hand it to the locator.
void cannotConstructAValidatedKey() {
    // must fail: ValidatedKey is private
    NG::ListDictionaryInternal::ValidatedKey key(&gKey);
    (void)key;
}

// 2. Above all, it cannot manufacture one from a NULL pointer -- which is the
//    exact bypass the boundary exists to make inexpressible. Even if the
//    constructor's null check were removed, this line would still not compile.
void cannotManufactureAValidatedNullKey() {
    // must fail: ValidatedKey is private
    NG::ListDictionaryInternal::ValidatedKey key(nullptr);
    (void)key;
}

// 3. The type cannot even be named in a declaration, so no consumer-side alias,
//    typedef or function signature can traffic in it.
// must fail: ValidatedKey is private
using SmuggledKey = NG::ListDictionaryInternal::ValidatedKey;

// 4. The single lookup path is unreachable, so a consumer cannot search storage
//    while skipping the boundary.
void cannotCallTheLocator() {
    NG::ListDictionaryInternal dictionary;
    dictionary.Add(&gKey, &gValue);
    const NG::ListDictionaryInternal& readOnly = dictionary;
    // must fail: findNode is private
    (void)readOnly.findNode(NG::ListDictionaryInternal::ValidatedKey(&gKey));
}

// 5. Nor through the non-const overload.
void cannotCallTheMutatingLocator() {
    NG::ListDictionaryInternal dictionary;
    // must fail: findNode is private
    (void)dictionary.findNode(NG::ListDictionaryInternal::ValidatedKey(&gKey));
}

// 6. The node type stays private too, so a consumer cannot construct storage
//    entries directly and splice them past every check.
void cannotNameTheNodeType() {
    // must fail: Node is private
    NG::ListDictionaryInternal::Node node{&gKey, &gValue};
    (void)node;
}

} // namespace

int main() { return 0; }
