// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Positive consumer fixture for ticket #1791
// (REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT), design record
// docs/ListIndexerVersioningDesign.md section 20.
//
// Compiled against ONLY SharpRuntime::Collections.Core (which brings Core.Base with it) under
// -Wall -Wextra -Wpedantic -Werror, through the public headers, with no test framework, no
// repository-internal include path, and NO access to the test-only mutation-counter seam --
// a real consumer cannot read the counter, so every claim below is made the way a consumer
// would have to make it: through the observable behaviour of an outstanding enumerator. CNA
// and mobile-eggbert deliberately remain on older revisions and were NOT inspected, so this
// fixture is the only consumer-shaped evidence this ticket has.
//
// What it proves, through the public API alone:
//    1. an indexed READ still works through every accessor and invalidates nothing;
//    2. `list[i] = v` -- the exact spelling C# uses -- still compiles, and now invalidates;
//    3. getItem/setItem work and agree with the indexer;
//    4. an EQUAL-value replacement invalidates too, matching .NET, which never compares;
//    5. a rejected (out-of-range) write mutates nothing and invalidates nothing;
//    6. the read-only wrapper reads but refuses every write, and yields no mutable alias;
//    7. copy-modify-set is a working migration for a struct element;
//    8. ToVector() remains a usable READ path, including contiguous `.data()`;
//    9. no public API returns a mutable backing vector -- asserted at COMPILE time;
//   10. structural mutation goes through the tracked List<T> methods and fails fast.
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/detail/ElementReference.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NotSupportedException.hpp"

namespace Gen = System::Collections::Generic;
namespace Obj = System::Collections::ObjectModel;
namespace Det = System::Collections::detail;

namespace {

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++gFailures;
    }
}

/** Owns the caller-owned enumerator, exactly as a consumer must. */
template <typename T>
class ScopedEnumerator {
    Gen::IEnumerator<T>* e_;
public:
    explicit ScopedEnumerator(Gen::IList<T>& list) : e_(list.GetEnumerator()) {}
    ScopedEnumerator(const ScopedEnumerator&) = delete;
    ScopedEnumerator& operator=(const ScopedEnumerator&) = delete;
    ~ScopedEnumerator() { delete e_; }
    Gen::IEnumerator<T>* operator->() const { return e_; }
};

/** Returns true if the enumerator failed fast, as a consumer would observe it. */
template <typename T>
bool failedFast(Gen::IEnumerator<T>* e)
{
    try {
        (void)e->MoveNext();
        return false;
    } catch (const System::InvalidOperationException&) {
        return true;
    }
}

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point&) const = default;
};

// -- 1. reads --------------------------------------------------------------

void readsWorkAndInvalidateNothing()
{
    Gen::List<int> list(std::vector<int>{10, 20, 30});
    const Gen::List<int>& asConst = list;
    Gen::IList<int>& asInterface = list;

    ScopedEnumerator<int> e(list);
    check(e->MoveNext(), "the enumerator yields its first element");

    check(list[0] == 10, "read through the tracked indexer");
    check(asConst[1] == 20, "read through the const indexer");
    check(list.getItem(2) == 30, "read through getItem");
    check(asInterface.getItem(0) == 10, "read through the interface");
    const int copied = list[1];
    check(copied == 20, "conversion of the proxy to a value");
    check(list.Contains(30), "Contains");
    check(list.IndexOf(20) == 1, "IndexOf");

    check(!failedFast(e.operator->()), "no read may invalidate an outstanding enumerator");
}

// -- 2, 3, 4, 5. tracked writes --------------------------------------------

void indexedWriteInvalidates()
{
    Gen::List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    check(e->MoveNext(), "the enumerator starts");

    list[0] = 99;  // the exact spelling C# uses

    check(list.getItem(0) == 99, "the indexed write replaced the element");
    check(list.getCountProperty() == 3, "an indexed write does not resize");
    check(failedFast(e.operator->()), "an indexed write must invalidate an outstanding enumerator");
}

void setItemAgreesWithTheIndexer()
{
    Gen::List<std::string> viaIndexer(std::vector<std::string>{"a", "b"});
    Gen::List<std::string> viaSetItem(std::vector<std::string>{"a", "b"});

    viaIndexer[1] = "changed";
    viaSetItem.setItem(1, "changed");

    check(viaIndexer.getItem(1) == viaSetItem.getItem(1), "setItem and the indexer agree");
    check(viaIndexer.getItem(0) == "a", "neither disturbs the other elements");

    // Compound assignment and increment are tracked writes too.
    Gen::List<int> numbers(std::vector<int>{1, 1, 1});
    numbers[0] += 4;
    ++numbers[1];
    numbers[2]++;
    check(numbers.getItem(0) == 5, "compound assignment");
    check(numbers.getItem(1) == 2, "pre-increment");
    check(numbers.getItem(2) == 2, "post-increment");

    ScopedEnumerator<int> e(numbers);
    check(e->MoveNext(), "the enumerator starts");
    numbers[0] -= 1;
    check(failedFast(e.operator->()), "a compound assignment invalidates too");
}

void anEqualValueReplacementInvalidatesToo()
{
    // .NET never compares the old value (List.cs:161-162).
    Gen::List<int> list(std::vector<int>{10, 20, 30});
    {
        ScopedEnumerator<int> e(list);
        check(e->MoveNext(), "the enumerator starts");
        list[1] = 20;  // the same value
        check(failedFast(e.operator->()), "an equal-value indexed write invalidates");
    }
    {
        ScopedEnumerator<int> e(list);
        check(e->MoveNext(), "the enumerator starts");
        list.setItem(1, 20);  // the same value again
        check(failedFast(e.operator->()), "an equal-value setItem invalidates");
    }
    check(list.getItem(1) == 20, "the value is unchanged");
}

void aRejectedWriteChangesNothing()
{
    Gen::List<int> list(std::vector<int>{10, 20, 30});
    ScopedEnumerator<int> e(list);
    check(e->MoveNext(), "the enumerator starts");

    bool threw = false;
    try {
        list.setItem(9, 0);
    } catch (const System::ArgumentOutOfRangeException&) {
        threw = true;
    }
    check(threw, "an out-of-range setItem throws ArgumentOutOfRangeException");

    threw = false;
    try {
        (void)list[-1];
    } catch (const System::ArgumentOutOfRangeException&) {
        threw = true;
    }
    check(threw, "an out-of-range indexer throws ArgumentOutOfRangeException");

    check(list.getItem(0) == 10 && list.getItem(1) == 20 && list.getItem(2) == 30,
          "a rejected write mutates nothing");
    check(list.getCountProperty() == 3, "a rejected write does not resize");
    check(!failedFast(e.operator->()),
          "a rejected write must not invalidate: bounds are checked before the counter moves");
}

// -- 6. the read-only wrapper ----------------------------------------------

void theReadOnlyWrapperReadsButRefusesEveryWrite()
{
    Obj::ReadOnlyCollection<int> readOnly(std::vector<int>{1, 2, 3});
    const Obj::ReadOnlyCollection<int>& asConst = readOnly;
    Gen::IList<int>& asInterface = readOnly;

    check(asConst[0] == 1, "the read-only wrapper reads through the const indexer");
    check(readOnly.getItem(2) == 3, "the read-only wrapper reads through getItem");
    check(asInterface.getItem(1) == 2, "the read-only wrapper reads through the interface");
    check(readOnly.getIsReadOnlyProperty(), "it reports itself read-only");

    const char* const refusals[] = {"indexer", "setItem", "Add", "Clear"};
    bool threw[4] = {false, false, false, false};
    try { (void)readOnly[0]; }        catch (const System::NotSupportedException&) { threw[0] = true; }
    try { readOnly.setItem(0, 9); }   catch (const System::NotSupportedException&) { threw[1] = true; }
    try { readOnly.Add(4); }          catch (const System::NotSupportedException&) { threw[2] = true; }
    try { readOnly.Clear(); }         catch (const System::NotSupportedException&) { threw[3] = true; }
    for (int i = 0; i < 4; ++i)
        check(threw[i], refusals[i]);

    check(readOnly.getCountProperty() == 3, "nothing was written by any refused call");
    check(asConst[0] == 1, "and no element changed");
}

// -- 7. copy-modify-set ----------------------------------------------------

void copyModifySetIsAWorkingMigration()
{
    // `points[0].x = 42;` no longer compiles, exactly as C# rejects it for a value type
    // (CS1612). This is the replacement, and it is a tracked write.
    Gen::List<Point> points(std::vector<Point>{Point{1, 2}, Point{3, 4}});
    ScopedEnumerator<Point> e(points);
    check(e->MoveNext(), "the enumerator starts");

    Point copy = points.getItem(0);
    copy.x = 42;
    points.setItem(0, copy);

    check(points.getItem(0).x == 42, "copy-modify-set replaced the element");
    check(points.getItem(0).y == 2, "and preserved its other members");
    check(points.getItem(1).x == 3, "and left the other elements alone");
    check(failedFast(e.operator->()), "copy-modify-set is a tracked write");

    // Read-only member access still has two spellings.
    check(points.getItem(1).x == 3, "read-only member access through getItem");
    check(points[1]->y == 4, "read-only member access through the proxy's operator->");
}

// -- 8, 9. the ToVector surface --------------------------------------------

// A consumer can assert the absence of the mutable overload at COMPILE time: on a NON-const
// List<T>, ToVector() must still yield a const reference.
static_assert(std::is_same_v<decltype(std::declval<Gen::List<int>&>().ToVector()),
                             const std::vector<int>&>,
              "ticket #1791: no public API may return a mutable backing std::vector<T>&");
static_assert(std::is_same_v<decltype(std::declval<const Gen::List<int>&>().ToVector()),
                             const std::vector<int>&>,
              "the const ToVector() overload is unchanged");
static_assert(std::is_same_v<decltype(std::declval<Gen::List<int>&>()[0]),
                             Det::ElementReference<int>>,
              "ticket #1791: the non-const indexer yields the tracked proxy");
static_assert(std::is_same_v<decltype(std::declval<const Gen::List<int>&>()[0]), const int&>,
              "the const indexer is unchanged");
static_assert(std::is_same_v<decltype(std::declval<Gen::List<int>&>().ToArray()),
                             std::vector<int>>,
              "ToArray() returns an owning copy, not an alias");

void toVectorRemainsAUsableReadPath()
{
    Gen::List<int> list(std::vector<int>{1, 2, 3});
    ScopedEnumerator<int> e(list);
    check(e->MoveNext(), "the enumerator starts");

    const std::vector<int>& view = list.ToVector();
    check(view.size() == 3, "ToVector() still reads the size");
    check(view[2] == 3, "ToVector() still reads elements");

    // Contiguity is preserved, so interop that needs a const T* still works.
    const int* data = list.ToVector().data();
    check(data[0] == 1 && data[2] == 3, "ToVector().data() still yields a const T*");

    // An owning copy is the replacement when a freely mutable vector is wanted.
    std::vector<int> owned = list.ToArray();
    owned.push_back(4);
    owned[0] = 99;
    check(list.getCountProperty() == 3, "mutating the copy does not mutate the list");
    check(list.getItem(0) == 1, "and does not change its elements");

    check(!failedFast(e.operator->()), "reading the backing vector invalidates nothing");
}

// -- 10. structural mutation -----------------------------------------------

void structuralMutationGoesThroughTrackedMethods()
{
    Gen::List<int> list(std::vector<int>{1, 2, 3});
    {
        ScopedEnumerator<int> e(list);
        check(e->MoveNext(), "the enumerator starts");
        list.Add(4);
        check(failedFast(e.operator->()), "Add invalidates");
    }
    check(list.getCountProperty() == 4, "Add appended");
    {
        ScopedEnumerator<int> e(list);
        check(e->MoveNext(), "the enumerator starts");
        list.RemoveAt(0);
        check(failedFast(e.operator->()), "RemoveAt invalidates");
    }
    check(list.getCountProperty() == 3, "RemoveAt removed");
    {
        ScopedEnumerator<int> e(list);
        check(e->MoveNext(), "the enumerator starts");
        check(!list.Remove(1000), "removing an absent element reports false");
        check(!failedFast(e.operator->()), "a Remove that erased nothing must not invalidate");
    }
}

// -- Collection<T>, the second mutable implementer -------------------------

void theCollectionWrapperTracksToo()
{
    Obj::Collection<int> collection;
    collection.Add(1);
    collection.Add(2);

    check(collection.getItem(0) == 1, "Collection<T> reads through getItem");

    Gen::IEnumerator<int>* e = collection.GetEnumerator();
    check(e->MoveNext(), "the collection enumerator starts");
    collection[0] = 99;
    check(failedFast(e), "an indexed write on Collection<T> invalidates");
    delete e;

    check(collection.getItem(0) == 99, "the write landed");

    e = collection.GetEnumerator();
    check(e->MoveNext(), "a fresh enumerator starts");
    collection.setItem(1, 42);
    check(failedFast(e), "setItem on Collection<T> invalidates");
    delete e;

    check(collection.getItem(1) == 42, "the setItem write landed");
}

}  // namespace

int main()
{
    readsWorkAndInvalidateNothing();
    indexedWriteInvalidates();
    setItemAgreesWithTheIndexer();
    anEqualValueReplacementInvalidatesToo();
    aRejectedWriteChangesNothing();
    theReadOnlyWrapperReadsButRefusesEveryWrite();
    copyModifySetIsAWorkingMigration();
    toVectorRemainsAUsableReadPath();
    structuralMutationGoesThroughTrackedMethods();
    theCollectionWrapperTracksToo();

    if (gFailures != 0) {
        std::printf("collections_list_indexer: %d failure(s)\n", gFailures);
        return 1;
    }
    std::printf("collections_list_indexer: all checks passed\n");
    return 0;
}
