// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1791
// (REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT), design record
// docs/ListIndexerVersioningDesign.md sections 13, 15 and 21.
//
// It writes the exact pre-#1791 consumer source that obtained a mutable alias into live
// List<T> storage, or mutated that storage behind the mutation counter, and proves the
// compiler now REJECTS that source rather than the hazard merely being discouraged in a
// doc-comment. Each of the four retained-reference shapes reproduced as an AddressSanitizer
// heap-use-after-free by #1790 §5.3 begins with one of the spellings below.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block must be REJECTED by the compiler, and
// the `#else` branch of each guard is the migrated spelling that must still compile. With no
// site selected the whole file compiles cleanly, which is what lets ticket #1801's tracked
// checker, scripts/check_negative_consumer_fixtures.py, attribute every diagnostic to the one
// enabled site; the record is docs/NegativeConsumerFixtureValidation.md.
//
// What deliberately still compiles, and is therefore NOT marked here:
//
//   * `const int& r = list[0];`  -- a read. Reads were never the defect, and the proxy
//     converts to `const T&` precisely so every read keeps its old spelling.
//   * `auto r = list[0];`        -- copying the proxy is legal, and copies the ALIAS, like
//     copying a pointer. It is documented as unsafe to retain across a mutation, exactly as
//     the old `T&` was, but it is not ill-formed.
//   * `*list.begin() = v;`       -- the explicitly unsafe STL-interop surface, kept by design
//     (§12.1) and pinned by ListIndexerVersionDivergence.StlInteropEscapesStillDoNotInvalidate.
//     It is the one remaining ordinary route to a mutable `T&` into the storage, and this
//     fixture does not pretend otherwise.
//
// Migration for a caller that hits one of these -- see design section 21:
//   was:  int& r = list[i]; r = v;          now:  list[i] = v;   or  list.setItem(i, v);
//   was:  &list[i]                          now:  &*(list.begin() + i)   // opt into unsafe
//   was:  list[i].member = v;               now:  T c = list.getItem(i); c.member = v;
//                                                 list.setItem(i, c);
//   was:  list[i].constMethod();            now:  list.getItem(i).constMethod();
//                                                 or list[i]->constMethod()
//   was:  std::swap(list[i], list[j]);      now:  T t = list.getItem(i);
//                                                 list.setItem(i, list.getItem(j));
//                                                 list.setItem(j, t);
//   was:  list.ToVector().push_back(v);     now:  list.Add(v);
//   was:  std::vector<T>& v = list.ToVector();
//                                           now:  const std::vector<T>& v = list.ToVector();
//                                                 or std::vector<T> v = list.ToArray();
//   was:  implementing IList<T> with `T& operator[](intcs)`
//                                           now:  return ElementReference<T>, and implement
//                                                 getItem/setItem
//
// NEGATIVE-FIXTURE: component=Collections.Core
#include <memory>
#include <utility>
#include <vector>

#include "System/Collections/Generic/IList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/Collections/detail/ElementReference.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

namespace Gen = System::Collections::Generic;
namespace Obj = System::Collections::ObjectModel;
namespace Det = System::Collections::detail;

/** A value-type element, so member access is the C# CS1612 case rather than a handle. */
struct Point {
    int x = 0;
    int y = 0;
    [[nodiscard]] int sum() const { return x + y; }
    bool operator==(const Point&) const = default;
};

/** Takes a mutable reference the way a pre-#1791 helper did. */
void writeThrough(int& slot);
void writeThrough(int& slot) { slot = 0; }

// An implementer that never migrated. A class return type is not a covariant return for a
// reference return, so this cannot compile lazily -- there is no candidate design under which
// an unmigrated hand-written implementer of IList<T> keeps working silently.
class UnmigratedList : public Gen::IList<int> {
    std::vector<int> data_{1, 2, 3};
    Det::MutationCounter version_;

public:
    Gen::IEnumerator<int>* GetEnumerator() override { return nullptr; }
    [[nodiscard]] SharpRuntime::intcs getCountProperty() const override {
        return static_cast<SharpRuntime::intcs>(data_.size());
    }
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }
    void Add(const int& item) override { data_.push_back(item); ++version_; }
    void Clear() override { data_.clear(); ++version_; }
    [[nodiscard]] bool Contains(const int&) const override { return false; }
    bool Remove(const int&) override { return false; }
    [[nodiscard]] const int& operator[](SharpRuntime::intcs i) const override {
        return data_[static_cast<std::size_t>(i)];
    }

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(unmigrated-indexer-override): conflicting return type specified for
    //     | invalid covariant return type
    //     | marked 'override', but does not override
    int& operator[](SharpRuntime::intcs i) override {
        return data_[static_cast<std::size_t>(i)];
    }
#else
    Det::ElementReference<int> operator[](SharpRuntime::intcs i) override {
        return {&data_[static_cast<std::size_t>(i)], &version_};
    }
#endif

    [[nodiscard]] const int& getItem(SharpRuntime::intcs i) const override {
        return data_[static_cast<std::size_t>(i)];
    }
    void setItem(SharpRuntime::intcs i, const int& value) override {
        Det::ElementReference<int>{&data_[static_cast<std::size_t>(i)], &version_} = value;
    }
    [[nodiscard]] SharpRuntime::intcs IndexOf(const int&) const override { return -1; }
    void Insert(SharpRuntime::intcs i, const int& item) override {
        data_.insert(data_.begin() + i, item);
        ++version_;
    }
    void RemoveAt(SharpRuntime::intcs i) override {
        data_.erase(data_.begin() + i);
        ++version_;
    }
};

void oldMutableAliasPath()
{
    Gen::List<int> list(std::vector<int>{10, 20, 30});

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(indexer-bind-mutable-ref): binding reference of type
    //     | discards qualifiers
    //     | cannot bind non-const lvalue reference
    //
    // The proxy converts to `const T&`, never to `T&`, so the mutable binding is rejected at
    // the const-qualification step rather than for want of any conversion at all.
    int& mutableAlias = list[0];
    mutableAlias = 99;
#else
    // One tracked expression, which is what the alias was reaching for.
    list[0] = 99;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(indexer-bind-auto-ref): cannot bind non-const lvalue reference
    //     | invalid initialization of non-const reference
    auto& deducedAlias = list[1];
    deducedAlias = 98;
#else
    list.setItem(1, 98);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(indexer-address-of): taking address of rvalue
    //     | cannot take the address of an rvalue
    int* slotAddress = &list[2];
    (void)slotAddress;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(indexer-std-addressof): no matching function for call to 'addressof'
    //     | use of deleted function
    //     | cannot bind rvalue reference
    const int* slotAddress = std::addressof(list[2]);
    (void)slotAddress;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(indexer-bind-to-parameter): invalid user-defined conversion
    //     | discards qualifiers
    //     | cannot bind non-const lvalue reference
    writeThrough(list[0]);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(indexer-swap): no matching function for call to 'swap'
    //     | cannot bind non-const lvalue reference
    std::swap(list[0], list[1]);
#else
    // Both writes tracked.
    const int held = list.getItem(0);
    list.setItem(0, list.getItem(1));
    list.setItem(1, held);
#endif
}

void oldMemberAccessPath()
{
    Gen::List<Point> points(std::vector<Point>{Point{1, 2}, Point{3, 4}});

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(indexer-member-write): has no member named 'x'
    //     | request for member 'x' in
    points[0].x = 42;
#else
    // Copy-modify-set. C# forbids the original for a value type too (CS1612).
    Point copy = points.getItem(0);
    copy.x = 42;
    points.setItem(0, copy);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(indexer-member-call): has no member named 'sum'
    //     | request for member 'sum' in
    const int total = points[1].sum();
    (void)total;
#else
    // `operator.` is not overloadable; `operator->` and getItem() both are read paths.
    const int total = points.getItem(1).sum();
    (void)total;
    const int alsoTotal = points[1]->sum();
    (void)alsoTotal;
#endif
}

void constProxyPath()
{
    Gen::List<int> list(std::vector<int>{1, 2, 3});

    // Copying the proxy is legal and copies the alias. A CONST copy, however, is a read-only
    // handle, and writing through it must be rejected.
    const auto readOnlyProxy = list[0];

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(const-proxy-write): discards qualifiers
    //     | no match for 'operator='
    //     | passing 'const System::Collections::detail::ElementReference<int>'
    readOnlyProxy = 42;
#else
    (void)readOnlyProxy.getValueProperty();
#endif
}

void oldToVectorPath()
{
    Gen::List<int> list(std::vector<int>{1, 2, 3});

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(tovector-structural-mutation): discards qualifiers
    //     | passing 'const std::vector<int>'
    //     | no matching function for call to
    list.ToVector().push_back(4);
#else
    // The tracked structural route, which advances the counter and fails an
    // in-progress enumeration fast.
    list.Add(4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 12
    // NEGATIVE(tovector-bind-mutable-ref): binding reference of type
    //     | discards qualifiers
    //     | invalid initialization of reference
    std::vector<int>& backing = list.ToVector();
    backing.clear();
#else
    // Read through a const view, or take an owning copy.
    const std::vector<int>& backing = list.ToVector();
    (void)backing.size();
    std::vector<int> owned = list.ToArray();
    owned.clear();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 13
    // NEGATIVE(tovector-mutable-data): invalid conversion from 'const int*'
    //     | cannot initialize
    int* writableData = list.ToVector().data();
    (void)writableData;
#else
    // Contiguity is preserved for reading.
    const int* readableData = list.ToVector().data();
    (void)readableData;
#endif
}

void readOnlyWrapperPath()
{
    Obj::ReadOnlyCollection<int> readOnly(std::vector<int>{1, 2, 3});

#if SHARP_RUNTIME_NEGATIVE_SITE == 14
    // NEGATIVE(readonly-mutable-alias): binding reference of type
    //     | discards qualifiers
    //     | cannot bind non-const lvalue reference
    //
    // A read-only wrapper must not be a way to reach mutable storage. The non-const indexer
    // throws NotSupportedException at run time; it must not even TYPE-CHECK as a mutable
    // alias, or a caller could hold one and write through it without ever calling the
    // throwing accessor.
    int& mutableAlias = readOnly[0];
    (void)mutableAlias;
#else
    const Obj::ReadOnlyCollection<int>& asConst = readOnly;
    const int& reading = asConst[0];
    (void)reading;
#endif
}

int main()
{
    UnmigratedList unmigrated;
    (void)unmigrated.getCountProperty();
    oldMutableAliasPath();
    oldMemberAccessPath();
    constProxyPath();
    oldToVectorPath();
    readOnlyWrapperPath();
    return 0;
}
