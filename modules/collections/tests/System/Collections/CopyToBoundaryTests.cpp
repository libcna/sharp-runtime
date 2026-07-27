// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for the non-generic ICollection copy boundary
// (ticket #1771, audit findings SR-AUD-358 / CCF-020, design record
// docs/ICollectionCopyToDesign.md).
//
// CCF-020's root cause was per-implementation divergence: six ICollection
// implementations each cast one unchecked void* to an element type of their own
// choosing, so the suite is parameterised over *every* implementation in the
// repository rather than testing one collection. Every case that used to be
// undefined behaviour is now an assertion about a specific exception type.
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Span.hpp"
#include "System/Collections/ArrayList.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IList.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Queue.hpp"
#include "System/Collections/Stack.hpp"

using SharpRuntime::intcs;
using System::Collections::ArrayList;
using System::Collections::DictionaryEntry;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IList;
using System::Collections::ListDictionaryInternal;
using System::Collections::ObjectSpan;
using System::Collections::Queue;
using System::Collections::Stack;

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Stable addresses for the collections whose natural element type is void*.
int gSlots[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// ArgumentNullException and ArgumentOutOfRangeException both derive from
// ArgumentException, so EXPECT_THROW(..., ArgumentException) cannot tell the
// three apart. classify() names the most-derived type actually thrown.
enum class Thrown { None, Null, OutOfRange, Argument, Other };

template <typename Fn>
Thrown classify(Fn&& call) {
    try {
        call();
    } catch (const System::ArgumentNullException&) {
        return Thrown::Null;
    } catch (const System::ArgumentOutOfRangeException&) {
        return Thrown::OutOfRange;
    } catch (const System::ArgumentException&) {
        return Thrown::Argument;
    } catch (...) {
        return Thrown::Other;
    }
    return Thrown::None;
}

// A test-only ICollection implementation, standing in for the "some downstream
// type also implements this interface" case. Its natural element type (int) is
// neither std::any nor void* nor DictionaryEntry, so it also proves that a
// fourth element shape needs no interface change.
class ProbeCollection : public ICollection {
    std::vector<int> data_;

public:
    explicit ProbeCollection(std::vector<int> d) : data_(std::move(d)) {}

    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(data_.size());
    }

    System::Collections::IEnumerator* GetEnumerator() override { return nullptr; }

protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        for (std::size_t i = 0; i < data_.size(); ++i)
            destination[index + static_cast<intcs>(i)] = std::any(data_[i]);
    }
};

// Every non-generic ICollection implementation in the repository, including both
// private MemberCollection views (reached only as an ICollection*, exactly as a
// getKeysProperty()/getValuesProperty() consumer sees them) and the test-only
// implementation above.
enum class Kind {
    ArrayListKind,
    QueueKind,
    StackKind,
    HashtableKind,
    ListDictionaryKind,
    ListDictionaryKeysKind,
    ListDictionaryValuesKind,
    ProbeKind,
};

const char* kindName(Kind kind) {
    switch (kind) {
        case Kind::ArrayListKind:            return "ArrayList";
        case Kind::QueueKind:                return "Queue";
        case Kind::StackKind:                return "Stack";
        case Kind::HashtableKind:            return "Hashtable";
        case Kind::ListDictionaryKind:       return "ListDictionaryInternal";
        case Kind::ListDictionaryKeysKind:   return "ListDictionaryInternal.Keys";
        case Kind::ListDictionaryValuesKind: return "ListDictionaryInternal.Values";
        case Kind::ProbeKind:                return "ProbeCollection";
    }
    return "?";
}

// Builds a populated collection of the requested kind and hands it to `fn` as a
// plain ICollection&, keeping every owner (and, for the dictionary views, the
// heap-allocated view itself) alive for the duration of the call.
template <typename Fn>
void withCollection(Kind kind, intcs count, Fn&& fn) {
    switch (kind) {
        case Kind::ArrayListKind: {
            ArrayList c;
            for (intcs i = 0; i < count; ++i) c.Add(std::any(static_cast<int>(i)));
            fn(static_cast<ICollection&>(c));
            return;
        }
        case Kind::QueueKind: {
            Queue c;
            for (intcs i = 0; i < count; ++i) c.Enqueue(&gSlots[i]);
            fn(static_cast<ICollection&>(c));
            return;
        }
        case Kind::StackKind: {
            Stack c;
            for (intcs i = 0; i < count; ++i) c.Push(&gSlots[i]);
            fn(static_cast<ICollection&>(c));
            return;
        }
        case Kind::HashtableKind: {
            Hashtable c;
            for (intcs i = 0; i < count; ++i)
                c.Add(std::string("k") + std::to_string(i), std::any(static_cast<int>(i)));
            fn(static_cast<ICollection&>(c));
            return;
        }
        case Kind::ListDictionaryKind: {
            ListDictionaryInternal c;
            for (intcs i = 0; i < count; ++i) c.Add(&gSlots[i], &gSlots[i]);
            fn(static_cast<ICollection&>(c));
            return;
        }
        case Kind::ListDictionaryKeysKind:
        case Kind::ListDictionaryValuesKind: {
            ListDictionaryInternal owner;
            for (intcs i = 0; i < count; ++i) owner.Add(&gSlots[i], &gSlots[i]);
            std::unique_ptr<ICollection> view(kind == Kind::ListDictionaryKeysKind
                                                  ? owner.getKeysProperty()
                                                  : owner.getValuesProperty());
            fn(*view);
            return;
        }
        case Kind::ProbeKind: {
            std::vector<int> values;
            for (intcs i = 0; i < count; ++i) values.push_back(static_cast<int>(i));
            ProbeCollection c(std::move(values));
            fn(static_cast<ICollection&>(c));
            return;
        }
    }
    FAIL() << "unhandled collection kind";
}

const Kind kAllKinds[] = {
    Kind::ArrayListKind,            Kind::QueueKind,
    Kind::StackKind,                Kind::HashtableKind,
    Kind::ListDictionaryKind,       Kind::ListDictionaryKeysKind,
    Kind::ListDictionaryValuesKind, Kind::ProbeKind,
};

class CopyToBoundaryTest : public ::testing::TestWithParam<Kind> {};

INSTANTIATE_TEST_SUITE_P(AllNonGenericCollections, CopyToBoundaryTest,
                         ::testing::ValuesIn(kAllKinds),
                         [](const ::testing::TestParamInfo<Kind>& info) {
                             std::string name = kindName(info.param);
                             for (char& ch : name)
                                 if (ch == '.') ch = '_';
                             return name;
                         });

// ---------------------------------------------------------------------------
// Destination validation, uniform across every implementation
// ---------------------------------------------------------------------------

TEST_P(CopyToBoundaryTest, NullDestinationThrowsArgumentNullException) {
    withCollection(GetParam(), 3, [](ICollection& c) {
        EXPECT_EQ(classify([&] { c.CopyTo(ObjectSpan(), 0); }), Thrown::Null);
    });
}

TEST_P(CopyToBoundaryTest, NullDestinationThrowsEvenWhenCollectionIsEmpty) {
    withCollection(GetParam(), 0, [](ICollection& c) {
        ASSERT_EQ(c.getCountProperty(), 0);
        EXPECT_EQ(classify([&] { c.CopyTo(ObjectSpan(), 0); }), Thrown::Null);
    });
}

TEST_P(CopyToBoundaryTest, NegativeIndexThrowsArgumentOutOfRangeException) {
    withCollection(GetParam(), 3, [](ICollection& c) {
        std::vector<std::any> destination(8);
        EXPECT_EQ(classify([&] { c.CopyTo(destination, -1); }), Thrown::OutOfRange);
        try {
            c.CopyTo(destination, -1);
            FAIL() << "expected ArgumentOutOfRangeException";
        } catch (const System::ArgumentOutOfRangeException& ex) {
            EXPECT_NE(std::string(ex.what()).find("Non-negative number required."),
                      std::string::npos);
        }
    });
}

TEST_P(CopyToBoundaryTest, IndexPastDestinationEndThrowsArgumentException) {
    withCollection(GetParam(), 1, [](ICollection& c) {
        std::vector<std::any> destination(4);
        EXPECT_EQ(classify([&] { c.CopyTo(destination, 5); }), Thrown::Argument);
    });
}

TEST_P(CopyToBoundaryTest, InsufficientRemainingCapacityThrowsArgumentException) {
    withCollection(GetParam(), 4, [](ICollection& c) {
        std::vector<std::any> exact(4);
        EXPECT_EQ(classify([&] { c.CopyTo(exact, 1); }), Thrown::Argument);
        std::vector<std::any> tooSmall(3);
        EXPECT_EQ(classify([&] { c.CopyTo(tooSmall, 0); }), Thrown::Argument);
    });
}

TEST_P(CopyToBoundaryTest, MaximumIndexThrowsArgumentExceptionWithoutOverflow) {
    withCollection(GetParam(), 2, [](ICollection& c) {
        std::vector<std::any> destination(4);
        EXPECT_EQ(classify([&] { c.CopyTo(destination, SharpRuntime::INTCS_MAX); }),
                  Thrown::Argument);
        EXPECT_EQ(classify([&] { c.CopyTo(destination, SharpRuntime::INTCS_MAX - 1); }),
                  Thrown::Argument);
        EXPECT_EQ(classify([&] { c.CopyTo(destination, SharpRuntime::INTCS_MIN); }),
                  Thrown::OutOfRange);
        for (const std::any& slot : destination) EXPECT_FALSE(slot.has_value());
    });
}

TEST_P(CopyToBoundaryTest, ZeroLengthDestinationIsLegalOnlyForAnEmptyCollection) {
    std::any storage[1];
    withCollection(GetParam(), 0, [&](ICollection& c) {
        EXPECT_NO_THROW(c.CopyTo(ObjectSpan(storage, 0), 0));
    });
    withCollection(GetParam(), 1, [&](ICollection& c) {
        EXPECT_EQ(classify([&] { c.CopyTo(ObjectSpan(storage, 0), 0); }), Thrown::Argument);
    });
    EXPECT_FALSE(storage[0].has_value());
}

TEST_P(CopyToBoundaryTest, EmptyCollectionAtDestinationEndIsLegal) {
    withCollection(GetParam(), 0, [](ICollection& c) {
        std::vector<std::any> destination(3, std::any(7));
        EXPECT_NO_THROW(c.CopyTo(destination, 3));
        for (const std::any& slot : destination) EXPECT_EQ(std::any_cast<int>(slot), 7);
    });
}

TEST_P(CopyToBoundaryTest, NoElementIsWrittenWhenValidationFails) {
    withCollection(GetParam(), 3, [](ICollection& c) {
        std::vector<std::any> destination(3, std::any(std::string("sentinel")));
        EXPECT_ANY_THROW(c.CopyTo(destination, 1));    // one slot short
        EXPECT_ANY_THROW(c.CopyTo(destination, -1));   // negative index
        EXPECT_ANY_THROW(c.CopyTo(destination, 9));    // past the end
        for (const std::any& slot : destination)
            EXPECT_EQ(std::any_cast<std::string>(slot), "sentinel");
    });
}

// ---------------------------------------------------------------------------
// Successful copies, uniform across every implementation
// ---------------------------------------------------------------------------

TEST_P(CopyToBoundaryTest, ExactFitCopyFillsEveryDestinationSlot) {
    withCollection(GetParam(), 4, [](ICollection& c) {
        std::vector<std::any> destination(4);
        ASSERT_NO_THROW(c.CopyTo(destination, 0));
        for (const std::any& slot : destination) EXPECT_TRUE(slot.has_value());
    });
}

TEST_P(CopyToBoundaryTest, CopyAtMiddleIndexLeavesSurroundingSlotsUntouched) {
    withCollection(GetParam(), 2, [](ICollection& c) {
        std::vector<std::any> destination(5);
        ASSERT_NO_THROW(c.CopyTo(destination, 2));
        EXPECT_FALSE(destination[0].has_value());
        EXPECT_FALSE(destination[1].has_value());
        EXPECT_TRUE(destination[2].has_value());
        EXPECT_TRUE(destination[3].has_value());
        EXPECT_FALSE(destination[4].has_value());
    });
}

TEST_P(CopyToBoundaryTest, CopyReplacesExistingDestinationValues) {
    withCollection(GetParam(), 2, [](ICollection& c) {
        std::vector<std::any> destination(3, std::any(std::string("old")));
        ASSERT_NO_THROW(c.CopyTo(destination, 0));
        EXPECT_THROW(std::any_cast<std::string>(destination[0]), std::bad_any_cast);
        EXPECT_THROW(std::any_cast<std::string>(destination[1]), std::bad_any_cast);
        EXPECT_EQ(std::any_cast<std::string>(destination[2]), "old");
    });
}

TEST_P(CopyToBoundaryTest, SpanAndVectorOverloadsAgree) {
    withCollection(GetParam(), 3, [](ICollection& c) {
        std::vector<std::any> viaVector(3);
        std::any viaSpan[3];
        ASSERT_NO_THROW(c.CopyTo(viaVector, 0));
        ASSERT_NO_THROW(c.CopyTo(ObjectSpan(viaSpan, 3), 0));
        for (std::size_t i = 0; i < 3; ++i) {
            ASSERT_TRUE(viaSpan[i].has_value());
            EXPECT_EQ(viaSpan[i].type(), viaVector[i].type());
        }
    });
}

TEST_P(CopyToBoundaryTest, PolymorphicDispatchMatchesReferenceDispatch) {
    withCollection(GetParam(), 3, [](ICollection& c) {
        ICollection* pointer = &c;
        ICollection& reference = c;
        std::vector<std::any> viaPointer(3);
        std::vector<std::any> viaReference(3);
        ASSERT_NO_THROW(pointer->CopyTo(viaPointer, 0));
        ASSERT_NO_THROW(reference.CopyTo(viaReference, 0));
        for (std::size_t i = 0; i < 3; ++i)
            EXPECT_EQ(viaPointer[i].type(), viaReference[i].type());
    });
}

// ---------------------------------------------------------------------------
// Per-collection element identity, boxing, and ordering
// ---------------------------------------------------------------------------

TEST(CopyToBoundaryValues, ArrayListCopiesStoredAnyValuesUnchanged) {
    ArrayList list;
    list.Add(std::any(1));
    list.Add(std::any(std::string("two")));
    list.Add(std::any(3.5));
    list.Add(std::any(static_cast<void*>(&gSlots[0])));

    std::vector<std::any> destination(5);
    ASSERT_NO_THROW(list.CopyTo(destination, 1));
    EXPECT_FALSE(destination[0].has_value());
    EXPECT_EQ(std::any_cast<int>(destination[1]), 1);
    EXPECT_EQ(std::any_cast<std::string>(destination[2]), "two");
    EXPECT_DOUBLE_EQ(std::any_cast<double>(destination[3]), 3.5);
    EXPECT_EQ(std::any_cast<void*>(destination[4]), &gSlots[0]);
}

TEST(CopyToBoundaryValues, QueuePreservesFifoOrderThroughBothOverloads) {
    Queue queue;
    queue.Enqueue(&gSlots[0]);
    queue.Enqueue(&gSlots[1]);
    queue.Enqueue(&gSlots[2]);

    std::vector<std::any> boxed(3);
    ASSERT_NO_THROW(queue.CopyTo(boxed, 0));
    EXPECT_EQ(std::any_cast<void*>(boxed[0]), &gSlots[0]);
    EXPECT_EQ(std::any_cast<void*>(boxed[1]), &gSlots[1]);
    EXPECT_EQ(std::any_cast<void*>(boxed[2]), &gSlots[2]);

    std::vector<void*> typed(4);
    ASSERT_NO_THROW(queue.CopyTo(typed, 1));
    EXPECT_EQ(typed[0], nullptr);
    EXPECT_EQ(typed[1], &gSlots[0]);
    EXPECT_EQ(typed[3], &gSlots[2]);
}

TEST(CopyToBoundaryValues, StackPreservesTopToBottomOrderThroughBothOverloads) {
    Stack stack;
    stack.Push(&gSlots[0]);
    stack.Push(&gSlots[1]);
    stack.Push(&gSlots[2]);

    std::vector<std::any> boxed(3);
    ASSERT_NO_THROW(stack.CopyTo(boxed, 0));
    EXPECT_EQ(std::any_cast<void*>(boxed[0]), &gSlots[2]);
    EXPECT_EQ(std::any_cast<void*>(boxed[2]), &gSlots[0]);

    std::vector<void*> typed(3);
    ASSERT_NO_THROW(stack.CopyTo(typed, 0));
    EXPECT_EQ(typed[0], &gSlots[2]);
    EXPECT_EQ(typed[2], &gSlots[0]);
}

TEST(CopyToBoundaryValues, HashtableBoxesDictionaryEntriesThroughBothOverloads) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    table.Add(std::string("b"), std::any(2));

    std::vector<std::any> boxed(2);
    ASSERT_NO_THROW(table.CopyTo(boxed, 0));
    int boxedSum = 0;
    for (const std::any& slot : boxed)
        boxedSum += std::any_cast<int>(std::any_cast<DictionaryEntry>(slot).getValueProperty());
    EXPECT_EQ(boxedSum, 3);

    std::vector<DictionaryEntry> typed(3);
    ASSERT_NO_THROW(table.CopyTo(typed, 1));
    EXPECT_FALSE(typed[0].getKeyProperty().has_value());
    EXPECT_EQ(std::any_cast<int>(typed[1].getValueProperty())
                  + std::any_cast<int>(typed[2].getValueProperty()),
              3);
}

TEST(CopyToBoundaryValues, ListDictionaryInternalBoxesDictionaryEntriesThroughBothOverloads) {
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);

    std::vector<std::any> boxed(2);
    ASSERT_NO_THROW(dictionary.CopyTo(boxed, 0));
    DictionaryEntry first = std::any_cast<DictionaryEntry>(boxed[0]);
    EXPECT_EQ(std::any_cast<const void*>(first.getKeyProperty()), &gSlots[0]);
    EXPECT_EQ(std::any_cast<void*>(first.getValueProperty()), &gSlots[1]);

    std::vector<DictionaryEntry> typed(2);
    ASSERT_NO_THROW(dictionary.CopyTo(typed, 0));
    EXPECT_EQ(std::any_cast<void*>(typed[1].getValueProperty()), &gSlots[3]);
}

TEST(CopyToBoundaryValues, DictionaryViewsBoxKeysAndValuesIdentically) {
    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    dictionary.Add(&gSlots[2], &gSlots[3]);

    std::unique_ptr<ICollection> keys(dictionary.getKeysProperty());
    std::unique_ptr<ICollection> values(dictionary.getValuesProperty());

    // A consumer that only ever sees an ICollection* can now allocate correctly:
    // getCountProperty() plus a fixed element type is all it needs.
    std::vector<std::any> copiedKeys(static_cast<std::size_t>(keys->getCountProperty()));
    std::vector<std::any> copiedValues(static_cast<std::size_t>(values->getCountProperty()));
    ASSERT_NO_THROW(keys->CopyTo(copiedKeys, 0));
    ASSERT_NO_THROW(values->CopyTo(copiedValues, 0));

    EXPECT_EQ(std::any_cast<void*>(copiedKeys[0]), &gSlots[0]);
    EXPECT_EQ(std::any_cast<void*>(copiedKeys[1]), &gSlots[2]);
    EXPECT_EQ(std::any_cast<void*>(copiedValues[0]), &gSlots[1]);
    EXPECT_EQ(std::any_cast<void*>(copiedValues[1]), &gSlots[3]);
}

// ---------------------------------------------------------------------------
// Inherited interface surfaces
// ---------------------------------------------------------------------------

TEST(CopyToBoundaryInterfaces, IListSurfaceExposesTheValidatedOverloads) {
    ArrayList list;
    list.Add(std::any(11));
    list.Add(std::any(22));
    IList& asList = list;

    std::vector<std::any> destination(2);
    ASSERT_NO_THROW(asList.CopyTo(destination, 0));
    EXPECT_EQ(std::any_cast<int>(destination[0]), 11);
    EXPECT_EQ(classify([&] { asList.CopyTo(destination, 1); }), Thrown::Argument);
}

TEST(CopyToBoundaryInterfaces, IDictionarySurfaceExposesTheValidatedOverloads) {
    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    IDictionary& asDictionary = table;

    std::vector<std::any> destination(1);
    ASSERT_NO_THROW(asDictionary.CopyTo(destination, 0));
    EXPECT_TRUE(destination[0].has_value());
    EXPECT_EQ(classify([&] { asDictionary.CopyTo(destination, -1); }), Thrown::OutOfRange);

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    IDictionary& asListDictionary = dictionary;
    std::vector<std::any> tooSmall(0);
    EXPECT_ANY_THROW(asListDictionary.CopyTo(tooSmall, 0));
}

TEST(CopyToBoundaryInterfaces, BaseValidationRunsBeforeDerivedCopyToCore) {
    // The derived hook can only be reached through the validating base method,
    // so a rejected destination is provably never handed to an implementation:
    // a destination whose elements would be corrupted by a partial write stays
    // exactly as the caller left it.
    ArrayList list;
    for (int i = 0; i < 100; ++i) list.Add(std::any(i));

    std::vector<std::any> destination(99, std::any(std::string("untouched")));
    EXPECT_EQ(classify([&] { list.CopyTo(destination, 0); }), Thrown::Argument);
    for (const std::any& slot : destination)
        EXPECT_EQ(std::any_cast<std::string>(slot), "untouched");
}

// ---------------------------------------------------------------------------
// Typed concrete overloads
// ---------------------------------------------------------------------------

TEST(CopyToBoundaryTypedOverloads, TypedOverloadsRejectTheSameInvalidInputs) {
    Queue queue;
    queue.Enqueue(&gSlots[0]);
    queue.Enqueue(&gSlots[1]);

    std::vector<void*> destination(2);
    EXPECT_EQ(classify([&] { queue.CopyTo(destination, -1); }), Thrown::OutOfRange);
    EXPECT_EQ(classify([&] { queue.CopyTo(destination, 1); }), Thrown::Argument);
    EXPECT_EQ(classify([&] { queue.CopyTo(destination, 3); }), Thrown::Argument);
    EXPECT_EQ(classify([&] { queue.CopyTo(destination, SharpRuntime::INTCS_MAX); }),
              Thrown::Argument);
    std::vector<void*> empty;
    EXPECT_EQ(classify([&] { queue.CopyTo(empty, 0); }), Thrown::Null);
    for (void* slot : destination) EXPECT_EQ(slot, nullptr);

    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    std::vector<DictionaryEntry> entries(1);
    EXPECT_EQ(classify([&] { table.CopyTo(entries, -1); }), Thrown::OutOfRange);
    EXPECT_EQ(classify([&] { table.CopyTo(entries, 1); }), Thrown::Argument);
    EXPECT_FALSE(entries[0].getKeyProperty().has_value());

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    std::vector<DictionaryEntry> tooSmall(0);
    EXPECT_EQ(classify([&] { dictionary.CopyTo(tooSmall, 0); }), Thrown::Null);

    Stack stack;
    stack.Push(&gSlots[0]);
    std::vector<void*> stackDestination(1);
    EXPECT_EQ(classify([&] { stack.CopyTo(stackDestination, 1); }), Thrown::Argument);
    EXPECT_EQ(stackDestination[0], nullptr);
}

// `using ICollection::CopyTo;` regression: a derived class that declares its own
// CopyTo overload hides every inherited one unless the using-declaration is
// present. Each of these calls binds to an inherited overload on a class that
// also declares a typed one.
TEST(CopyToBoundaryTypedOverloads, InheritedOverloadsStayVisibleOnConcreteClasses) {
    std::any storage[2];

    Queue queue;
    queue.Enqueue(&gSlots[0]);
    std::vector<std::any> queueBoxed(1);
    EXPECT_NO_THROW(queue.CopyTo(queueBoxed, 0));
    EXPECT_NO_THROW(queue.CopyTo(ObjectSpan(storage, 2), 1));

    Stack stack;
    stack.Push(&gSlots[0]);
    std::vector<std::any> stackBoxed(1);
    EXPECT_NO_THROW(stack.CopyTo(stackBoxed, 0));
    EXPECT_NO_THROW(stack.CopyTo(ObjectSpan(storage, 2), 1));

    Hashtable table;
    table.Add(std::string("a"), std::any(1));
    std::vector<std::any> tableBoxed(1);
    EXPECT_NO_THROW(table.CopyTo(tableBoxed, 0));
    EXPECT_NO_THROW(table.CopyTo(ObjectSpan(storage, 2), 1));

    ListDictionaryInternal dictionary;
    dictionary.Add(&gSlots[0], &gSlots[1]);
    std::vector<std::any> dictionaryBoxed(1);
    EXPECT_NO_THROW(dictionary.CopyTo(dictionaryBoxed, 0));
    EXPECT_NO_THROW(dictionary.CopyTo(ObjectSpan(storage, 2), 1));

    ArrayList list;
    list.Add(std::any(1));
    std::vector<std::any> listBoxed(1);
    EXPECT_NO_THROW(list.CopyTo(listBoxed, 0));
    EXPECT_NO_THROW(list.CopyTo(ObjectSpan(storage, 2), 1));
}

// ---------------------------------------------------------------------------
// Compile-time migration behaviour
// ---------------------------------------------------------------------------

template <typename C, typename D, typename = void>
struct AcceptsDestination : std::false_type {};

template <typename C, typename D>
struct AcceptsDestination<
    C, D,
    std::void_t<decltype(std::declval<C&>().CopyTo(std::declval<D>(), intcs{0}))>>
    : std::true_type {};

// The removed raw boundary. Every one of these was a legal call before ticket
// #1771 and is now a compile error naming the surviving overloads, which is the
// migration diagnostic: old code cannot compile, so it cannot corrupt memory.
static_assert(!AcceptsDestination<ICollection, void*>::value,
              "ICollection::CopyTo(void*, intcs) must not be callable");
static_assert(!AcceptsDestination<ArrayList, void*>::value);
static_assert(!AcceptsDestination<ArrayList, std::any*>::value);
static_assert(!AcceptsDestination<Queue, void*>::value);
static_assert(!AcceptsDestination<Queue, void**>::value);
static_assert(!AcceptsDestination<Stack, void*>::value);
static_assert(!AcceptsDestination<Stack, void**>::value);
static_assert(!AcceptsDestination<Hashtable, void*>::value);
static_assert(!AcceptsDestination<Hashtable, DictionaryEntry*>::value);
static_assert(!AcceptsDestination<ListDictionaryInternal, void*>::value);
static_assert(!AcceptsDestination<ListDictionaryInternal, DictionaryEntry*>::value);
static_assert(!AcceptsDestination<IList, void*>::value);
static_assert(!AcceptsDestination<IDictionary, void*>::value);
static_assert(!AcceptsDestination<ProbeCollection, int*>::value);

// The surviving overloads, and only those.
static_assert(AcceptsDestination<ICollection, ObjectSpan>::value);
static_assert(AcceptsDestination<ICollection, std::vector<std::any>&>::value);
static_assert(AcceptsDestination<ArrayList, std::vector<std::any>&>::value);
static_assert(AcceptsDestination<Queue, std::vector<std::any>&>::value);
static_assert(AcceptsDestination<Queue, std::vector<void*>&>::value);
static_assert(AcceptsDestination<Stack, std::vector<void*>&>::value);
static_assert(AcceptsDestination<Hashtable, std::vector<DictionaryEntry>&>::value);
static_assert(AcceptsDestination<ListDictionaryInternal, std::vector<DictionaryEntry>&>::value);

// A wrong element type is rejected at compile time, so .NET's runtime
// ArrayTypeMismatchException / InvalidCastException paths are unreachable here.
static_assert(!AcceptsDestination<ArrayList, std::vector<DictionaryEntry>&>::value);
static_assert(!AcceptsDestination<Queue, std::vector<DictionaryEntry>&>::value);
static_assert(!AcceptsDestination<Hashtable, std::vector<void*>&>::value);
static_assert(!AcceptsDestination<ICollection, std::vector<void*>&>::value);
static_assert(!AcceptsDestination<ICollection, std::vector<std::string>&>::value);
// A temporary destination would be copied and discarded; only lvalues bind.
static_assert(!AcceptsDestination<ICollection, std::vector<std::any>>::value);

TEST(CopyToBoundaryMigration, RemovedRawOverloadIsNotCallable) {
    // Mirrors the static_asserts above as a runtime-visible assertion so the
    // migration guarantee is reported by the test suite, not only by the build.
    EXPECT_FALSE((AcceptsDestination<ICollection, void*>::value));
    EXPECT_FALSE((AcceptsDestination<Hashtable, DictionaryEntry*>::value));
    EXPECT_TRUE((AcceptsDestination<ICollection, ObjectSpan>::value));
    EXPECT_TRUE((AcceptsDestination<ICollection, std::vector<std::any>&>::value));
}

// ---------------------------------------------------------------------------
// Value lifetime
// ---------------------------------------------------------------------------

struct LifetimeProbe {
    static int liveInstances;
    std::string payload;

    explicit LifetimeProbe(std::string text = {}) : payload(std::move(text)) { ++liveInstances; }
    LifetimeProbe(const LifetimeProbe& other) : payload(other.payload) { ++liveInstances; }
    LifetimeProbe& operator=(const LifetimeProbe& other) {
        payload = other.payload;
        return *this;
    }
    ~LifetimeProbe() { --liveInstances; }
};

int LifetimeProbe::liveInstances = 0;

TEST(CopyToBoundaryLifetime, NonTrivialValuesAreCopiedNotAliased) {
    ASSERT_EQ(LifetimeProbe::liveInstances, 0);
    {
        std::vector<std::any> destination(2);
        {
            ArrayList list;
            list.Add(std::any(LifetimeProbe("first")));
            list.Add(std::any(std::vector<std::string>{"a", "b"}));
            ASSERT_NO_THROW(list.CopyTo(destination, 0));
            EXPECT_EQ(std::any_cast<const LifetimeProbe&>(destination[0]).payload, "first");
        }
        // The source collection is gone; the copied values are still valid.
        EXPECT_EQ(std::any_cast<const LifetimeProbe&>(destination[0]).payload, "first");
        EXPECT_EQ(std::any_cast<const std::vector<std::string>&>(destination[1]).at(1), "b");
        EXPECT_EQ(LifetimeProbe::liveInstances, 1);
    }
    EXPECT_EQ(LifetimeProbe::liveInstances, 0);
}

TEST(CopyToBoundaryLifetime, OverwrittenDestinationValuesAreDestroyedExactlyOnce) {
    ASSERT_EQ(LifetimeProbe::liveInstances, 0);
    {
        std::vector<std::any> destination(3);
        for (std::any& slot : destination) slot = std::any(LifetimeProbe("old"));
        EXPECT_EQ(LifetimeProbe::liveInstances, 3);

        ArrayList list;
        list.Add(std::any(1));
        list.Add(std::any(2));
        ASSERT_NO_THROW(list.CopyTo(destination, 0));

        // Two probes were replaced by ints and destroyed; the third is untouched.
        EXPECT_EQ(LifetimeProbe::liveInstances, 1);
        EXPECT_EQ(std::any_cast<int>(destination[0]), 1);
        EXPECT_EQ(std::any_cast<const LifetimeProbe&>(destination[2]).payload, "old");
    }
    EXPECT_EQ(LifetimeProbe::liveInstances, 0);
}

TEST(CopyToBoundaryLifetime, HeterogeneousValuesRoundTripThroughTheInterface) {
    ArrayList list;
    list.Add(std::any(42));
    list.Add(std::any(std::string("text")));
    list.Add(std::any(2.5));
    list.Add(std::any(true));
    list.Add(std::any(LifetimeProbe("probe")));
    list.Add(std::any(static_cast<void*>(&gSlots[0])));
    list.Add(std::any());  // an empty std::any is this port's boxed null

    ICollection& asCollection = list;
    std::vector<std::any> destination(static_cast<std::size_t>(asCollection.getCountProperty()));
    ASSERT_NO_THROW(asCollection.CopyTo(destination, 0));

    EXPECT_EQ(std::any_cast<int>(destination[0]), 42);
    EXPECT_EQ(std::any_cast<std::string>(destination[1]), "text");
    EXPECT_DOUBLE_EQ(std::any_cast<double>(destination[2]), 2.5);
    EXPECT_TRUE(std::any_cast<bool>(destination[3]));
    EXPECT_EQ(std::any_cast<const LifetimeProbe&>(destination[4]).payload, "probe");
    EXPECT_EQ(std::any_cast<void*>(destination[5]), &gSlots[0]);
    EXPECT_FALSE(destination[6].has_value());
}

TEST(CopyToBoundaryLifetime, LargeCopyIntoExactFitDestinationIsClean) {
    constexpr intcs kCount = 10000;
    ArrayList list;
    for (intcs i = 0; i < kCount; ++i) list.Add(std::any(std::string("v") + std::to_string(i)));

    std::vector<std::any> destination(static_cast<std::size_t>(kCount));
    ASSERT_NO_THROW(list.CopyTo(destination, 0));
    EXPECT_EQ(std::any_cast<std::string>(destination[0]), "v0");
    EXPECT_EQ(std::any_cast<std::string>(destination[kCount - 1]),
              "v" + std::to_string(kCount - 1));

    std::vector<std::any> oneShort(static_cast<std::size_t>(kCount) - 1);
    EXPECT_EQ(classify([&] { list.CopyTo(oneShort, 0); }), Thrown::Argument);
    for (const std::any& slot : oneShort) EXPECT_FALSE(slot.has_value());
}

}  // namespace
