// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1793
// (REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT), design record
// docs/IEnumeratorCurrentSafetyDesign.md section 26.
//
// This file must NEVER compile successfully. It is the fixture that makes the
// ticket verifiable: it writes the exact pre-#1793 consumer source that
// obtained a writable pointer into live collection storage through the
// non-generic enumerator interface, and proves the compiler now rejects it
// rather than the change merely being discouraged in a doc-comment.
//
// It is deliberately excluded from every normal build target, following the
// collections_mutation_version_negative.cpp / collections_sorted_set_view_negative.cpp
// precedent, and is compiled on purpose by the ticket's validation step.
//
// Expected diagnostics (GCC 14), one per marked line:
//   error: invalid 'static_cast' from type 'std::any' to type 'int*'
//   error: invalid 'static_cast' from type 'std::any' to type 'void*'
//   error: invalid conversion from 'std::any' to 'void*'
//   error: no match for 'operator==' (operand types are 'std::any' and ...)
//   error: conflicting return type specified for 'virtual void*
//       HandWrittenEnumerator::getCurrentProperty() const'
//
// Migration for a caller that hits one of these -- see design section 23:
//   was:  T* p = static_cast<T*>(e->getCurrentProperty());
//   now:  T  v = std::any_cast<T>(e->getCurrentProperty());
//
//   was:  *static_cast<T*>(e->getCurrentProperty()) = value;    // a WRITE
//   now:  there is no replacement, and that is the point. Use the collection's
//         own setter (setItem, operator[], Insert) so the mutation counter
//         advances and outstanding enumerators fail fast.
#include <any>
#include <memory>
#include <vector>

#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/IEnumerator.hpp"

namespace Coll = System::Collections;
namespace G = System::Collections::Generic;

// A hand-written implementer that never migrated. `void*` is NOT a covariant
// return for `std::any`, so this cannot compile lazily -- there is no candidate
// design under which an unmigrated implementer keeps working.
class HandWrittenEnumerator : public Coll::IEnumerator {
    const std::vector<int>& data_;
    int index_ = -1;

public:
    explicit HandWrittenEnumerator(const std::vector<int>& d) : data_(d) {}

    bool MoveNext() override { return ++index_ < static_cast<int>(data_.size()); }
    void Reset() override { index_ = -1; }

    // must fail: conflicting return type for the override
    void* getCurrentProperty() const override {
        return const_cast<int*>(&data_[static_cast<std::size_t>(index_)]);
    }
};

int main() {
    G::List<int> list;
    list.Add(1);
    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    (void)walk->MoveNext();

    // must fail: the accessor no longer returns a pointer to cast from.
    // This is the ticket's own reproduction, verbatim.
    *static_cast<int*>(walk->getCurrentProperty()) = 5;

    // must fail: the same through the typed interface's inherited bridge.
    std::unique_ptr<G::IEnumerator<int>> typed(list.GetEnumerator());
    (void)typed->MoveNext();
    *static_cast<int*>(typed->getCurrentProperty()) = 6;

    // must fail: retaining the result as a raw pointer at all.
    void* retained = walk->getCurrentProperty();
    (void)retained;

    // must fail: comparing the result to a pointer.
    if (walk->getCurrentProperty() == nullptr) return 1;

    // must fail: the wrong-type reinterpretation the void* used to permit.
    const float wrong = *static_cast<float*>(walk->getCurrentProperty());
    (void)wrong;

    return 0;
}
