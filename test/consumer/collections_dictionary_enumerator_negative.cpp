// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1794
// (REMED-COLL-IDICTENUM-KEYVALUE-SAFETY), design record
// docs/IDictionaryEnumeratorKeyValueSafetyDesign.md section 28.
//
// This file must NEVER compile successfully. It is the fixture that makes the
// ticket verifiable: it writes the exact pre-#1794 consumer source that
// obtained a type-erased address into live dictionary storage and wrote through
// it, and proves the compiler now rejects that source rather than the hazard
// merely being discouraged in a doc-comment.
//
// It is deliberately excluded from every normal build target -- following the
// collections_enumerator_current_negative.cpp precedent -- and is compiled on
// purpose by this ticket's validation step, which asserts that EVERY marked site
// below produces a diagnostic.
//
// Expected diagnostics (GCC 14), one per marked line:
//   error: cannot convert 'std::any' to 'const void*' in initialization
//   error: invalid 'static_cast' from type 'std::any' to type 'const std::string*'
//   error: invalid 'static_cast' from type 'std::any' to type 'const std::any*'
//   error: invalid 'const_cast' from type 'std::any' to type 'void*'
//   error: conflicting return type specified for
//       'virtual const void* HandWrittenDictionaryEnumerator::getKeyProperty() const'
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
#include <any>
#include <memory>
#include <string>

#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

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

    // must fail: conflicting return type for the override
    [[nodiscard]] const void* getKeyProperty() const override { return &key_; }

    // must fail: conflicting return type for the override
    [[nodiscard]] const void* getValueProperty() const override { return &value_; }
};

void oldHashtableReadPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    (void)e->MoveNext();

    // must fail: no conversion from std::any to const void*
    const void* rawKey = e->getKeyProperty();
    (void)rawKey;

    // must fail: no static_cast from std::any to a pointer
    const std::string* typedKey =
        static_cast<const std::string*>(e->getKeyProperty());
    (void)typedKey;

    // must fail: no static_cast from std::any to a pointer
    const std::any* typedValue =
        static_cast<const std::any*>(e->getValueProperty());
    (void)typedValue;
}

void oldHashtableWritePath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    (void)e->MoveNext();

    // must fail: this was WELL-FORMED, FULLY DEFINED C++ before #1794 -- the
    // map's mapped_type is a non-const std::any -- and it rewrote live
    // dictionary storage with the mutation counter unmoved and a second
    // enumerator none the wiser.
    *const_cast<std::any*>(static_cast<const std::any*>(e->getValueProperty())) =
        std::any(std::string("rewritten through the enumerator"));

    // must fail: the key write, which was undefined behaviour and which at 64
    // entries left an entry Count still reported and no lookup could return.
    *const_cast<std::string*>(static_cast<const std::string*>(e->getKeyProperty())) =
        "CORRUPTED";
}

void oldListDictionaryPath()
{
    const int key = 1;
    int value = 99;
    Coll::ListDictionaryInternal dictionary;
    dictionary.Add(&key, &value);
    std::unique_ptr<Coll::IDictionaryEnumerator> e(dictionary.GetEnumerator());
    (void)e->MoveNext();

    // must fail: no conversion from std::any to const void*
    const void* rawKey = e->getKeyProperty();
    (void)rawKey;

    // must fail: no const_cast from std::any to void*
    void* rawValue = const_cast<void*>(e->getValueProperty());
    (void)rawValue;

    // must fail: no operator== between std::any and std::nullptr_t
    const bool present = (e->getKeyProperty() != nullptr);
    (void)present;
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
