// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1796
// (REMED-COLL-HASHTABLE-WRITE-ESCAPES), design record
// docs/HashtableValueAccessSafetyDesign.md sections 25 and 27.
//
// It writes the exact pre-#1796 consumer source that obtained a mutable or
// aliasing handle into live Hashtable value storage, and proves the compiler now
// REJECTS that source rather than the hazard merely being discouraged in a
// doc-comment.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by
// the compiler, and the `#else` branch of each guard is the migrated spelling
// that must still compile. With no site selected the whole file compiles
// cleanly, which is what lets ticket #1801's tracked checker,
// scripts/check_negative_consumer_fixtures.py, attribute every diagnostic to
// the one enabled site; the record is
// docs/NegativeConsumerFixtureValidation.md. Until #1801 the per-site checker
// for this file lived only in the gitignored build-probe/1796_check_negative.py
// and no tracked job ran it.
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
// as item 2 of design section 32, and it is therefore NOT marked here.
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
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <any>
#include <string>

#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace Coll = System::Collections;

// An implementer that never migrated. `void*` is NOT a covariant return for
// `std::any`, so this cannot compile lazily -- there is no candidate design under
// which an unmigrated implementer of IDictionary keeps working silently.
class UnmigratedDictionary : public Coll::IDictionary {
    int value_ = 1;

public:
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(unmigrated-getitem-override): conflicting return type specified for
    //     | invalid covariant return type
    [[nodiscard]] void* getItem(const void*) const override
    {
        return const_cast<int*>(&value_);
    }
#else
    [[nodiscard]] std::any getItem(const void*) const override
    {
        return std::any(value_);
    }
#endif

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

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(indexer-alias-bind): cannot bind non-const lvalue reference of type 'std::any&'
    //     | cannot bind non-const lvalue reference
    std::any& mutableAlias = table["alpha"];
    mutableAlias = std::any(99);
#else
    // One tracked insert-or-replace, which is what the alias was reaching for.
    table["alpha"] = std::any(99);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(indexer-address-of): taking address of rvalue
    //     | cannot take the address of an rvalue
    std::any* slotAddress = &table["alpha"];
    (void)slotAddress;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(indexer-any-cast-reference): must be constructible from an rvalue
    //     | static assertion failed
    std::string& inPlace = std::any_cast<std::string&>(table["alpha"]);
    (void)inPlace;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(indexer-bind-to-parameter): cannot bind non-const lvalue reference of type 'std::any&'
    //     | cannot bind non-const lvalue reference
    writeThrough(table["alpha"]);
#endif
}

void oldProxyCopyPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    // Binding the proxy to a named variable is fine and intended: guaranteed copy
    // elision constructs it in place, so no copy constructor is needed.
    Coll::Hashtable::ValueReference proxy = table["alpha"];

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(proxy-copy): use of deleted function
    //     | is private within this context
    //
    // The proxy is non-copyable ON PURPOSE. A copyable proxy makes
    // `std::any b = table[k];` prefer std::any's converting constructor over the
    // proxy's own conversion operator, so `b` would silently hold a
    // ValueReference and the next any_cast would throw at RUN TIME.
    Coll::Hashtable::ValueReference copied = proxy;
    (void)copied;
#endif

    (void)static_cast<std::any>(proxy);
}

void oldAtAliasPath()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(at-const-cast): invalid 'const_cast' of an rvalue of type 'std::any'
    //     | invalid const_cast
    const_cast<std::any&>(table.at("alpha")) = std::any(1234);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(at-alias-bind): cannot bind non-const lvalue reference of type 'std::any&'
    //     | cannot bind non-const lvalue reference
    std::any& liveView = table.at("alpha");
    (void)liveView;
#else
    // An owning snapshot, which is all at() ever safely gave.
    const std::any snapshot = table.at("alpha");
    (void)snapshot;
#endif
}

void oldGetItemPointerPath()
{
    Coll::Hashtable table;
    int anchor = 1;
    std::any value = std::any(1);
    table.Add(static_cast<const void*>(&anchor), &value);

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(getitem-raw-pointer): cannot convert 'std::any' to 'void*'
    //     | invalid conversion from 'std::any'
    void* raw = table.getItem(&anchor);
    (void)raw;
#else
    const std::any stored = table.getItem(&anchor);
    (void)stored;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(getitem-static-cast): invalid 'static_cast' from type 'std::any'
    //     | invalid static_cast from type 'std::any'
    std::any* typed = static_cast<std::any*>(table.getItem(&anchor));
    (void)typed;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(getitem-compare-nullptr): no match for 'operator!='
    //     | no match for 'operator=='
    const bool present = (table.getItem(&anchor) != nullptr);
    (void)present;
#endif
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
