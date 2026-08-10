// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Positive consumer fixture for ticket #1796
// (REMED-COLL-HASHTABLE-WRITE-ESCAPES), design record
// docs/HashtableValueAccessSafetyDesign.md section 27.
//
// Compiled against ONLY SharpRuntime::Collections.Core (which brings Core.Base
// with it) under -Wall -Wextra -Wpedantic -Werror, through the public headers,
// with no test framework and no repository-internal include path. It is the
// check that the migrated value-access surface is usable the way a real
// downstream consumer would use it -- CNA and mobile-eggbert deliberately remain
// on older revisions and were NOT inspected, so this fixture is the only
// consumer-shaped evidence this ticket has.
//
// What it proves, through the public API alone:
//   1. an existing-key read yields the value;
//   2. a missing-key read yields an empty std::any and inserts NOTHING;
//   3. proxy assignment inserts an absent key;
//   4. proxy assignment replaces a present one;
//   5. the const operator[] reads without any write path existing;
//   6. getItem() returns an owning std::any by value;
//   7. at() returns the value, and throws KeyNotFoundException when absent --
//      catchable as System::Exception&, which std::out_of_range never was;
//   8. an outstanding enumerator FAILS FAST after a setter mutation;
//   9. an outstanding enumerator stays VALID across every getter read;
//  10. a value read before Remove/Clear/assignment/destruction stays valid;
//  11. pointer-valued entries keep their two-level semantics.
#include <any>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"

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

/** @brief 1, 2, 3, 4, 5 -- the indexer's four behaviours and the const overload. */
void indexerContract()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    // 1. existing-key read
    const std::any existing = table["alpha"];
    check(existing.type() == typeid(int), "existing-key read is the value, not a proxy");
    check(std::any_cast<int>(existing) == 1, "existing-key read yields the stored value");

    // 2. missing-key read inserts nothing
    const std::any missing = table["ghost"];
    check(!missing.has_value(), "missing-key read yields an empty std::any");
    check(table.getCountProperty() == 1, "missing-key read did NOT insert");
    check(!table.ContainsKey("ghost"), "missing-key read left no ghost entry");

    // 3. proxy assignment inserts
    table["beta"] = std::any(2);
    check(table.getCountProperty() == 2, "proxy assignment inserted an absent key");
    check(std::any_cast<int>(table.at("beta")) == 2, "the inserted value is readable");

    // 4. proxy assignment replaces
    table["beta"] = std::any(20);
    check(table.getCountProperty() == 2, "proxy replacement did not grow the table");
    check(std::any_cast<int>(table.at("beta")) == 20, "proxy assignment replaced the value");

    // ... and proxy-to-proxy assignment writes the value through.
    table["alpha"] = table["beta"];
    check(std::any_cast<int>(table.at("alpha")) == 20, "proxy-to-proxy assignment writes through");

    // 5. const operator[]
    const Coll::Hashtable& constTable = table;
    check(std::any_cast<int>(constTable["beta"]) == 20, "const operator[] reads the value");
    check(!constTable["ghost"].has_value(), "const operator[] yields empty for an absent key");
    check(table.getCountProperty() == 2, "no const read inserted anything");

    // hasValueProperty is the discriminator between absent and present-but-empty.
    table.setItem(std::string("empty"), std::any{});
    check(table["empty"].hasValueProperty(), "a present key with an empty value reports present");
    check(!table["absent"].hasValueProperty(), "an absent key reports absent");
}

/** @brief 6, 7 -- getItem by value and at()'s KeyNotFoundException contract. */
void getterContract()
{
    Coll::Hashtable table;
    int anchor = 1;
    std::any value = std::any(10);
    table.Add(static_cast<const void*>(&anchor), &value);

    // 6. getItem returns an owning std::any BY VALUE.
    const std::any boxed = table.getItem(&anchor);
    check(boxed.has_value(), "getItem found the entry");
    check(boxed.type() == typeid(int), "getItem boxes the stored payload, not a nested any");
    check(std::any_cast<int>(boxed) == 10, "getItem yields the stored value");

    int otherAnchor = 2;
    check(!table.getItem(&otherAnchor).has_value(), "getItem yields empty for an absent key");
    check(table.getCountProperty() == 1, "getItem inserted nothing");

    // And through the interface, where the old signature handed a const reference a
    // writable pointer into storage.
    const Coll::IDictionary& asInterface = table;
    check(std::any_cast<int>(asInterface.getItem(&anchor)) == 10,
          "IDictionary::getItem yields the value by value");

    // 7. at() succeeds, and throws a catchable System exception when absent.
    table.Add(std::string("named"), std::any(std::string("text")));
    check(std::any_cast<std::string>(table.at("named")) == "text", "at() yields the value");

    bool caughtAsKeyNotFound = false;
    try {
        (void)table.at("missing");
    } catch (const System::Collections::Generic::KeyNotFoundException&) {
        caughtAsKeyNotFound = true;
    }
    check(caughtAsKeyNotFound, "at() throws KeyNotFoundException for an absent key");

    bool caughtAsSystemException = false;
    std::string message;
    try {
        (void)table.at("missing");
    } catch (const System::Exception& e) {
        caughtAsSystemException = true;
        message = e.getMessageProperty();
    }
    check(caughtAsSystemException,
          "at()'s exception is catchable as System::Exception& (std::out_of_range was not)");
    check(message == "The given key 'missing' was not present in the dictionary.",
          "at()'s message names the key");
    check(table.getCountProperty() == 2, "a failed at() changed nothing");
}

/** @brief 8, 9 -- fail-fast after a write, and validity across every read. */
void enumeratorContract()
{
    {
        Coll::Hashtable table;
        table.Add(std::string("alpha"), std::any(1));
        table.Add(std::string("beta"), std::any(2));

        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the enumerator started");

        // 9. every getter leaves it valid
        (void)std::any(table["alpha"]);
        (void)std::any(table["ghost"]);
        (void)table.at("alpha");
        (void)table.getItem(&table);
        const Coll::Hashtable& constTable = table;
        (void)constTable["alpha"];

        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(!threw, "no getter invalidated an outstanding enumerator");
    }
    {
        Coll::Hashtable table;
        table.Add(std::string("alpha"), std::any(1));
        table.Add(std::string("beta"), std::any(2));

        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
        check(e->MoveNext(), "the enumerator started");

        table["alpha"] = std::any(99);      // 8. a tracked replacement

        bool threw = false;
        try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "an indexer write invalidates an outstanding enumerator");
    }
    {
        // A live value view must fail fast on the same write.
        Coll::Hashtable table;
        table.Add(std::string("alpha"), std::any(1));
        table.Add(std::string("beta"), std::any(2));

        std::unique_ptr<Coll::ICollection> view(table.getValuesProperty());
        std::unique_ptr<Coll::IEnumerator> ve(view->GetEnumerator());
        check(ve->MoveNext(), "the value view started");

        table["alpha"] = std::any(99);

        bool threw = false;
        try { (void)ve->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
        check(threw, "an indexer write invalidates a live value view");
    }
}

/** @brief 10 -- a returned value is the caller's own and outlives the table. */
void ownershipContract()
{
    std::any afterRemove;
    std::any afterClear;
    std::any afterAssign;
    std::any afterDestroy;

    {
        Coll::Hashtable table;
        table.Add(std::string("alpha"), std::any(std::string("first")));

        afterRemove = table["alpha"];
        table.Remove(std::string("alpha"));

        table.setItem(std::string("alpha"), std::any(std::string("second")));
        afterClear = table.at("alpha");
        table.Clear();

        table.setItem(std::string("alpha"), std::any(std::string("third")));
        afterAssign = table["alpha"];
        Coll::Hashtable other;
        other.Add(std::string("z"), std::any(std::string("other")));
        table = other;

        Coll::Hashtable doomed;
        doomed.Add(std::string("d"), std::any(std::string("fourth")));
        afterDestroy = doomed.at("d");

        // Rehash relocates buckets, not nodes -- but the copy does not care either way.
        Coll::Hashtable big;
        big.Add(std::string("kept"), std::any(std::string("survivor")));
        const std::any kept = big["kept"];
        for (int i = 0; i < 8000; ++i) big.Add("f" + std::to_string(i), std::any(i));
        check(std::any_cast<std::string>(kept) == "survivor",
              "a read value survives 8,000 insertions and rehash");
    }

    check(std::any_cast<std::string>(afterRemove) == "first", "a read value survives Remove()");
    check(std::any_cast<std::string>(afterClear) == "second", "a read value survives Clear()");
    check(std::any_cast<std::string>(afterAssign) == "third", "a read value survives assignment");
    check(std::any_cast<std::string>(afterDestroy) == "fourth", "a read value survives destruction");
}

/** @brief 11 -- pointer-valued entries: two levels that must not be conflated. */
void pointerValueContract()
{
    Coll::Hashtable table;
    int first = 1;
    int second = 2;
    table.setItem(std::string("ptr"), std::any(&first));

    // Mutating the pointee is NOT a dictionary mutation: an enumerator stays valid.
    std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());
    check(e->MoveNext(), "the enumerator started");
    *std::any_cast<int*>(table.at("ptr")) = 100;
    check(first == 100, "mutating the pointee is visible, as it must be");
    bool threw = false;
    try { (void)e->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
    check(!threw, "mutating a pointee does not invalidate an enumerator");

    // Rebinding the RETURNED pointer cannot reach the stored one.
    std::any read = table.at("ptr");
    std::any_cast<int*&>(read) = &second;
    check(std::any_cast<int*>(table.at("ptr")) == &first,
          "rebinding the returned pointer does not replace the stored value");

    // Replacing the STORED pointer IS a dictionary mutation.
    std::unique_ptr<Coll::IDictionaryEnumerator> e2(table.GetEnumerator());
    check(e2->MoveNext(), "the second enumerator started");
    table["ptr"] = std::any(&second);
    check(std::any_cast<int*>(table.at("ptr")) == &second, "the stored pointer was replaced");
    threw = false;
    try { (void)e2->MoveNext(); } catch (const System::InvalidOperationException&) { threw = true; }
    check(threw, "replacing the stored pointer invalidates an enumerator");
}

/** @brief The sibling implementation of the same interface, migrated mechanically. */
void listDictionaryInternalGetItem()
{
    Coll::ListDictionaryInternal dictionary;
    int key = 1;
    int value = 2;
    dictionary.Add(&key, &value);

    const Coll::IDictionary& asInterface = dictionary;
    const std::any boxed = asInterface.getItem(&key);
    check(boxed.has_value(), "ListDictionaryInternal::getItem found the entry");
    check(std::any_cast<void*>(boxed) == static_cast<void*>(&value),
          "ListDictionaryInternal::getItem boxes the caller's own pointer, unchanged");
    check(!asInterface.getItem(&value).has_value(),
          "ListDictionaryInternal::getItem yields empty for an absent key");
}

} // namespace

int main()
{
    indexerContract();
    getterContract();
    enumeratorContract();
    ownershipContract();
    pointerValueContract();
    listDictionaryInternalGetItem();

    if (gFailures != 0) {
        std::printf("consumer-fixture-failures=%d\n", gFailures);
        return EXIT_FAILURE;
    }
    std::printf("collections_hashtable_value_access: OK\n");
    return EXIT_SUCCESS;
}
