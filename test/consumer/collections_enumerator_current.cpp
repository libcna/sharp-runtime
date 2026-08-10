// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Positive consumer fixture for ticket #1793
// (REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT), design record
// docs/IEnumeratorCurrentSafetyDesign.md.
//
// Compiled against the PUBLIC Collections.Core surface only, with
// -Wall -Wextra -Wpedantic -Werror, and run. It proves through the shipped
// headers -- not through a test-only seam -- that:
//
//   * System::Collections::IEnumerator::getCurrentProperty() returns an owning
//     std::any by value, on both the non-generic interface and the generic
//     bridge;
//   * the box survives MoveNext, Reset, collection mutation, the enumerator's
//     destruction, and the collection's destruction;
//   * writing to the box leaves the collection completely unchanged, including
//     for a collection whose declared contract is read-only;
//   * a wrong std::any_cast is diagnosed instead of silently reinterpreting
//     bytes;
//   * the typed Generic::IEnumerator<T>::Current() path is unchanged at
//     const T&;
//   * a move-only element type keeps the typed path and loses only the
//     type-erased one, with System::NotSupportedException.
//
// Before #1793 the accessor returned a mutable void* aliasing live collection
// storage; every check below was either false or inexpressible.
#include <any>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "System/InvalidOperationException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/Stack.hpp"

namespace Coll = System::Collections;
namespace G = System::Collections::Generic;
namespace OM = System::Collections::ObjectModel;

// The public contract, asserted at compile time from a consumer translation unit.
static_assert(
    std::is_same_v<decltype(std::declval<const Coll::IEnumerator&>().getCurrentProperty()),
                   std::any>,
    "IEnumerator::getCurrentProperty() must return std::any BY VALUE");
static_assert(
    std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().getCurrentProperty()),
                   std::any>,
    "the generic bridge must return std::any BY VALUE");
static_assert(
    std::is_same_v<decltype(std::declval<const G::IEnumerator<int>&>().Current()), const int&>,
    "the typed accessor must remain const T&");

namespace {

/** A move-only element type -- the C++ analogue of .NET's `ref struct`. */
struct MoveOnly {
    std::unique_ptr<int> owned;

    explicit MoveOnly(int v) : owned(std::make_unique<int>(v)) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    ~MoveOnly() = default;
};

/** A hand-written implementer, exactly as downstream consumer code writes them. */
class MoveOnlyEnumerator : public G::IEnumerator<MoveOnly> {
    const std::vector<MoveOnly>& data_;
    int index_ = -1;

public:
    explicit MoveOnlyEnumerator(const std::vector<MoveOnly>& d) : data_(d) {}

    bool MoveNext() override { return ++index_ < static_cast<int>(data_.size()); }
    void Reset() override { index_ = -1; }

    [[nodiscard]] const MoveOnly& Current() const override {
        if (index_ < 0 || index_ >= static_cast<int>(data_.size()))
            throw System::InvalidOperationException("Enumeration has not started. Call MoveNext.");
        return data_[static_cast<std::size_t>(index_)];
    }
};

/** Reads a value type through the non-generic interface and unboxes it. */
bool readsValueTypeThroughTheNonGenericInterface() {
    G::List<int> list;
    list.Add(10);
    list.Add(20);

    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    int sum = 0;
    while (walk->MoveNext()) {
        const std::any boxed = walk->getCurrentProperty();
        if (boxed.type() != typeid(int)) return false;
        sum += std::any_cast<int>(boxed);
    }
    return sum == 30;
}

/** The same for a non-trivial element type. */
bool readsStringThroughTheNonGenericInterface() {
    G::List<std::string> list;
    list.Add("alpha");

    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    if (!walk->MoveNext()) return false;
    return std::any_cast<std::string>(walk->getCurrentProperty()) == "alpha";
}

/** The non-generic Stack, whose element type IS a void*. */
bool readsTheVoidPointerElementCase() {
    int a = 1;
    int b = 2;
    Coll::Stack stack;
    stack.Push(&a);
    stack.Push(&b);

    std::unique_ptr<Coll::IEnumerator> walk(stack.GetEnumerator());
    if (!walk->MoveNext()) return false;
    return std::any_cast<void*>(walk->getCurrentProperty()) == &b;
}

/** An ArrayList element is already a box; the accessor returns a copy of it. */
bool readsTheAlreadyBoxedElementCase() {
    Coll::ArrayList list;
    list.Add(std::any(7));

    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    if (!walk->MoveNext()) return false;

    const std::any boxed = walk->getCurrentProperty();
    if (boxed.type() == typeid(std::any)) return false;   // not nested
    return std::any_cast<int>(boxed) == 7;
}

/** The typed path is unchanged, and still hands back a reference. */
bool typedCurrentIsStillAConstReference() {
    G::List<int> list;
    list.Add(5);

    std::unique_ptr<G::IEnumerator<int>> walk(list.GetEnumerator());
    if (!walk->MoveNext()) return false;

    const int& element = walk->Current();
    return element == 5;
}

/** The box outlives MoveNext, Reset, mutation, the enumerator and the collection. */
bool boxOutlivesEverything() {
    std::any boxed;
    {
        G::List<std::string> list;
        list.Add(std::string(64, 'z'));
        {
            std::unique_ptr<G::IEnumerator<std::string>> walk(list.GetEnumerator());
            if (!walk->MoveNext()) return false;
            boxed = walk->getCurrentProperty();

            if (walk->MoveNext()) return false;            // past the end
            walk->Reset();
            if (std::any_cast<std::string>(boxed) != std::string(64, 'z')) return false;
        }                                                  // enumerator destroyed
        if (std::any_cast<std::string>(boxed) != std::string(64, 'z')) return false;

        for (int i = 0; i < 64; ++i) list.Add("filler");   // reallocation
        if (std::any_cast<std::string>(boxed) != std::string(64, 'z')) return false;

        list.Clear();
        if (std::any_cast<std::string>(boxed) != std::string(64, 'z')) return false;
    }                                                      // collection destroyed
    return std::any_cast<std::string>(boxed) == std::string(64, 'z');
}

/** Writing to the box leaves the collection untouched. */
bool writingTheBoxCannotReachTheCollection() {
    G::List<int> list;
    list.Add(1);

    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    if (!walk->MoveNext()) return false;

    std::any boxed = walk->getCurrentProperty();
    *std::any_cast<int>(&boxed) = 99;
    boxed = std::any(std::string("retyped"));

    return list.ToVector() == std::vector<int>{1};
}

/** ...including for a collection whose declared contract is read-only. */
bool readOnlyCollectionStaysReadOnly() {
    auto backing = std::make_shared<std::vector<int>>(std::vector<int>{1, 2});
    OM::ReadOnlyCollection<int> readOnly(backing);

    std::unique_ptr<Coll::IEnumerator> walk(readOnly.GetEnumerator());
    if (!walk->MoveNext()) return false;

    std::any boxed = walk->getCurrentProperty();
    *std::any_cast<int>(&boxed) = 77;

    return (*backing)[0] == 1 &&
           static_cast<const OM::ReadOnlyCollection<int>&>(readOnly)[0] == 1;
}

/** A Hashtable key can no longer be rewritten through the key view. */
bool hashtableKeysCannotBeCorrupted() {
    Coll::Hashtable table;
    table.Add(std::string("alpha"), std::any(1));

    std::unique_ptr<Coll::ICollection> keys(table.getKeysProperty());
    std::unique_ptr<Coll::IEnumerator> walk(keys->GetEnumerator());
    if (!walk->MoveNext()) return false;

    std::any boxed = walk->getCurrentProperty();
    if (std::any_cast<std::string>(boxed) != "alpha") return false;
    *std::any_cast<std::string>(&boxed) = "CORRUPTED";

    return table.getCountProperty() == 1 && table.ContainsKey("alpha") &&
           !table.ContainsKey("CORRUPTED");
}

/** A wrong cast is diagnosed rather than silently reinterpreting bytes. */
bool wrongCastIsDiagnosed() {
    G::List<int> list;
    list.Add(0x41424344);

    std::unique_ptr<Coll::IEnumerator> walk(list.GetEnumerator());
    if (!walk->MoveNext()) return false;

    const std::any boxed = walk->getCurrentProperty();
    try {
        // float is the same width as int -- silently wrong before #1793.
        (void)std::any_cast<float>(boxed);
        return false;
    } catch (const std::bad_any_cast&) {
        return std::any_cast<int>(boxed) == 0x41424344;
    }
}

/** The state machine still runs, and still runs first. */
bool stateMachineIsUnchanged() {
    G::List<int> list;
    list.Add(1);

    std::unique_ptr<G::IEnumerator<int>> walk(list.GetEnumerator());
    int rejected = 0;

    try { (void)walk->getCurrentProperty(); }
    catch (const System::InvalidOperationException&) { ++rejected; }

    if (!walk->MoveNext()) return false;
    if (walk->MoveNext()) return false;

    try { (void)walk->getCurrentProperty(); }
    catch (const System::InvalidOperationException&) { ++rejected; }

    walk->Reset();
    try { (void)walk->getCurrentProperty(); }
    catch (const System::InvalidOperationException&) { ++rejected; }

    return rejected == 3;
}

/** A move-only T keeps the typed path and loses only the type-erased one. */
bool moveOnlyKeepsTheTypedPath() {
    std::vector<MoveOnly> data;
    data.emplace_back(1);
    data.emplace_back(2);
    MoveOnlyEnumerator walk(data);

    if (!walk.MoveNext()) return false;
    if (*walk.Current().owned != 1) return false;

    bool refused = false;
    try {
        (void)walk.getCurrentProperty();
    } catch (const System::NotSupportedException&) {
        refused = true;
    }
    if (!refused) return false;

    // Everything else still works after the refusal.
    if (!walk.MoveNext()) return false;
    if (*walk.Current().owned != 2) return false;
    walk.Reset();
    return walk.MoveNext() && *walk.Current().owned == 1;
}

struct Check {
    const char* name;
    bool (*run)();
};

const Check kChecks[] = {
    {"readsValueTypeThroughTheNonGenericInterface", &readsValueTypeThroughTheNonGenericInterface},
    {"readsStringThroughTheNonGenericInterface", &readsStringThroughTheNonGenericInterface},
    {"readsTheVoidPointerElementCase", &readsTheVoidPointerElementCase},
    {"readsTheAlreadyBoxedElementCase", &readsTheAlreadyBoxedElementCase},
    {"typedCurrentIsStillAConstReference", &typedCurrentIsStillAConstReference},
    {"boxOutlivesEverything", &boxOutlivesEverything},
    {"writingTheBoxCannotReachTheCollection", &writingTheBoxCannotReachTheCollection},
    {"readOnlyCollectionStaysReadOnly", &readOnlyCollectionStaysReadOnly},
    {"hashtableKeysCannotBeCorrupted", &hashtableKeysCannotBeCorrupted},
    {"wrongCastIsDiagnosed", &wrongCastIsDiagnosed},
    {"stateMachineIsUnchanged", &stateMachineIsUnchanged},
    {"moveOnlyKeepsTheTypedPath", &moveOnlyKeepsTheTypedPath},
};

} // namespace

int main() {
    for (const auto& check : kChecks) {
        if (!check.run()) {
            std::fputs("FAIL: ", stderr);
            std::fputs(check.name, stderr);
            std::fputc('\n', stderr);
            return 1;
        }
    }
    return 0;
}
