// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1796
// (REMED-COLL-HASHTABLE-WRITE-ESCAPES), design record
// docs/HashtableValueAccessSafetyDesign.md sections 25 and 27.
//
// This file must NEVER compile successfully. It is the fixture that makes the
// ticket verifiable: it writes the exact pre-#1796 consumer source that obtained
// a mutable or aliasing handle into live Hashtable value storage, and proves the
// compiler now REJECTS that source rather than the hazard merely being
// discouraged in a doc-comment.
//
// It is deliberately excluded from every normal build target -- following the
// collections_enumerator_current_negative.cpp and
// collections_dictionary_enumerator_negative.cpp precedent -- and is compiled on
// purpose by this ticket's validation step, which asserts that EVERY marked site
// below produces a diagnostic.
//
// Expected diagnostics (GCC 14), one per marked line:
//   error: cannot bind non-const lvalue reference of type 'std::any&' to an
//          rvalue of type 'std::any'                       (x3: operator[], at, param)
//   error: taking address of rvalue
//   error: static assertion failed: Template argument must be constructible
//          from an rvalue                                  (any_cast<T&> on a prvalue)
//   error: invalid 'const_cast' of an rvalue of type 'std::any' to type 'std::any&'
//   error: invalid 'static_cast' from type 'std::any' to type 'std::any*'
//   error: cannot convert 'std::any' to 'void*' in initialization
//   error: use of deleted function
//          'System::Collections::Hashtable::ValueReference::ValueReference(
//              const System::Collections::Hashtable::ValueReference&)'
//   error: conflicting return type specified for
//          'virtual void* UnmigratedDictionary::getItem(const void*) const'
//
// Note what is NOT available as an escape hatch. The by-value returns are
// PRVALUES, so `const_cast` cannot recover an lvalue from them and there is no
// "just add a cast" migration for a caller that wants to write through a read.
// That is deliberate: the write path is meant to become inexpressible, not
// awkward.
//
// The ONE spelling that deliberately still compiles is
// `const std::any& r = table[key];`. It binds a lifetime-extended TEMPORARY, so
// it is memory-safe, but its meaning changed silently from a live view to a
// snapshot. That is the single silent semantic change in this ticket, approved
// as item 2 of design section 32, and it is therefore NOT listed here.
//
// Migration for a caller that hits one of these -- see design section 20:
//   was:  std::any& r = table[key]; r = value;
//   now:  table[key] = value;                 // one tracked insert-or-replace
//
//   was:  void* raw = table.getItem(key);
//         std::any stored = *static_cast<std::any*>(raw);
//   now:  std::any stored = table.getItem(key);
//
//   was:  const std::any& r = table.at(key);  // a live view
//   now:  std::any r = table.at(key);         // an owning snapshot
//
//   was:  const_cast<std::any&>(table.at(key)) = value;
//   now:  there is no replacement, and that is the point. Use the dictionary's
//         own mutating API (the indexer setter, setItem, Add) so that the
//         mutation counter advances and outstanding enumerators fail fast.
#include <any>
#include <string>

#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"

namespace Coll = System::Collections;

// An implementer that never migrated. `void*` is NOT a covariant return for
// `std::any`, so this cannot compile lazily -- there is no candidate design under
// which an unmigrated implementer of IDictionary keeps working silently.
class UnmigratedDictionary : public Coll::IDictionary {
    int value_ = 1;

public:
    // must fail: conflicting return type specified for the override
    [[nodiscard]] void* getItem(const void*) const override
    {
        return const_cast<int*>(&value_);
    }

    void setItem(const void*, void*) override {}
    [[nodiscard]] Coll::ICollection* getKeysProperty() const override { return nullptr; }
    [[nodiscard]] Coll::ICollection* getValuesProperty() const override { return nullptr; }
    [[nodiscard]] bool Contains(const void*) const override { return false; }
    void Add(const void*, void*) override {}
    void Clear() override {}
    void Remove(const void*) override {}
    [[nodiscard]] Coll::intcs getCountProperty() const override { return 0; }
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }
    [[nodiscard]] const void* getSyncRootProperty() const override { return this; }
    [[nodiscard]] Coll::IDictionaryEnumerator* GetEnumerator() override { return nullptr; }

protected:
    void copyToCore(Coll::ObjectSpan, Coll::intcs) override {}
};

// Takes a mutable reference the way a pre-#1796 helper did.
void writeThrough(std::any& slot);
void writeThrough(std::any& slot) { slot = std::any(0); }

void oldIndexerAliasPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    // must fail: cannot bind std::any& to the proxy's by-value read
    std::any& mutableAlias = table["alpha"];
    mutableAlias = std::any(99);

    // must fail: taking the address of an rvalue
    std::any* slotAddress = &table["alpha"];
    (void)slotAddress;

    // must fail: any_cast to a reference requires an lvalue operand
    std::string& inPlace = std::any_cast<std::string&>(table["alpha"]);
    (void)inPlace;

    // must fail: the proxy's read is a prvalue, so it binds no std::any& parameter
    writeThrough(table["alpha"]);
}

void oldProxyCopyPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    // Binding the proxy to a named variable is fine and intended: guaranteed copy
    // elision constructs it in place, so no copy constructor is needed.
    Coll::Hashtable::ValueReference proxy = table["alpha"];

    // must fail: the proxy is non-copyable ON PURPOSE. A copyable proxy makes
    // `std::any b = table[k];` prefer std::any's converting constructor over the
    // proxy's own conversion operator, so `b` would silently hold a
    // ValueReference and the next any_cast would throw at RUN TIME.
    Coll::Hashtable::ValueReference copied = proxy;
    (void)copied;
}

void oldAtAliasPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    // must fail: at() yields a prvalue, so const_cast cannot recover an lvalue
    const_cast<std::any&>(table.at("alpha")) = std::any(1234);

    // must fail: cannot bind a non-const lvalue reference to at()'s prvalue
    std::any& liveView = table.at("alpha");
    (void)liveView;
}

void oldGetItemPointerPath()
{
    Coll::Hashtable table;
    int anchor = 1;
    std::any value = std::any(1);
    table.Add(static_cast<const void*>(&anchor), &value);

    // must fail: getItem no longer returns a pointer at all
    void* raw = table.getItem(&anchor);
    (void)raw;

    // must fail: no static_cast from std::any to std::any*
    std::any* typed = static_cast<std::any*>(table.getItem(&anchor));
    (void)typed;

    // must fail: no comparison between std::any and std::nullptr_t
    const bool present = (table.getItem(&anchor) != nullptr);
    (void)present;
}

int main()
{
    UnmigratedDictionary unmigrated;
    (void)unmigrated.getCountProperty();
    oldIndexerAliasPath();
    oldProxyCopyPath();
    oldAtAliasPath();
    oldGetItemPointerPath();
    return 0;
}
