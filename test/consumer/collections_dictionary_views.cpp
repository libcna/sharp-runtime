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

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"

using SharpRuntime::intcs;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IDictionaryEnumerator;
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
// silently accepted address-zero key and not a std:: exception. Spelled through
// an IDictionary& so it proves the INTERFACE contract rather than one type's:
// until ticket #1798 the two implementations disagreed on all five rows, which
// meant a polymorphic consumer had no usable contract at all.
bool rejectsNullKeysThroughTheInterface(IDictionary& dictionary) {
    std::any value = std::any(1);
    void* payload = &value;
    int rejected = 0;

    try { dictionary.Add(static_cast<const void*>(nullptr), payload); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { dictionary.setItem(nullptr, payload); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { (void)dictionary.getItem(nullptr); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { (void)dictionary.Contains(nullptr); }
    catch (const System::ArgumentNullException&) { ++rejected; }
    try { dictionary.Remove(static_cast<const void*>(nullptr)); }
    catch (const System::ArgumentNullException&) { ++rejected; }

    // Nothing was stored, found or erased by any rejected call.
    return rejected == 5 && dictionary.getCountProperty() == 0;
}

bool rejectsNullKeys() {
    Hashtable table;
    ListDictionaryInternal dictionary;
    if (!rejectsNullKeysThroughTheInterface(table)) return false;
    if (!rejectsNullKeysThroughTheInterface(dictionary)) return false;

    // The C-string Remove overload is Hashtable's alone and is not on the
    // interface, so it is checked on the concrete type.
    try { table.Remove(static_cast<const char*>(nullptr)); return false; }
    catch (const System::ArgumentNullException&) { /* expected */ }
    return table.getCountProperty() == 0;
}

// Ticket #1798: replacing an existing value is an effective mutation, so an
// outstanding enumerator obtained through the interface must fail fast. Before
// it, four enumerator kinds walked to the end after a replacement with no
// diagnostic, and the value view enumerated the post-mutation value.
bool aValueReplacementInvalidatesAnOutstandingEnumerator(IDictionary& dictionary,
                                                         const void* key,
                                                         void* replacement) {
    std::unique_ptr<IDictionaryEnumerator> walk(dictionary.GetEnumerator());
    if (!walk->MoveNext()) return false;

    std::unique_ptr<ICollection> values(dictionary.getValuesProperty());
    std::unique_ptr<IEnumerator> valueWalk(values->GetEnumerator());
    if (!valueWalk->MoveNext()) return false;

    dictionary.setItem(key, replacement);

    bool dictionaryFailedFast = false;
    try { (void)walk->MoveNext(); }
    catch (const System::InvalidOperationException&) { dictionaryFailedFast = true; }

    bool valueViewFailedFast = false;
    try { (void)valueWalk->MoveNext(); }
    catch (const System::InvalidOperationException&) { valueViewFailedFast = true; }

    return dictionaryFailedFast && valueViewFailedFast;
}

bool replacementInvalidatesOnBothImplementations() {
    Hashtable table;
    std::any first = std::any(1);
    std::any second = std::any(2);
    std::any replacement = std::any(3);
    int tableKeyA = 0;
    int tableKeyB = 0;
    table.Add(static_cast<const void*>(&tableKeyA), &first);
    table.Add(static_cast<const void*>(&tableKeyB), &second);
    if (!aValueReplacementInvalidatesAnOutstandingEnumerator(
            table, static_cast<const void*>(&tableKeyA), &replacement))
        return false;

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);
    return aValueReplacementInvalidatesAnOutstandingEnumerator(dictionary, &gSlots[0],
                                                               &gSlots[2]);
}

// Ticket #1798: the key view's CopyTo boxes `const void*`, the same type its own
// Current boxes. It previously boxed a const_cast'd `void*`, so one view had two
// incompatible element types and the library published a writable pointer to an
// object the caller had declared const.
bool theKeyViewPublishesNoWritablePointer() {
    static const int readOnlyKey = 7;
    ListDictionaryInternal dictionary;
    dictionary.Add(&readOnlyKey, &gSlots[1]);

    std::unique_ptr<ICollection> keys(dictionary.getKeysProperty());
    std::vector<std::any> copied(1);
    keys->CopyTo(copied, 0);

    if (std::any_cast<const void*>(copied[0]) != &readOnlyKey) return false;

    // The superseded spelling still compiles and now throws at run time.
    try { (void)std::any_cast<void*>(copied[0]); return false; }
    catch (const std::bad_any_cast&) { /* expected */ }

    // CopyTo now agrees with the view's own enumerator.
    std::unique_ptr<IEnumerator> walk(keys->GetEnumerator());
    if (!walk->MoveNext()) return false;
    return std::any_cast<const void*>(walk->getCurrentProperty()) == &readOnlyKey;
}

// A throwing duplicate Add and a Remove of an absent key change nothing, so
// neither may invalidate an outstanding enumerator. This is the port's
// deliberate deviation from .NET ListDictionaryInternal, which bumps its version
// first and unconditionally and would invalidate on both.
bool failedAndNoOpCallsLeaveEnumeratorsValid() {
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);
    IDictionary& asInterface = dictionary;

    std::unique_ptr<IDictionaryEnumerator> walk(asInterface.GetEnumerator());
    if (!walk->MoveNext()) return false;

    try { asInterface.Add(&gSlots[0], &gSlots[1]); return false; }
    catch (const System::ArgumentException&) { /* expected: duplicate */ }

    asInterface.Remove(&gSlots[3]);   // absent key: a no-op

    try { return walk->MoveNext(); }
    catch (const System::InvalidOperationException&) { return false; }
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
                 && rejectsNullKeys()
                 && replacementInvalidatesOnBothImplementations()
                 && theKeyViewPublishesNoWritablePointer()
                 && failedAndNoOpCallsLeaveEnumeratorsValid();
    return ok ? 0 : 1;
}
