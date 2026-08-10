// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1794
// (REMED-COLL-IDICTENUM-KEYVALUE-SAFETY), design record
// docs/IDictionaryEnumeratorKeyValueSafetyDesign.md section 28.
//
// It writes the exact pre-#1794 consumer source that obtained a type-erased
// address into live dictionary storage and wrote through it, and proves the
// compiler now rejects that source rather than the hazard merely being
// discouraged in a doc-comment.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by
// the compiler, and the `#else` branch of each guard is the migrated spelling
// that must still compile. With no site selected the whole file compiles
// cleanly, which is what lets ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, attribute every diagnostic to
// the one enabled site; the record is
// docs/NegativeConsumerFixtureValidation.md.
//
// Note what is NOT available as an escape hatch: `const_cast` cannot turn a
// std::any into a pointer, so there is no "just add a cast" migration. That is
// deliberate -- the write path is meant to become inexpressible, not awkward.
//
// Migration for a caller that hits one of these -- see design section 25:
//   was:  const std::string* k =
//             static_cast<const std::string*>(e->getKeyProperty());
//   now:  std::string k = std::any_cast<std::string>(e->getKeyProperty());
//
//   was:  *const_cast<std::any*>(
//             static_cast<const std::any*>(e->getValueProperty())) = newValue;
//   now:  there is no replacement, and that is the point. Use the dictionary's
//         own mutating API (setItem, Add) so the mutation counter advances and
//         outstanding enumerators fail fast.
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <any>
#include <memory>
#include <string>

#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace Coll = System::Collections;

// A hand-written implementer that never migrated. `const void*` is NOT a
// covariant return for `std::any`, so this cannot compile lazily -- there is no
// candidate design under which an unmigrated implementer keeps working.
class HandWrittenDictionaryEnumerator : public Coll::IDictionaryEnumerator {
    int index_ = -1;
    std::string key_ = "k";
    std::any value_ = std::any(1);

public:
    bool MoveNext() override { return ++index_ < 1; }
    void Reset() override { index_ = -1; }
    [[nodiscard]] std::any getCurrentProperty() const override { return value_; }
    [[nodiscard]] Coll::DictionaryEntry getEntryProperty() const override
    {
        return Coll::DictionaryEntry(key_, value_);
    }

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(unmigrated-key-override): conflicting return type specified for
    //     | invalid covariant return type
    [[nodiscard]] const void* getKeyProperty() const override { return &key_; }
#else
    [[nodiscard]] std::any getKeyProperty() const override { return std::any(key_); }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(unmigrated-value-override): conflicting return type specified for
    //     | invalid covariant return type
    [[nodiscard]] const void* getValueProperty() const override { return &value_; }
#else
    [[nodiscard]] std::any getValueProperty() const override { return value_; }
#endif
};

void oldHashtableReadPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    (void)e->MoveNext();

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(hashtable-raw-key): cannot convert 'std::any' to 'const void*'
    //     | invalid conversion from 'std::any'
    const void* rawKey = e->getKeyProperty();
    (void)rawKey;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(hashtable-key-static-cast): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    const std::string* typedKey =
        static_cast<const std::string*>(e->getKeyProperty());
    (void)typedKey;
#else
    const std::string typedKey = std::any_cast<std::string>(e->getKeyProperty());
    (void)typedKey;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(hashtable-value-static-cast): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    const std::any* typedValue =
        static_cast<const std::any*>(e->getValueProperty());
    (void)typedValue;
#endif
}

void oldHashtableWritePath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    (void)e->MoveNext();

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(hashtable-value-write-through): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    //
    // This was WELL-FORMED, FULLY DEFINED C++ before #1794 -- the map's
    // mapped_type is a non-const std::any -- and it rewrote live dictionary
    // storage with the mutation counter unmoved and a second enumerator none
    // the wiser.
    *const_cast<std::any*>(static_cast<const std::any*>(e->getValueProperty())) =
        std::any(std::string("rewritten through the enumerator"));
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(hashtable-key-write-through): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    //
    // The key write, which was undefined behaviour and which at 64 entries left
    // an entry Count still reported and no lookup could return.
    *const_cast<std::string*>(static_cast<const std::string*>(e->getKeyProperty())) =
        "CORRUPTED";
#endif
}

void oldListDictionaryPath()
{
    const int key = 1;
    int value = 99;
    Coll::ListDictionaryInternal dictionary;
    dictionary.Add(&key, &value);
    std::unique_ptr<Coll::IDictionaryEnumerator> e(dictionary.GetEnumerator());
    (void)e->MoveNext();

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(listdictionary-raw-key): cannot convert 'std::any' to 'const void*'
    //     | invalid conversion from 'std::any'
    const void* rawKey = e->getKeyProperty();
    (void)rawKey;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(listdictionary-value-const-cast): invalid 'const_cast' from type 'std::any'
    //     | invalid const_cast from type 'std::any'
    void* rawValue = const_cast<void*>(e->getValueProperty());
    (void)rawValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(listdictionary-compare-nullptr): no match for 'operator!='
    //     | no match for 'operator=='
    const bool present = (e->getKeyProperty() != nullptr);
    (void)present;
#endif
}

int main()
{
    HandWrittenDictionaryEnumerator unmigrated;
    (void)unmigrated.MoveNext();
    oldHashtableReadPath();
    oldHashtableWritePath();
    oldListDictionaryPath();
    return 0;
}
