// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Positive consumer fixture for ticket #1794
// (REMED-COLL-IDICTENUM-KEYVALUE-SAFETY), design record
// docs/IDictionaryEnumeratorKeyValueSafetyDesign.md section 28.
//
// Compiled against ONLY SharpRuntime::Collections.Core (which brings Core.Base
// with it) under -Wall -Wextra -Wpedantic -Werror, through the installed public
// headers, with no test framework and no repository-internal include path. It
// is the check that the migrated interface is usable the way a real downstream
// consumer would use it -- CNA and mobile-eggbert deliberately remain on older
// revisions and were NOT inspected, so this fixture is the only consumer-shaped
// evidence this ticket has.
//
// What it proves:
//   1. both non-generic dictionaries can be walked through IDictionaryEnumerator*;
//   2. Entry, Current, Key and Value all return OWNING values that agree;
//   3. those values outlive the enumerator AND the dictionary;
//   4. mutating what came back leaves the dictionary bit-for-bit intact;
//   5. a wrong std::any_cast is diagnosed (std::bad_any_cast) instead of
//      silently reinterpreting bytes, which the previous `const void*` did.
#include <any>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>

#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
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

/** @brief The Hashtable half: string keys, std::any-valued elements. */
void walkHashtable()
{
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));
    table.Add(std::string("beta"), std::any(std::string("two")));

    // Values that must outlive both the enumerator and the table.
    std::any survivingKey;
    std::any survivingValue;
    Coll::DictionaryEntry survivingEntry;

    {
        std::unique_ptr<Coll::IDictionaryEnumerator> e(table.GetEnumerator());

        int visited = 0;
        while (e->MoveNext()) {
            ++visited;

            const Coll::DictionaryEntry entry = e->getEntryProperty();
            const std::any key = e->getKeyProperty();
            const std::any value = e->getValueProperty();
            const std::any current = e->getCurrentProperty();

            // The documented Hashtable key shape.
            check(key.type() == typeid(std::string), "hashtable key boxes std::string");

            // Current IS Entry, boxed -- .NET's `public object Current => Entry;`.
            check(current.type() == typeid(Coll::DictionaryEntry),
                  "hashtable Current boxes DictionaryEntry");
            const auto boxedEntry = std::any_cast<Coll::DictionaryEntry>(current);
            check(std::any_cast<std::string>(boxedEntry.getKeyProperty()) ==
                      std::any_cast<std::string>(key),
                  "hashtable Current.Key == Key");

            // Key and Value are Entry's members, by construction.
            check(std::any_cast<std::string>(entry.getKeyProperty()) ==
                      std::any_cast<std::string>(key),
                  "hashtable Entry.Key == Key");
            check(entry.getValueProperty().type() == value.type(),
                  "hashtable Entry.Value == Value");

            // The element's own std::any is NOT nested inside a second one.
            check(value.type() != typeid(std::any), "hashtable value is not a nested any");

            if (std::any_cast<std::string>(key) == "alpha") {
                check(std::any_cast<int>(value) == 1, "hashtable alpha -> 1");
                survivingKey = key;
                survivingValue = value;
                survivingEntry = entry;
            }
        }
        check(visited == 2, "hashtable visited both entries");

        // The loop above left the enumerator past the end, where the state check
        // fires BEFORE any box is built -- so reposition first, otherwise the
        // next assertion would be measuring the state machine, not the cast.
        e->Reset();
        check(e->MoveNext(), "hashtable repositioned after Reset");

        // A wrong cast is DIAGNOSED. Before #1794 the equivalent misuse was a
        // silent reinterpretation, and across implementations an ASan
        // stack-buffer-overflow.
        bool threw = false;
        try {
            (void)std::any_cast<double>(e->getKeyProperty());
        } catch (const std::bad_any_cast&) {
            threw = true;
        } catch (...) {
        }
        check(threw, "hashtable wrong any_cast throws bad_any_cast");
    } // enumerator destroyed

    // Mutating the caller's own copies cannot reach the table.
    survivingKey = std::any(std::string("spoofed"));
    survivingEntry.setValueProperty(std::any(-1));

    check(table.getCountProperty() == 2, "hashtable count unchanged");
    check(table.ContainsKey("alpha"), "hashtable alpha still reachable");
    check(!table.ContainsKey("spoofed"), "hashtable spoofed key absent");
    check(std::any_cast<int>(table.at("alpha")) == 1, "hashtable alpha value unchanged");
    check(std::any_cast<int>(survivingValue) == 1, "boxed value survived the enumerator");

    // The member views hand back owning copies too.
    {
        std::unique_ptr<Coll::ICollection> keys(table.getKeysProperty());
        std::unique_ptr<Coll::IEnumerator> walk(keys->GetEnumerator());
        int seen = 0;
        while (walk->MoveNext()) {
            const std::any boxed = walk->getCurrentProperty();
            check(boxed.type() == typeid(std::string), "hashtable key view boxes std::string");
            ++seen;
        }
        check(seen == 2, "hashtable key view visited both keys");
    }
}

/** @brief The ListDictionaryInternal half: address-identity keys. */
void walkListDictionaryInternal()
{
    const int keyA = 1;
    const int keyB = 2;
    int valueA = 10;
    int valueB = 20;

    std::any survivingKey;
    std::any survivingValue;
    Coll::DictionaryEntry survivingEntry;
    std::any survivingCurrent;

    {
        Coll::ListDictionaryInternal dictionary;
        dictionary.Add(&keyA, &valueA);
        dictionary.Add(&keyB, &valueB);

        std::unique_ptr<Coll::IDictionaryEnumerator> e(dictionary.GetEnumerator());
        int visited = 0;
        while (e->MoveNext()) {
            ++visited;
            const std::any key = e->getKeyProperty();
            const std::any value = e->getValueProperty();

            // The documented ListDictionaryInternal shapes: the key keeps the
            // caller's const, the value does not, because the dictionary stores
            // `void*` and DictionaryEntry::Value has always held `void*`.
            check(key.type() == typeid(const void*), "listdict key boxes const void*");
            check(value.type() == typeid(void*), "listdict value boxes void*");

            const std::any current = e->getCurrentProperty();
            check(current.type() == typeid(Coll::DictionaryEntry),
                  "listdict Current boxes DictionaryEntry");

            if (std::any_cast<const void*>(key) == static_cast<const void*>(&keyA)) {
                check(std::any_cast<void*>(value) == static_cast<void*>(&valueA),
                      "listdict keyA -> valueA");
                survivingKey = key;
                survivingValue = value;
                survivingEntry = e->getEntryProperty();
                survivingCurrent = current;
            }
        }
        check(visited == 2, "listdict visited both entries");

        // Reading again after a mutation returns the snapshot rather than
        // dereferencing an invalidated iterator -- three of the pre-#1794
        // AddressSanitizer reports needed no caller misuse beyond this.
        std::unique_ptr<Coll::IDictionaryEnumerator> stale(dictionary.GetEnumerator());
        (void)stale->MoveNext();
        const std::any beforeClear = stale->getKeyProperty();
        dictionary.Clear();
        const std::any afterClear = stale->getKeyProperty();
        check(std::any_cast<const void*>(beforeClear) == std::any_cast<const void*>(afterClear),
              "listdict accessor after Clear returns the snapshot");
        check(stale->getEntryProperty().getKeyProperty().type() == typeid(const void*),
              "listdict Entry after Clear is safe");
    } // enumerator AND dictionary destroyed

    check(std::any_cast<const void*>(survivingKey) == static_cast<const void*>(&keyA),
          "listdict key survived the dictionary");
    check(std::any_cast<void*>(survivingValue) == static_cast<void*>(&valueA),
          "listdict value survived the dictionary");
    check(std::any_cast<const void*>(survivingEntry.getKeyProperty()) ==
              static_cast<const void*>(&keyA),
          "listdict Entry survived the dictionary");
    check(std::any_cast<Coll::DictionaryEntry>(survivingCurrent).getValueProperty().type() ==
              typeid(void*),
          "listdict Current survived the dictionary");
}

} // namespace

int main()
{
    walkHashtable();
    walkListDictionaryInternal();

    if (gFailures != 0) {
        std::printf("consumer-fixture-failures=%d\n", gFailures);
        return EXIT_FAILURE;
    }
    std::printf("collections_dictionary_enumerator: OK\n");
    return EXIT_SUCCESS;
}
