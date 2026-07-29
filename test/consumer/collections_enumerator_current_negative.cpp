// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1793
// (REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT), design record
// docs/IEnumeratorCurrentSafetyDesign.md section 26.
//
// It writes the exact pre-#1793 consumer source that obtained a writable
// pointer into live collection storage through the non-generic enumerator
// interface, and proves the compiler now rejects it rather than the change
// merely being discouraged in a doc-comment.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by
// the compiler, and the `#else` branch of each guard is the migrated spelling
// that must still compile. With no site selected the whole file compiles
// cleanly, which is what lets ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, attribute every diagnostic to
// the one enabled site; the record is
// docs/NegativeConsumerFixtureValidation.md.
//
// Migration for a caller that hits one of these -- see design section 23:
//   was:  T* p = static_cast<T*>(e->getCurrentProperty());
//   now:  T  v = std::any_cast<T>(e->getCurrentProperty());
//
//   was:  *static_cast<T*>(e->getCurrentProperty()) = value;    // a WRITE
//   now:  there is no replacement, and that is the point. Use the collection's
//         own setter (setItem, operator[], Insert) so the mutation counter
//         advances and outstanding enumerators fail fast.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <any>
#include <memory>
#include <vector>

#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/IEnumerator.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

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

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(unmigrated-override): conflicting return type specified for
    //     | invalid covariant return type
    void* getCurrentProperty() const override {
        return const_cast<int*>(&data_[static_cast<std::size_t>(index_)]);
    }
#else
    [[nodiscard]] std::any getCurrentProperty() const override {
        return std::any(data_[static_cast<std::size_t>(index_)]);
    }
#endif
};

int main() {
    G::List<int> list;
    list.Add(1);
    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    (void)walk->MoveNext();

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(nongeneric-write-through): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    *static_cast<int*>(walk->getCurrentProperty()) = 5;
#else
    // The ticket's own reproduction, verbatim, in its migrated form: a read of
    // an owning snapshot, with no write path at all.
    (void)std::any_cast<int>(walk->getCurrentProperty());
#endif

    std::unique_ptr<G::IEnumerator<int>> typed(list.GetEnumerator());
    (void)typed->MoveNext();

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(typed-bridge-write-through): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    *static_cast<int*>(typed->getCurrentProperty()) = 6;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(retain-raw-pointer): cannot convert 'std::any' to 'void*'
    //     | invalid conversion from 'std::any'
    void* retained = walk->getCurrentProperty();
    (void)retained;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(compare-to-nullptr): no match for 'operator=='
    //     | no match for 'operator!='
    if (walk->getCurrentProperty() == nullptr) return 1;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(wrong-type-reinterpretation): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    const float wrong = *static_cast<float*>(walk->getCurrentProperty());
    (void)wrong;
#endif

    return 0;
}
