// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1802
// (REMED-COLL-HASHTABLE-REMOVE-VERSION).
//
// Before this ticket all three System::Collections::Hashtable::Remove overloads
// were `_map.erase(key); ++version_;` -- the fail-fast mutation counter advanced
// whether or not the key was present. Measured against the committed headers
// (build-probe/1802_prefix.log, 24 defects over 43 checks): removing an ABSENT
// key moved the counter 3 -> 4, and the InvalidOperationException that followed
// came out of every outstanding enumerator kind -- the IDictionaryEnumerator, the
// key view, the value view, and the same reached through an IDictionary& -- after
// an operation that changed nothing at all. Count and contents were correct
// throughout; the defect is purely in the counter.
//
// That is a FALSE POSITIVE, and it is the exact opposite error from ticket
// #1798's on the sibling implementation, which failed to bump for a mutation that
// really happened. Both are now closed, and both of this port's IDictionary
// implementations follow one rule:
//
//   ADVANCE THE COUNTER EXACTLY WHEN THE DICTIONARY'S OBSERVABLE CONTENT
//   CHANGED; NEVER WHEN THE CALL THROWS OR IS A NO-OP.
//
// That rule is .NET Hashtable's own: Hashtable.Remove calls UpdateVersion()
// *inside* the branch that found and cleared a bucket (Hashtable.cs:968-1005),
// where .NET ListDictionaryInternal.Remove does `version++` first and
// unconditionally. .NET's two implementations genuinely disagree, so "match .NET"
// was never by itself a specification; docs/ListDictionaryInternalSetterDesign.md
// section 9.3 chose the Hashtable rule and section 9.4 recorded this file's
// defect as the last row on which the port's two implementations still differed.
//
// Clear() is the ONE deliberate carve-out and is NOT changed by this ticket: it
// bumps unconditionally, including on an already-empty table. .NET Hashtable
// early-returns when `_count == 0 && _occupancy == 0`, but `_occupancy` counts
// buckets whose collision bit was ever set and std::unordered_map has no
// analogue, so an `if (_map.empty()) return;` would NOT reproduce .NET's rule --
// it would skip the bump where .NET still bumps. The unconditional bump also errs
// safely: it can only invalidate an enumerator that had nothing to read. .NET's
// own ListDictionaryInternal.Clear bumps unconditionally too. Asserted below so
// the decision is a test, not a comment.
//
// Cases about the IDictionary INTERFACE are parameterised over both non-generic
// implementations, the shape ticket #1775 established, so neither can regress
// alone. Cases about Hashtable's own string-keyed domain are written against
// Hashtable directly, because ListDictionaryInternal's key domain is
// address-identity and pretending otherwise would misdescribe one of them.
#include <gtest/gtest.h>

#include <any>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/ListDictionaryInternal.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

using SharpRuntime::intcs;
using System::Collections::DictionaryEntry;
using System::Collections::Hashtable;
using System::Collections::ICollection;
using System::Collections::IDictionary;
using System::Collections::IDictionaryEnumerator;
using System::Collections::IEnumerator;
using System::Collections::ListDictionaryInternal;
using System::Collections::Generic::KeyNotFoundException;

// ---------------------------------------------------------------------------
// Mutation-counter seam. Declared by detail/MutationCounter.hpp and befriended by
// both dictionaries; never defined in production. Reading the counter DIRECTLY is
// what lets "an absent Remove moved the counter by zero" and "a present Remove
// moved it by exactly one" be assertions rather than inferences from an
// enumerator's silence -- and a delta of exactly one is not observable through
// fail-fast at all.
//
// This file spelled its own copy (SR1794_SEAM_BODY) until ticket #1800, which is
// the ticket that closed the divergence #1802 recorded here: four suites spelled
// a version()-only body while CollectionVersionCounterTests.cpp spelled a
// version()+positionVersion() one, and all five shared one program. The single
// definition now lives in ../../support/CollectionVersionSeam.hpp.
// ---------------------------------------------------------------------------
#include "../../support/CollectionVersionSeam.hpp"

namespace {

template<typename T>
using Owning = std::unique_ptr<T>;

template<typename C>
unsigned long long versionOf(const C& c) {
    return SharpRuntime::Testing::CollectionVersionAccess<C>::version(c);
}

/** Three string-keyed entries, the shape most cases below start from. */
Hashtable seeded() {
    Hashtable t;
    t.Add(std::string("alpha"), std::any(1));
    t.Add(std::string("beta"), std::any(2));
    t.Add(std::string("gamma"), std::any(3));
    return t;
}

/** The enumeration order of a table, so "first/middle/last logical entry" is
 *  meaningful for an unordered container. */
std::vector<std::string> enumerationOrder(Hashtable& t) {
    std::vector<std::string> order;
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    while (e->MoveNext()) order.push_back(std::any_cast<std::string>(e->getKeyProperty()));
    return order;
}

/** An absent key that hashes into the same bucket as a present one, so the no-op
 *  case is exercised where a real implementation must walk an occupied chain. */
std::string colliderFor(const std::vector<std::string>& present, const std::string& target) {
    std::unordered_map<std::string, int> shadow;   // same hasher and bucket policy
    for (const std::string& k : present) shadow.emplace(k, 0);
    const std::size_t bucket = shadow.bucket(target);
    for (int i = 0; i < 200000; ++i) {
        const std::string candidate = "collider" + std::to_string(i);
        if (shadow.count(candidate) == 0 && shadow.bucket(candidate) == bucket) return candidate;
    }
    return std::string();
}

// =====================================================================================
// 1. Version delta, per Remove outcome, on all three overloads
// =====================================================================================

TEST(HashtableRemoveVersioning, AnAbsentKeyMovesTheCounterByZeroOnAllThreeOverloads) {
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Remove(std::string("absent"));
        EXPECT_EQ(versionOf(t), before) << "Remove(const std::string&)";
    }
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Remove("absent");
        EXPECT_EQ(versionOf(t), before) << "Remove(const char*)";
    }
    {
        Hashtable t = seeded();
        int anchor = 0;
        const unsigned long long before = versionOf(t);
        t.Remove(static_cast<const void*>(&anchor));
        EXPECT_EQ(versionOf(t), before) << "Remove(const void*)";
    }
}

TEST(HashtableRemoveVersioning, APresentKeyMovesTheCounterByExactlyOneOnAllThreeOverloads) {
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Remove(std::string("beta"));
        EXPECT_EQ(versionOf(t), before + 1) << "Remove(const std::string&)";
    }
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Remove("beta");
        EXPECT_EQ(versionOf(t), before + 1) << "Remove(const char*)";
    }
    {
        Hashtable t = seeded();
        int anchor = 0;
        std::any value = std::any(7);
        t.Add(static_cast<const void*>(&anchor), &value);
        const unsigned long long before = versionOf(t);
        t.Remove(static_cast<const void*>(&anchor));
        EXPECT_EQ(versionOf(t), before + 1) << "Remove(const void*)";
    }
}

TEST(HashtableRemoveVersioning, RepeatedAbsentRemovalsMoveTheCounterByZeroEveryTime) {
    Hashtable t = seeded();
    const unsigned long long before = versionOf(t);
    for (int i = 0; i < 32; ++i) {
        t.Remove(std::string("absent"));
        EXPECT_EQ(versionOf(t), before) << "iteration " << i;
    }
    EXPECT_EQ(t.getCountProperty(), 3);
}

TEST(HashtableRemoveVersioning, RemovingTheSameKeyTwiceBumpsOnceNotTwice) {
    Hashtable t = seeded();
    const unsigned long long before = versionOf(t);
    t.Remove(std::string("beta"));
    EXPECT_EQ(versionOf(t), before + 1);
    t.Remove(std::string("beta"));                       // now absent: a no-op
    EXPECT_EQ(versionOf(t), before + 1);
    t.Remove(std::string("beta"));
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(t.getCountProperty(), 2);
}

TEST(HashtableRemoveVersioning, ARejectedNullKeyMovesTheCounterByZero) {
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        EXPECT_THROW(t.Remove(static_cast<const void*>(nullptr)), System::ArgumentNullException);
        EXPECT_EQ(versionOf(t), before);
        EXPECT_EQ(t.getCountProperty(), 3);
    }
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        EXPECT_THROW(t.Remove(static_cast<const char*>(nullptr)), System::ArgumentNullException);
        EXPECT_EQ(versionOf(t), before);
        EXPECT_EQ(t.getCountProperty(), 3);
    }
}

TEST(HashtableRemoveVersioning, TheNullKeyExceptionContractIsUnchanged) {
    // #1775 established it and #1796 re-asserted it; this ticket must not weaken it.
    Hashtable t = seeded();
    try {
        t.Remove(static_cast<const void*>(nullptr));
        FAIL() << "a null raw key must be rejected";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getMessageProperty(), "Value cannot be null. (Parameter 'key')");
    }
    try {
        t.Remove(static_cast<const char*>(nullptr));
        FAIL() << "a null C-string key must be rejected";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ(e.getMessageProperty(), "Value cannot be null. (Parameter 'key')");
    }
}

TEST(HashtableRemoveVersioning, ClearKeepsItsUnconditionalBumpIncludingOnAnEmptyTable) {
    // The one deliberate carve-out from "advance on effective mutation", decided by
    // #1802 rather than left implicit. See this file's header comment for why the
    // obvious `if (_map.empty()) return;` would not reproduce .NET Hashtable's rule.
    {
        Hashtable empty;
        const unsigned long long before = versionOf(empty);
        empty.Clear();
        EXPECT_EQ(versionOf(empty), before + 1) << "an already-empty Clear still bumps";
        empty.Clear();
        EXPECT_EQ(versionOf(empty), before + 2);
    }
    {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Clear();
        EXPECT_EQ(versionOf(t), before + 1);
        EXPECT_EQ(t.getCountProperty(), 0);
    }
}

TEST(HashtableRemoveVersioning, AddAndReplacementVersioningIsUnchanged) {
    // #1796's rules, re-asserted so this ticket cannot move them by accident.
    Hashtable t;
    const unsigned long long v0 = versionOf(t);
    t.Add(std::string("k"), std::any(1));
    EXPECT_EQ(versionOf(t), v0 + 1) << "insert";
    t.setItem(std::string("k"), std::any(2));
    EXPECT_EQ(versionOf(t), v0 + 2) << "replace";
    t.setItem(std::string("k"), std::any(2));
    EXPECT_EQ(versionOf(t), v0 + 3) << "equal-value replace still bumps";
    t[std::string("k")] = std::any(3);
    EXPECT_EQ(versionOf(t), v0 + 4) << "proxy assignment";
    EXPECT_THROW(t.Add(std::string("k"), std::any(4)), System::ArgumentException);
    EXPECT_EQ(versionOf(t), v0 + 4) << "a throwing duplicate Add bumps nothing";
}

// =====================================================================================
// 2. Enumerator invalidation matrix
// =====================================================================================
//
// Four enumerator families reach a Hashtable: its own IDictionaryEnumerator; the
// non-generic IEnumerator adaptation the views return (MemberEnumerator), in its
// key and value flavours; and the same IDictionaryEnumerator reached through an
// IDictionary& base reference. Every family is driven through one helper so a
// family cannot be silently omitted.

enum class Family { Dictionary, KeyView, ValueView, InterfaceDictionary };

const char* familyName(Family f) {
    switch (f) {
        case Family::Dictionary:          return "IDictionaryEnumerator";
        case Family::KeyView:             return "key-view MemberEnumerator";
        case Family::ValueView:           return "value-view MemberEnumerator";
        case Family::InterfaceDictionary: return "IDictionaryEnumerator via IDictionary&";
    }
    return "?";
}

/** Owns whichever enumerator (and, for a view, the view itself) the family needs,
 *  and exposes the two operations whose fail-fast behaviour is under test. */
class AnyFamilyEnumerator {
    Owning<ICollection> view_;
    Owning<IEnumerator> viewEnumerator_;
    Owning<IDictionaryEnumerator> dictionaryEnumerator_;

public:
    AnyFamilyEnumerator(Hashtable& t, Family f) {
        switch (f) {
            case Family::Dictionary:
                dictionaryEnumerator_.reset(t.GetEnumerator());
                break;
            case Family::InterfaceDictionary: {
                IDictionary& d = t;
                dictionaryEnumerator_.reset(d.GetEnumerator());
                break;
            }
            case Family::KeyView:
                view_.reset(t.getKeysProperty());
                viewEnumerator_.reset(view_->GetEnumerator());
                break;
            case Family::ValueView:
                view_.reset(t.getValuesProperty());
                viewEnumerator_.reset(view_->GetEnumerator());
                break;
        }
    }

    bool MoveNext() {
        return dictionaryEnumerator_ ? dictionaryEnumerator_->MoveNext()
                                     : viewEnumerator_->MoveNext();
    }
    void Reset() {
        if (dictionaryEnumerator_) dictionaryEnumerator_->Reset(); else viewEnumerator_->Reset();
    }
    std::any Current() const {
        return dictionaryEnumerator_ ? dictionaryEnumerator_->getCurrentProperty()
                                     : viewEnumerator_->getCurrentProperty();
    }
};

class HashtableRemoveEnumeratorFamily : public ::testing::TestWithParam<Family> {};

INSTANTIATE_TEST_SUITE_P(AllFamilies, HashtableRemoveEnumeratorFamily,
                         ::testing::Values(Family::Dictionary, Family::KeyView,
                                           Family::ValueView, Family::InterfaceDictionary),
                         [](const ::testing::TestParamInfo<Family>& info) {
                             switch (info.param) {
                                 case Family::Dictionary:          return "Dictionary";
                                 case Family::KeyView:             return "KeyView";
                                 case Family::ValueView:           return "ValueView";
                                 case Family::InterfaceDictionary: return "InterfaceDictionary";
                             }
                             return "Unknown";
                         });

TEST_P(HashtableRemoveEnumeratorFamily, SurvivesAnAbsentRemoveAndKeepsEnumerating) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    ASSERT_TRUE(e.MoveNext()) << familyName(GetParam());

    const unsigned long long before = versionOf(t);
    t.Remove(std::string("absent"));
    EXPECT_EQ(versionOf(t), before) << familyName(GetParam());

    int remaining = 0;
    EXPECT_NO_THROW({ while (e.MoveNext()) ++remaining; }) << familyName(GetParam());
    EXPECT_EQ(remaining, 2) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, SurvivesRepeatedAbsentRemovesInterleavedWithMoveNext) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    const unsigned long long before = versionOf(t);

    int visited = 0;
    EXPECT_NO_THROW({
        while (e.MoveNext()) {
            ++visited;
            t.Remove(std::string("never-present-" + std::to_string(visited)));
        }
    }) << familyName(GetParam());
    EXPECT_EQ(visited, 3) << familyName(GetParam());
    EXPECT_EQ(versionOf(t), before) << familyName(GetParam());
    EXPECT_EQ(t.getCountProperty(), 3) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, ResetStillWorksAfterAnAbsentRemove) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    ASSERT_TRUE(e.MoveNext());
    t.Remove(std::string("absent"));

    EXPECT_NO_THROW(e.Reset()) << familyName(GetParam());
    int visited = 0;
    EXPECT_NO_THROW({ while (e.MoveNext()) ++visited; }) << familyName(GetParam());
    EXPECT_EQ(visited, 3) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, IsInvalidatedByAnEffectiveRemove) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    ASSERT_TRUE(e.MoveNext());

    const unsigned long long before = versionOf(t);
    t.Remove(std::string("beta"));
    EXPECT_EQ(versionOf(t), before + 1) << familyName(GetParam());

    EXPECT_THROW(e.MoveNext(), System::InvalidOperationException) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, ResetIsInvalidatedByAnEffectiveRemove) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    ASSERT_TRUE(e.MoveNext());
    t.Remove(std::string("beta"));
    EXPECT_THROW(e.Reset(), System::InvalidOperationException) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, SurvivesARejectedNullKey) {
    Hashtable t = seeded();
    AnyFamilyEnumerator e(t, GetParam());
    ASSERT_TRUE(e.MoveNext());

    const unsigned long long before = versionOf(t);
    EXPECT_THROW(t.Remove(static_cast<const void*>(nullptr)), System::ArgumentNullException);
    EXPECT_THROW(t.Remove(static_cast<const char*>(nullptr)), System::ArgumentNullException);
    EXPECT_EQ(versionOf(t), before) << familyName(GetParam());

    int remaining = 0;
    EXPECT_NO_THROW({ while (e.MoveNext()) ++remaining; }) << familyName(GetParam());
    EXPECT_EQ(remaining, 2) << familyName(GetParam());
}

TEST_P(HashtableRemoveEnumeratorFamily, ReadsNeitherDuplicatesNorLosesAnEntryAcrossAnAbsentRemove) {
    Hashtable t;
    for (int i = 0; i < 64; ++i) t.Add("key" + std::to_string(i), std::any(i));

    AnyFamilyEnumerator e(t, GetParam());
    std::multiset<std::string> seen;
    int step = 0;
    while (e.MoveNext()) {
        const std::any current = e.Current();
        if (GetParam() == Family::KeyView) {
            seen.insert(std::any_cast<std::string>(current));
        } else if (GetParam() == Family::ValueView) {
            seen.insert(std::to_string(std::any_cast<int>(current)));
        } else {
            seen.insert(std::any_cast<std::string>(
                std::any_cast<DictionaryEntry>(current).getKeyProperty()));
        }
        t.Remove(std::string("no-such-key-" + std::to_string(step++)));
    }

    EXPECT_EQ(seen.size(), 64u) << familyName(GetParam());
    std::set<std::string> distinct(seen.begin(), seen.end());
    EXPECT_EQ(distinct.size(), 64u) << familyName(GetParam()) << ": no duplicates";
    EXPECT_EQ(t.getCountProperty(), 64) << familyName(GetParam());
}

// =====================================================================================
// 3. Contents, Count and boundary shapes
// =====================================================================================

TEST(HashtableRemoveContents, AnAbsentRemoveChangesNeitherCountNorContents) {
    Hashtable t = seeded();
    t.Remove(std::string("absent"));
    EXPECT_EQ(t.getCountProperty(), 3);
    EXPECT_TRUE(t.ContainsKey("alpha"));
    EXPECT_TRUE(t.ContainsKey("beta"));
    EXPECT_TRUE(t.ContainsKey("gamma"));
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 1);
    EXPECT_EQ(std::any_cast<int>(t.at("beta")), 2);
    EXPECT_EQ(std::any_cast<int>(t.at("gamma")), 3);
}

TEST(HashtableRemoveContents, APresentRemoveRemovesExactlyOneEntryAndKeepsTheRest) {
    Hashtable t = seeded();
    t.Remove(std::string("beta"));
    EXPECT_EQ(t.getCountProperty(), 2);
    EXPECT_TRUE(t.ContainsKey("alpha"));
    EXPECT_FALSE(t.ContainsKey("beta"));
    EXPECT_TRUE(t.ContainsKey("gamma"));
    EXPECT_EQ(std::any_cast<int>(t.at("alpha")), 1);
    EXPECT_EQ(std::any_cast<int>(t.at("gamma")), 3);
    EXPECT_THROW((void)t.at("beta"), KeyNotFoundException);
}

TEST(HashtableRemoveContents, AnEmptyTableAbsorbsAnAbsentRemove) {
    Hashtable t;
    const unsigned long long before = versionOf(t);
    t.Remove(std::string("anything"));
    t.Remove("anything");
    EXPECT_EQ(versionOf(t), before);
    EXPECT_EQ(t.getCountProperty(), 0);
}

TEST(HashtableRemoveContents, AOneElementTableDistinguishesTheAbsentFromThePresentKey) {
    Hashtable t;
    t.Add(std::string("only"), std::any(1));
    const unsigned long long v0 = versionOf(t);

    t.Remove(std::string("other"));
    EXPECT_EQ(versionOf(t), v0);
    EXPECT_EQ(t.getCountProperty(), 1);

    t.Remove(std::string("only"));
    EXPECT_EQ(versionOf(t), v0 + 1);
    EXPECT_EQ(t.getCountProperty(), 0);

    t.Remove(std::string("only"));                       // now absent
    EXPECT_EQ(versionOf(t), v0 + 1);
}

TEST(HashtableRemoveContents, EachOfTheFirstMiddleAndLastLogicalEntryRemovesExactlyItself) {
    Hashtable probe = seeded();
    const std::vector<std::string> order = enumerationOrder(probe);
    ASSERT_EQ(order.size(), 3u);

    for (std::size_t i = 0; i < order.size(); ++i) {
        Hashtable t = seeded();
        const unsigned long long before = versionOf(t);
        t.Remove(order[i]);
        EXPECT_EQ(versionOf(t), before + 1) << "logical entry " << i;
        EXPECT_EQ(t.getCountProperty(), 2) << "logical entry " << i;
        EXPECT_FALSE(t.ContainsKey(order[i])) << "logical entry " << i;
        for (std::size_t j = 0; j < order.size(); ++j) {
            if (j != i) {
                EXPECT_TRUE(t.ContainsKey(order[j])) << "entry " << j << " survives";
            }
        }
    }
}

TEST(HashtableRemoveContents, AnAbsentKeyInAnOccupiedBucketIsStillANoOp) {
    Hashtable t;
    std::vector<std::string> present;
    for (int i = 0; i < 64; ++i) {
        present.push_back("key" + std::to_string(i));
        t.Add(present.back(), std::any(i));
    }
    const std::string collider = colliderFor(present, "key0");
    ASSERT_FALSE(collider.empty()) << "no colliding key found";
    ASSERT_FALSE(t.ContainsKey(collider));

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    const unsigned long long before = versionOf(t);
    t.Remove(collider);
    EXPECT_EQ(versionOf(t), before);
    EXPECT_EQ(t.getCountProperty(), 64);
    EXPECT_NO_THROW((void)e->MoveNext());
}

TEST(HashtableRemoveContents, TheEmptyStringAndZeroKeysBehaveLikeAnyOtherKey) {
    for (const std::string& key : {std::string(""), std::string("0")}) {
        Hashtable t = seeded();
        const unsigned long long v0 = versionOf(t);

        t.Remove(key);                                   // absent
        EXPECT_EQ(versionOf(t), v0) << "key='" << key << "'";
        EXPECT_EQ(t.getCountProperty(), 3) << "key='" << key << "'";

        t.Add(key, std::any(9));
        EXPECT_EQ(versionOf(t), v0 + 1) << "key='" << key << "'";
        t.Remove(key);                                   // present
        EXPECT_EQ(versionOf(t), v0 + 2) << "key='" << key << "'";
        EXPECT_EQ(t.getCountProperty(), 3) << "key='" << key << "'";
        EXPECT_FALSE(t.ContainsKey(key)) << "key='" << key << "'";
    }
}

TEST(HashtableRemoveContents, ARawPointerKeyAndTheStringKeyZeroStillNameDifferentEntries) {
    // #1775's key-space separation, re-asserted across a Remove: no non-null address
    // stringifies to "0", so removing one must not touch the other.
    Hashtable t;
    int anchor = 1;
    std::any value = std::any(10);
    t.Add(std::string("0"), std::any(7));
    t.Add(static_cast<const void*>(&anchor), &value);
    ASSERT_EQ(t.getCountProperty(), 2);

    t.Remove(std::string("0"));
    EXPECT_EQ(t.getCountProperty(), 1);
    EXPECT_TRUE(t.Contains(&anchor));

    t.Remove(static_cast<const void*>(&anchor));
    EXPECT_EQ(t.getCountProperty(), 0);
}

TEST(HashtableRemoveContents, RemovalAfterAReplacementBumpsOnceForEach) {
    Hashtable t = seeded();
    const unsigned long long v0 = versionOf(t);
    t.setItem(std::string("beta"), std::any(22));
    EXPECT_EQ(versionOf(t), v0 + 1);
    EXPECT_EQ(std::any_cast<int>(t.at("beta")), 22);
    t.Remove(std::string("beta"));
    EXPECT_EQ(versionOf(t), v0 + 2);
    t.Remove(std::string("beta"));
    EXPECT_EQ(versionOf(t), v0 + 2);
}

TEST(HashtableRemoveContents, MissingKeyReadsStillInsertNothingAndTheFollowingRemoveIsStillANoOp) {
    // #1796's rule -- a bare read of an absent key does NOT structurally insert --
    // combined with this ticket's: neither the reads nor the Remove may move anything.
    Hashtable t = seeded();
    const unsigned long long v0 = versionOf(t);
    const Hashtable& ct = t;

    for (int i = 0; i < 256; ++i) {
        const std::string key = "missing" + std::to_string(i);
        EXPECT_FALSE(std::any(t[key]).has_value());
        EXPECT_FALSE(ct[key].has_value());
        EXPECT_FALSE(t.getItem(&i).has_value());
        t.Remove(key);
    }

    EXPECT_EQ(t.getCountProperty(), 3);
    EXPECT_EQ(versionOf(t), v0);
    EXPECT_TRUE(t.ContainsKey("alpha"));
    EXPECT_TRUE(t.ContainsKey("beta"));
    EXPECT_TRUE(t.ContainsKey("gamma"));
}

TEST(HashtableRemoveContents, PointerValuedAndNonTrivialValuesAreUntouchedByAnAbsentRemove) {
    auto shared = std::make_shared<std::string>("owned");
    int target = 5;
    Hashtable t;
    t.Add(std::string("shared"), std::any(shared));
    t.Add(std::string("string"), std::any(std::string("a non-trivial value")));
    t.Add(std::string("pointer"), std::any(static_cast<void*>(&target)));

    const unsigned long long before = versionOf(t);
    const long useBefore = shared.use_count();
    t.Remove(std::string("nothing-here"));

    EXPECT_EQ(versionOf(t), before);
    EXPECT_EQ(shared.use_count(), useBefore);
    EXPECT_EQ(t.getCountProperty(), 3);
    EXPECT_EQ(*std::any_cast<std::shared_ptr<std::string>>(t.at("shared")), "owned");
    EXPECT_EQ(std::any_cast<std::string>(t.at("string")), "a non-trivial value");
    EXPECT_EQ(std::any_cast<void*>(t.at("pointer")), static_cast<void*>(&target));

    t.Remove(std::string("shared"));
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(shared.use_count(), useBefore - 1) << "the erased entry released its value";
}

TEST(HashtableRemoveContents, ALargeTableAnswersBothOutcomesCorrectly) {
    Hashtable t;
    for (int i = 0; i < 20000; ++i) t.Add("key" + std::to_string(i), std::any(i));

    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const unsigned long long before = versionOf(t);
    for (int i = 0; i < 100; ++i) t.Remove("absent" + std::to_string(i));
    EXPECT_EQ(versionOf(t), before);
    EXPECT_EQ(t.getCountProperty(), 20000);
    EXPECT_NO_THROW((void)e->MoveNext()) << "100 no-ops invalidated nothing";

    t.Remove(std::string("key19999"));
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(t.getCountProperty(), 19999);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);

    for (int i = 0; i < 19999; ++i) t.Remove("key" + std::to_string(i));
    EXPECT_EQ(t.getCountProperty(), 0);
    EXPECT_EQ(versionOf(t), before + 20000) << "exactly one bump per erased entry";
}

// =====================================================================================
// 4. The IDictionary interface -- both implementations must answer identically
// =====================================================================================

/** Presents either non-generic IDictionary through one key domain-neutral API. */
class InterfaceHarness {
public:
    virtual ~InterfaceHarness() = default;
    virtual IDictionary& dictionary() = 0;
    virtual void addEntries(int count) = 0;
    /** A valid key that is guaranteed NOT to be in the dictionary. */
    virtual const void* absentKey() = 0;
    /** A valid key that IS in the dictionary. */
    virtual const void* presentKey() = 0;
    virtual unsigned long long version() const = 0;
    virtual const char* name() const = 0;
};

class HashtableHarness : public InterfaceHarness {
    Hashtable table_;
    std::vector<std::string> keys_;
    std::vector<std::any> values_;
    int absentAnchor_ = 0;
    std::vector<std::unique_ptr<int>> anchors_;

public:
    IDictionary& dictionary() override { return table_; }
    void addEntries(int count) override {
        for (int i = 0; i < count; ++i) {
            anchors_.push_back(std::make_unique<int>(i));
            values_.push_back(std::any(i));
            table_.Add(static_cast<const void*>(anchors_.back().get()), &values_.back());
        }
    }
    const void* absentKey() override { return static_cast<const void*>(&absentAnchor_); }
    const void* presentKey() override { return static_cast<const void*>(anchors_.front().get()); }
    unsigned long long version() const override { return versionOf(table_); }
    const char* name() const override { return "Hashtable"; }
};

class ListDictionaryHarness : public InterfaceHarness {
    ListDictionaryInternal dictionary_;
    std::vector<std::unique_ptr<int>> keys_;
    std::vector<std::unique_ptr<int>> values_;
    int absentAnchor_ = 0;

public:
    IDictionary& dictionary() override { return dictionary_; }
    void addEntries(int count) override {
        for (int i = 0; i < count; ++i) {
            keys_.push_back(std::make_unique<int>(i));
            values_.push_back(std::make_unique<int>(i * 10));
            dictionary_.Add(static_cast<const void*>(keys_.back().get()), values_.back().get());
        }
    }
    const void* absentKey() override { return static_cast<const void*>(&absentAnchor_); }
    const void* presentKey() override { return static_cast<const void*>(keys_.front().get()); }
    unsigned long long version() const override { return versionOf(dictionary_); }
    const char* name() const override { return "ListDictionaryInternal"; }
};

using HarnessFactory = std::unique_ptr<InterfaceHarness> (*)();

std::unique_ptr<InterfaceHarness> makeHashtable() { return std::make_unique<HashtableHarness>(); }
std::unique_ptr<InterfaceHarness> makeListDictionary() {
    return std::make_unique<ListDictionaryHarness>();
}

class NonGenericDictionaryRemoveVersioning : public ::testing::TestWithParam<HarnessFactory> {
protected:
    void SetUp() override { harness_ = GetParam()(); }
    InterfaceHarness& harness() { return *harness_; }
    IDictionary& dictionary() { return harness_->dictionary(); }

private:
    std::unique_ptr<InterfaceHarness> harness_;
};

INSTANTIATE_TEST_SUITE_P(BothImplementations, NonGenericDictionaryRemoveVersioning,
                         ::testing::Values(&makeHashtable, &makeListDictionary),
                         [](const ::testing::TestParamInfo<HarnessFactory>& info) {
                             return info.index == 0 ? "Hashtable" : "ListDictionaryInternal";
                         });

TEST_P(NonGenericDictionaryRemoveVersioning, AnAbsentRemoveMovesNothingThroughTheInterface) {
    harness().addEntries(4);
    const unsigned long long before = harness().version();
    const intcs countBefore = dictionary().getCountProperty();

    dictionary().Remove(harness().absentKey());

    EXPECT_EQ(harness().version(), before) << harness().name();
    EXPECT_EQ(dictionary().getCountProperty(), countBefore) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, APresentRemoveMovesTheCounterByExactlyOne) {
    harness().addEntries(4);
    const unsigned long long before = harness().version();

    dictionary().Remove(harness().presentKey());

    EXPECT_EQ(harness().version(), before + 1) << harness().name();
    EXPECT_EQ(dictionary().getCountProperty(), 3) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, AnOutstandingEnumeratorSurvivesAnAbsentRemove) {
    harness().addEntries(4);
    Owning<IDictionaryEnumerator> e(dictionary().GetEnumerator());
    ASSERT_TRUE(e->MoveNext()) << harness().name();

    dictionary().Remove(harness().absentKey());

    int remaining = 0;
    EXPECT_NO_THROW({ while (e->MoveNext()) ++remaining; }) << harness().name();
    EXPECT_EQ(remaining, 3) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, AnOutstandingEnumeratorFailsFastAfterAPresentRemove) {
    harness().addEntries(4);
    Owning<IDictionaryEnumerator> e(dictionary().GetEnumerator());
    ASSERT_TRUE(e->MoveNext()) << harness().name();

    dictionary().Remove(harness().presentKey());

    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, ARejectedNullKeyNeitherMutatesNorInvalidates) {
    harness().addEntries(4);
    Owning<IDictionaryEnumerator> e(dictionary().GetEnumerator());
    ASSERT_TRUE(e->MoveNext()) << harness().name();
    const unsigned long long before = harness().version();

    EXPECT_THROW(dictionary().Remove(nullptr), System::ArgumentNullException) << harness().name();

    EXPECT_EQ(harness().version(), before) << harness().name();
    EXPECT_EQ(dictionary().getCountProperty(), 4) << harness().name();
    EXPECT_NO_THROW((void)e->MoveNext()) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, RepeatedAbsentRemovesStayNoOps) {
    harness().addEntries(4);
    const unsigned long long before = harness().version();
    for (int i = 0; i < 16; ++i) {
        dictionary().Remove(harness().absentKey());
        EXPECT_EQ(harness().version(), before) << harness().name() << " iteration " << i;
    }
    EXPECT_EQ(dictionary().getCountProperty(), 4) << harness().name();
}

TEST_P(NonGenericDictionaryRemoveVersioning, ClearBumpsUnconditionallyOnBothImplementations) {
    // The single shared carve-out. Stated as an assertion so that changing either
    // implementation's Clear() has to change this test on purpose.
    const unsigned long long before = harness().version();
    dictionary().Clear();
    EXPECT_EQ(harness().version(), before + 1) << harness().name() << ": empty Clear bumps";
    dictionary().Clear();
    EXPECT_EQ(harness().version(), before + 2) << harness().name();
}

// =====================================================================================
// 5. No regression to the #1794/#1796 contracts this ticket sits next to
// =====================================================================================

TEST(HashtableRemoveNoRegression, EnumeratorSnapshotsStillOwnTheirDataAcrossAnAbsentRemove) {
    Hashtable t = seeded();
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    const DictionaryEntry entry = e->getEntryProperty();

    t.Remove(std::string("absent"));

    // The snapshots are owning copies (#1794) and the enumerator is still valid
    // (#1802), so both the boxes and further enumeration keep working.
    EXPECT_EQ(key.type(), typeid(std::string));
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(std::any_cast<std::string>(entry.getKeyProperty()),
              std::any_cast<std::string>(key));
    EXPECT_NO_THROW((void)e->MoveNext());
}

TEST(HashtableRemoveNoRegression, EnumeratorSnapshotsStillOwnTheirDataAcrossAnEffectiveRemove) {
    Hashtable t = seeded();
    Owning<IDictionaryEnumerator> e(t.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());

    const std::any key = e->getKeyProperty();
    const std::any value = e->getValueProperty();
    t.Remove(std::any_cast<std::string>(key));

    EXPECT_EQ(key.type(), typeid(std::string));
    EXPECT_TRUE(value.has_value());
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(HashtableRemoveNoRegression, ValuesReadBeforeARemoveStillSurviveIt) {
    Hashtable t = seeded();
    const std::any owned = t["beta"];
    const std::any throughGetItem = t.at("beta");

    t.Remove(std::string("absent"));
    EXPECT_EQ(std::any_cast<int>(owned), 2);
    t.Remove(std::string("beta"));
    EXPECT_EQ(std::any_cast<int>(owned), 2) << "the copy is independent of the table";
    EXPECT_EQ(std::any_cast<int>(throughGetItem), 2);
}

TEST(HashtableRemoveNoRegression, TheViewsStayLiveAcrossBothRemoveOutcomes) {
    Hashtable t = seeded();
    Owning<ICollection> keys(t.getKeysProperty());
    Owning<ICollection> values(t.getValuesProperty());

    t.Remove(std::string("absent"));
    EXPECT_EQ(keys->getCountProperty(), 3);
    EXPECT_EQ(values->getCountProperty(), 3);

    t.Remove(std::string("beta"));
    EXPECT_EQ(keys->getCountProperty(), 2);
    EXPECT_EQ(values->getCountProperty(), 2);
}

TEST(HashtableRemoveNoRegression, AProxyRemainsUsableAcrossAnAbsentRemoveOfItsOwnKey) {
    Hashtable t = seeded();
    Hashtable::ValueReference r = t["delta"];             // absent
    const unsigned long long before = versionOf(t);

    t.Remove(std::string("delta"));
    EXPECT_EQ(versionOf(t), before) << "removing the proxy's absent key is a no-op";
    EXPECT_FALSE(r.hasValueProperty());

    r = std::any(4);                                      // and assigning still inserts
    EXPECT_EQ(versionOf(t), before + 1);
    EXPECT_EQ(t.getCountProperty(), 4);
    EXPECT_EQ(std::any_cast<int>(t.at("delta")), 4);
}

}  // namespace
