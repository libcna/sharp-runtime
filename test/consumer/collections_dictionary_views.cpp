// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for the non-generic IDictionary key
// and view contracts (ticket #1775, SR-AUD-363). It compiles against only the
// public Collections.Core surface, so it fails if the views start to require a
// private header, another component, or a non-public helper.
//
// It is written the way a consumer that trusts the interface documentation would
// write it: it takes an IDictionary&, asks for the documented Keys/Values views,
// and uses them without a null check. Before this ticket that shape was an
// ASan-confirmed null dereference against Hashtable.
#include <any>
#include <memory>
#include <string>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

using SharpRuntime::intcs;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IEnumerator;
using System::Collections::ListDictionaryInternal;

namespace {

int gSlots[4] = {0, 1, 2, 3};

// The interface-only consumer: never names a concrete dictionary, never
// null-checks the documented view, and owns what it is handed.
bool countsBothViewsThroughTheInterface(const IDictionary& dictionary, intcs expected) {
    std::unique_ptr<ICollection> keys(dictionary.getKeysProperty());
    std::unique_ptr<ICollection> values(dictionary.getValuesProperty());
    return keys->getCountProperty() == expected && values->getCountProperty() == expected;
}

// The views are live, so a consumer holding one sees later mutations.
bool viewsAreLive() {
    Hashtable table;
    std::unique_ptr<ICollection> keys(table.getKeysProperty());
    if (keys->getCountProperty() != 0) return false;
    table.Add(std::string("added-later"), std::any(1));
    if (keys->getCountProperty() != 1) return false;
    table.Clear();
    return keys->getCountProperty() == 0;
}

// The views reuse the ticket #1771/#1774 copy boundary unchanged.
bool viewsCopyThroughTheValidatedBoundary() {
    Hashtable table;
    table.Add(std::string("solo"), std::any(7));

    std::unique_ptr<ICollection> keys(table.getKeysProperty());
    std::unique_ptr<ICollection> values(table.getValuesProperty());

    std::vector<std::any> copiedKeys(static_cast<std::size_t>(keys->getCountProperty()));
    std::vector<std::any> copiedValues(static_cast<std::size_t>(values->getCountProperty()));
    keys->CopyTo(copiedKeys, 0);
    values->CopyTo(copiedValues, 0);

    return std::any_cast<std::string>(copiedKeys[0]) == "solo"
        && std::any_cast<int>(copiedValues[0]) == 7;
}

bool viewsEnumerate() {
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);

    std::unique_ptr<ICollection> keys(dictionary.getKeysProperty());
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    intcs visited = 0;
    while (walk->MoveNext()) {
        // Since ticket #1793 the accessor returns an owning std::any, so an
        // empty box -- not a null pointer -- is what an absent element looks like.
        if (!walk->getCurrentProperty().has_value()) return false;
        ++visited;
    }
    return visited == 2;
}

// A null key is a managed argument error on every raw entry point, not a
// silently accepted address-zero key and not a std:: exception.
bool rejectsNullKeys() {
    Hashtable table;
    std::any value = std::any(1);
    int rejected = 0;

    try { table.Add(static_cast<const void*>(nullptr), &value); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { table.setItem(nullptr, &value); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { (void)table.getItem(nullptr); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { (void)table.Contains(nullptr); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { table.Remove(static_cast<const void*>(nullptr)); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { table.Remove(static_cast<const char*>(nullptr)); }
    catch (const System::ArgumentNullException&) { ++rejected; }

    return rejected == 6 && table.getCountProperty() == 0;
}

} // namespace

int main() {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    table.Add(std::string("b"), std::any(2));

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);

    const bool ok = countsBothViewsThroughTheInterface(table, 2)
                 && countsBothViewsThroughTheInterface(dictionary, 1)
                 && viewsAreLive()
                 && viewsCopyThroughTheValidatedBoundary()
                 && viewsEnumerate()
                 && rejectsNullKeys();
    return ok ? 0 : 1;
}
