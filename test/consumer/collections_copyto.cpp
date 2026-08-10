// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Standalone public-header consumer fixture for the non-generic ICollection copy
// boundary (ticket #1771, SR-AUD-358 / CCF-020; corrected by ticket #1774 for
// zero-length destinations). It compiles against only the public
// Collections.Core surface, so it fails if the boundary starts to require
// a private header, another component, or a non-public helper -- and it is the
// executable form of the migration guidance in docs/Migration-ICollectionCopyTo.md.
#include <any>
#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Queue.hpp"
#include "System/Collections/Stack.hpp"

using SharpRuntime::intcs;
using System::Collections::ArrayList;
using System::Collections::DictionaryEntry;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::ListDictionaryInternal;
using System::Collections::ObjectSpan;
using System::Collections::Queue;
using System::Collections::Stack;

namespace {

int gSlots[4] = {0, 1, 2, 3};

// Interface-level copy: the destination element type is std::any for every
// implementation, so an ICollection* consumer can finally allocate correctly.
bool copiesThroughTheInterface(ICollection& collection, intcs expected) {
    std::vector<std::any> destination(static_cast<std::size_t>(expected) + 1);
    collection.CopyTo(destination, 1);
    if (destination[0].has_value()) return false;
    for (std::size_t i = 1; i < destination.size(); ++i)
        if (!destination[i].has_value()) return false;
    return true;
}

// The same call through a raw ObjectSpan over caller-owned storage.
bool copiesThroughASpan() {
    ArrayList list;
    list.Add(std::any(1));
    list.Add(std::any(std::string("two")));

    std::any storage[3];
    list.CopyTo(ObjectSpan(storage, 3), 1);
    return std::any_cast<int>(storage[1]) == 1
        && std::any_cast<std::string>(storage[2]) == "two"
        && !storage[0].has_value();
}

// Each concrete collection's typed overload, reachable alongside the inherited
// ones thanks to `using ICollection::CopyTo;`.
bool typedOverloadsAreReachable() {
    Queue queue;
    queue.Enqueue(&gSlots[0]);
    queue.Enqueue(&gSlots[1]);
    std::vector<void*> queueDestination(2);
    queue.CopyTo(queueDestination, 0);

    Stack stack;
    stack.Push(&gSlots[2]);
    std::vector<void*> stackDestination(1);
    stack.CopyTo(stackDestination, 0);

    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    std::vector<DictionaryEntry> tableDestination(1);
    table.CopyTo(tableDestination, 0);

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    std::vector<DictionaryEntry> dictionaryDestination(1);
    dictionary.CopyTo(dictionaryDestination, 0);

    std::vector<std::any> boxed(2);
    queue.CopyTo(boxed, 0);   // inherited overload, not hidden

    return queueDestination[0] == &gSlots[0]
        && stackDestination[0] == &gSlots[2]
        && std::any_cast<int>(tableDestination[0].getValueProperty()) == 1
        && std::any_cast<void*>(dictionaryDestination[0].getValueProperty()) == &gSlots[1]
        && std::any_cast<void*>(boxed[0]) == &gSlots[0];
}

// getKeysProperty()/getValuesProperty() hand back an ICollection* whose concrete
// type is not nameable by a consumer; copying from it is now possible.
bool copiesDictionaryViews() {
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);

    ICollection* keys = dictionary.getKeysProperty();
    ICollection* values = dictionary.getValuesProperty();
    std::vector<std::any> copiedKeys(static_cast<std::size_t>(keys->getCountProperty()));
    std::vector<std::any> copiedValues(static_cast<std::size_t>(values->getCountProperty()));
    keys->CopyTo(copiedKeys, 0);
    values->CopyTo(copiedValues, 0);
    // Ticket #1798: the key view's CopyTo boxes `const void*`, agreeing with its
    // own Current, with the enumerator's Key, with DictionaryEntry::Key and with
    // the typed CopyTo. It previously boxed a const_cast'd `void*` -- the only key
    // surface that disagreed -- so `std::any_cast<void*>` here still COMPILES and
    // now throws std::bad_any_cast at run time.
    const bool ok = std::any_cast<const void*>(copiedKeys[0]) == &gSlots[0]
                 && std::any_cast<void*>(copiedValues[1]) == &gSlots[3];
    delete keys;
    delete values;
    return ok;
}

// Invalid destinations are diagnosed by exception, never by a native write.
// A zero-length destination -- including a null-pointer ObjectSpan -- is a
// valid *empty* destination (ticket #1774); only a non-empty source rejects
// it, and only on capacity, while a null pointer paired with a positive length
// stays a distinct, always-malformed error.
bool rejectsInvalidDestinations() {
    ArrayList list;
    list.Add(std::any(1));
    list.Add(std::any(2));
    ICollection& collection = list;

    bool zeroLengthRejected = false;
    try { collection.CopyTo(ObjectSpan(), 0); }
    catch (const System::ArgumentException&) { zeroLengthRejected = true; }

    bool malformedNullRejected = false;
    try { collection.CopyTo(ObjectSpan(nullptr, 5), 0); }
    catch (const System::ArgumentNullException&) { malformedNullRejected = true; }

    std::vector<std::any> destination(2);
    bool negativeRejected = false;
    try { collection.CopyTo(destination, -1); }
    catch (const System::ArgumentOutOfRangeException&) { negativeRejected = true; }

    bool capacityRejected = false;
    try { collection.CopyTo(destination, 1); }
    catch (const System::ArgumentException&) { capacityRejected = true; }

    const bool untouched = !destination[0].has_value() && !destination[1].has_value();

    ArrayList empty;
    bool emptyToEmptySucceeded = true;
    try {
        std::vector<std::any> emptyDestination;
        empty.CopyTo(emptyDestination, 0);
        empty.CopyTo(ObjectSpan(), 0);
    } catch (...) {
        emptyToEmptySucceeded = false;
    }

    return zeroLengthRejected && malformedNullRejected && negativeRejected
        && capacityRejected && untouched && emptyToEmptySucceeded;
}

} // namespace

int main() {
    ArrayList list;
    list.Add(std::any(1));
    Queue queue;
    queue.Enqueue(&gSlots[0]);
    Stack stack;
    stack.Push(&gSlots[1]);
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[2], &gSlots[3]);

    return copiesThroughTheInterface(list, list.getCountProperty())
        && copiesThroughTheInterface(queue, queue.getCountProperty())
        && copiesThroughTheInterface(stack, stack.getCountProperty())
        && copiesThroughTheInterface(table, table.getCountProperty())
        && copiesThroughTheInterface(dictionary, dictionary.getCountProperty())
        && copiesThroughASpan()
        && typedOverloadsAreReachable()
        && copiesDictionaryViews()
        && rejectsInvalidDestinations() ? 0 : 1;
}
