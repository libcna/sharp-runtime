// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Positive consumer fixture for ticket #1802
// (REMED-COLL-HASHTABLE-REMOVE-VERSION).
//
// Compiled against ONLY SharpRuntime::Collections.Core (which brings Core.Base
// with it) under -Wall -Wextra -Wpedantic -Werror, through the public headers,
// with no test framework, no repository-internal include path, and NO access to
// the test-only mutation-counter seam -- a real consumer cannot read the counter,
// so every claim below is made the way a consumer would have to make it: through
// the observable behaviour of an outstanding enumerator. CNA and mobile-eggbert
// deliberately remain on older revisions and were NOT inspected, so this fixture
// is the only consumer-shaped evidence this ticket has.
//
// What it proves, through the public API alone:
//   1. Remove of an ABSENT key is a no-op: Count and contents are unchanged;
//   2. enumeration CONTINUES across an absent Remove -- every entry is still
//      visited exactly once, and Reset() still works;
//   3. Remove of an EXISTING key invalidates outstanding enumeration, which is
//      the fail-fast contract that must not regress;
//   4. a null key is rejected and mutates nothing, on both raw-key overloads;
//   5. the direct (Hashtable&) and interface (IDictionary&) paths agree;
//   6. Clear() keeps its deliberate unconditional invalidation, including on an
//      already-empty table.
#include <any>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

namespace Coll = System::Collections;

namespace {

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++gFailures;
    }
}

Coll::Hashtable seeded()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    table.Add(std::string("beta"), std::any(2));
    table.Add(std::string("gamma"), std::any(3));
    return table;
}

/** @brief 1 -- an absent Remove changes neither Count nor contents. */
void absentRemoveIsANoOp()
{
    Coll::Hashtable table = seeded();

    table.Remove(std::string("absent"));
    table.Remove("absent");
    for (int i = 0; i < 16; ++i) table.Remove(std::string("gone" + std::to_string(i)));

    check(table.getCountProperty() == 3, "an absent Remove leaves Count unchanged");
    check(table.ContainsKey("alpha") && table.ContainsKey("beta") && table.ContainsKey("gamma"),
          "an absent Remove leaves every entry in place");
    check(std::any_cast<int>(table.at("beta")) == 2,
          "an absent Remove leaves every value in place");

    Coll::Hashtable empty;
    empty.Remove(std::string("nothing"));
    check(empty.getCountProperty() == 0, "an absent Remove on an empty table is harmless");
}

/** @brief 2 -- enumeration continues, visiting every entry exactly once. */
void enumerationContinuesAcrossAnAbsentRemove()
{
    {
        Coll::Hashtable table = seeded();
        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the dictionary enumerator started");

        table.Remove(std::string("absent"));

        std::set<std::string> visited;
        bool threw = false;
        try {
            do {
                visited.insert(std::any_cast<std::string>(e->getKeyProperty()));
            } while (e->MoveNext());
        } catch (const System::InvalidOperationException&) {
            threw = true;
        }
        check(!threw, "the dictionary enumerator survived an absent Remove");
        check(visited.size() == 3, "every entry was visited exactly once after an absent Remove");
    }
    {   // the key view and the value view -- the two IEnumerator adaptations
        Coll::Hashtable table = seeded();
        std::unique_ptr<Coll::ICollection> keys(table.getKeysProperty());
        std::unique_ptr<Coll::ICollection> values(table.getValuesProperty());
        std::unique_ptr<Coll::IEnumerator> keyEnumerator(keys->GetEnumerator());
        std::unique_ptr<Coll::IEnumerator> valueEnumerator(values->GetEnumerator());
        check(keyEnumerator->MoveNext(), "the key-view enumerator started");
        check(valueEnumerator->MoveNext(), "the value-view enumerator started");

        table.Remove(std::string("absent"));

        int keyCount = 1;
        int valueCount = 1;
        bool threw = false;
        try {
            while (keyEnumerator->MoveNext()) ++keyCount;
            while (valueEnumerator->MoveNext()) ++valueCount;
        } catch (const System::InvalidOperationException&) {
            threw = true;
        }
        check(!threw, "both view enumerators survived an absent Remove");
        check(keyCount == 3 && valueCount == 3, "both views yielded all three elements");
    }
    {   // Reset() follows the same rule
        Coll::Hashtable table = seeded();
        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the enumerator started before Reset");
        table.Remove(std::string("absent"));

        bool threw = false;
        int visited = 0;
        try {
            e->Reset();
            while (e->MoveNext()) ++visited;
        } catch (const System::InvalidOperationException&) {
            threw = true;
        }
        check(!threw, "Reset() survived an absent Remove");
        check(visited == 3, "the re-walked enumeration yielded all three elements");
    }
}

/** @brief 3 -- an existing Remove still invalidates outstanding enumeration. */
void existingRemoveInvalidatesEnumeration()
{
    {
        Coll::Hashtable table = seeded();
        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the dictionary enumerator started");
        table.Remove(std::string("beta"));

        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "an effective Remove still fails the dictionary enumerator fast");
        check(table.getCountProperty() == 2, "an effective Remove removed exactly one entry");
    }
    {
        Coll::Hashtable table = seeded();
        std::unique_ptr<Coll::ICollection> keys(table.getKeysProperty());
        std::unique_ptr<Coll::IEnumerator> e(keys->GetEnumerator());
        check(e->MoveNext(), "the key-view enumerator started");
        table.Remove(std::string("beta"));

        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "an effective Remove still fails the key view fast");
    }
    {   // removing the same key twice: the second call changed nothing, so it must
        // not invalidate an enumerator taken after the first one.
        Coll::Hashtable table = seeded();
        table.Remove(std::string("beta"));
        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the enumerator started after the first Remove");
        table.Remove(std::string("beta"));

        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(!threw, "a repeated Remove of an already-removed key invalidates nothing");
    }
}

/** @brief 4 -- a rejected null key mutates nothing and invalidates nothing. */
void aRejectedNullKeyChangesNothing()
{
    Coll::Hashtable table = seeded();
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    check(e->MoveNext(), "the enumerator started before the null key");

    int rejected = 0;
    try { table.Remove(static_cast<const void*>(nullptr)); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { table.Remove(static_cast<const char*>(nullptr)); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    check(rejected == 2, "both raw-key Remove overloads reject a null key");

    // Reachable as System::Exception&, which is the catch every consumer writes.
    bool asBase = false;
    try { table.Remove(static_cast<const void*>(nullptr)); }
    catch (const System::Exception&) { asBase = true; }
    check(asBase, "the null-key rejection is catchable as System::Exception&");

    check(table.getCountProperty() == 3, "a rejected null key mutated nothing");

    bool threw = false;
    int remaining = 0;
    try { while (e->MoveNext()) ++remaining; }
    catch (const System::InvalidOperationException&) { threw = true; }
    check(!threw, "a rejected null key invalidated no enumerator");
    check(remaining == 2, "enumeration continued normally after the rejected null key");
}

/** @brief 5 -- the direct and IDictionary paths agree, on both implementations. */
void directAndInterfacePathsAgree()
{
    // Hashtable, through the interface, with the interface's raw-pointer key domain.
    {
        Coll::Hashtable table;
        int firstKey = 1;
        int secondKey = 2;
        int absentKey = 3;
        std::any firstValue = std::any(10);
        std::any secondValue = std::any(20);
        table.Add(static_cast<const void*>(&firstKey), &firstValue);
        table.Add(static_cast<const void*>(&secondKey), &secondValue);

        Coll::IDictionary& asInterface = table;
        std::unique_ptr<Coll::IDictionaryEnumerator> e(asInterface.GetEnumerator());
        check(e->MoveNext(), "the interface enumerator started");

        asInterface.Remove(static_cast<const void*>(&absentKey));
        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(!threw, "Hashtable: an absent Remove through IDictionary& invalidates nothing");
        check(asInterface.getCountProperty() == 2,
              "Hashtable: an absent Remove through IDictionary& removed nothing");

        std::unique_ptr<Coll::IDictionaryEnumerator> e2(asInterface.GetEnumerator());
        check(e2->MoveNext(), "the second interface enumerator started");
        asInterface.Remove(static_cast<const void*>(&firstKey));
        threw = false;
        try { (void)e2->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "Hashtable: a present Remove through IDictionary& fails fast");
        check(asInterface.getCountProperty() == 1,
              "Hashtable: a present Remove through IDictionary& removed exactly one entry");
    }

    // The sibling implementation must answer identically -- that is the whole point
    // of closing #1798 and #1802 together.
    {
        Coll::ListDictionaryInternal dictionary;
        int firstKey = 1;
        int secondKey = 2;
        int absentKey = 3;
        int firstValue = 10;
        int secondValue = 20;
        dictionary.Add(&firstKey, &firstValue);
        dictionary.Add(&secondKey, &secondValue);

        Coll::IDictionary& asInterface = dictionary;
        std::unique_ptr<Coll::IDictionaryEnumerator> e(asInterface.GetEnumerator());
        check(e->MoveNext(), "the sibling's interface enumerator started");

        asInterface.Remove(static_cast<const void*>(&absentKey));
        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(!threw, "ListDictionaryInternal: an absent Remove invalidates nothing");
        check(asInterface.getCountProperty() == 2,
              "ListDictionaryInternal: an absent Remove removed nothing");

        std::unique_ptr<Coll::IDictionaryEnumerator> e2(asInterface.GetEnumerator());
        check(e2->MoveNext(), "the sibling's second interface enumerator started");
        asInterface.Remove(static_cast<const void*>(&firstKey));
        threw = false;
        try { (void)e2->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "ListDictionaryInternal: a present Remove fails fast");
    }
}

/** @brief 6 -- Clear keeps its deliberate unconditional invalidation. */
void clearKeepsItsUnconditionalRule()
{
    Coll::Hashtable empty;
    std::unique_ptr<Coll::IDictionaryEnumerator> e(empty.GetEnumerator());
    empty.Clear();
    bool threw = false;
    try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
    check(threw, "Clear() on an already-empty table still invalidates -- a decided deviation");

    Coll::Hashtable table = seeded();
    std::unique_ptr<Coll::IDictionaryEnumerator> e2(table.GetEnumerator());
    check(e2->MoveNext(), "the enumerator started before Clear");
    table.Clear();
    threw = false;
    try { (void)e2->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
    check(threw, "Clear() on a non-empty table invalidates");
    check(table.getCountProperty() == 0, "Clear() emptied the table");
}

/** @brief A larger table, where a no-op Remove has a real bucket chain to walk. */
void aLargeTableBehavesTheSame()
{
    Coll::Hashtable table;
    for (int i = 0; i < 5000; ++i) table.Add("key" + std::to_string(i), std::any(i));

    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    check(e->MoveNext(), "the large-table enumerator started");
    for (int i = 0; i < 200; ++i) table.Remove("absent" + std::to_string(i));

    int visited = 1;
    bool threw = false;
    try { while (e->MoveNext()) ++visited; }
    catch (const System::InvalidOperationException&) { threw = true; }
    check(!threw, "200 no-op Removes invalidated nothing in a 5,000-entry table");
    check(visited == 5000, "the large-table walk still yielded every entry");
    check(table.getCountProperty() == 5000, "200 no-op Removes removed nothing");

    table.Remove(std::string("key4999"));
    check(table.getCountProperty() == 4999, "a present Remove in a large table removed one entry");
}

} // namespace

int main()
{
    absentRemoveIsANoOp();
    enumerationContinuesAcrossAnAbsentRemove();
    existingRemoveInvalidatesEnumeration();
    aRejectedNullKeyChangesNothing();
    directAndInterfacePathsAgree();
    clearKeepsItsUnconditionalRule();
    aLargeTableBehavesTheSame();

    if (gFailures != 0) {
        std::printf("consumer-fixture-failures=%d\n", gFailures);
        return EXIT_FAILURE;
    }
    std::printf("collections_hashtable_remove: OK\n");
    return EXIT_SUCCESS;
}
