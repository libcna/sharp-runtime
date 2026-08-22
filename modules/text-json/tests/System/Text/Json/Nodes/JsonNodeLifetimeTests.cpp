// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1886 (SR-AUD-327, CCF-019): JsonArray and JsonObject
// clear the parent link of every child they still own in their own destructor, so a child a
// caller retained past its owner's destruction reports no parent instead of dereferencing a
// dangling raw pointer.
//
// The contract these tests pin is documented in docs/OwnedTreeLifetimeContractPlan.md sections
// 13 and 31 item 1: after the owning container is destroyed a retained child is left in exactly
// the state RemoveAt()/Remove()/Clear() already produce - no parent, its own root, and
// re-attachable. Every case below is one of the shapes probe
// build-probe/1885_ccf019_lifetime_probe.cpp measured against the shipped bodies; the probe's
// case identifiers (J01, J04, J08, ...) are quoted so the two records stay linked.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

using System::Text::Json::JsonValueKind;
using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

namespace detail1888 {
    /// Dependent parameter, per the #2299 gcc trap: a non-dependent `requires` on a missing or
    /// inaccessible name hard-errors instead of yielding false.
    template <typename T> concept HasPublicDetachParent = requires(T& t) { t.DetachParent(); };
}

namespace {

    std::shared_ptr<JsonNode> str(const std::string& s) { return JsonValue::Create(s); }

} // namespace

// --- The owner is alive: nothing changes -------------------------------------------------

// Establishes the baseline the destructor must not disturb: while the owner lives, a child
// reports it as its parent and the tree root as its root.
TEST(JsonNodeLifetimeTests, LiveArrayOwner_ChildStillReportsItsParent) {
    JsonArray arr;
    arr.Add(str("v"));
    auto child = arr[0];
    EXPECT_EQ(child->getParentProperty(), &arr);
    EXPECT_EQ(child->getRootProperty(), &arr);
    EXPECT_EQ(arr.getCountProperty(), 1);
}

TEST(JsonNodeLifetimeTests, LiveObjectOwner_ValueStillReportsItsParent) {
    JsonObject obj;
    obj.Add("k", str("v"));
    auto value = obj["k"];
    EXPECT_EQ(value->getParentProperty(), &obj);
    EXPECT_EQ(value->getRootProperty(), &obj);
}

// --- Owner destruction detaches (J01, J02, J03) ------------------------------------------

// Probe case J01: heap JsonArray freed while a caller still holds a child. Before #1886 this
// read freed storage inside getRootProperty(); ASan reported heap-use-after-free.
TEST(JsonNodeLifetimeTests, RetainedChild_AfterHeapArrayDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> child;
    {
        auto arr = std::make_shared<JsonArray>();
        arr->Add(str("v"));
        child = (*arr)[0];
        ASSERT_EQ(child->getParentProperty(), arr.get());
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(child->getRootProperty(), child.get());
    EXPECT_EQ(child->GetValueKind(), JsonValueKind::String);
    EXPECT_EQ(child->ToJsonString(), "\"v\"");
}

// Probe case J03: the same for JsonObject.
TEST(JsonNodeLifetimeTests, RetainedValue_AfterHeapObjectDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> value;
    {
        auto obj = std::make_shared<JsonObject>();
        obj->Add("k", str("v"));
        value = (*obj)["k"];
        ASSERT_EQ(value->getParentProperty(), obj.get());
    }
    EXPECT_EQ(value->getParentProperty(), nullptr);
    EXPECT_EQ(value->getRootProperty(), value.get());
}

// An automatic-storage owner is the other half of the same contract, and is the storage class
// the repository's own tests use most; candidate designs A and D (weak_ptr / handle) could not
// have served it at all (plan section 12).
TEST(JsonNodeLifetimeTests, RetainedChild_AfterAutomaticStorageArrayDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> child;
    {
        JsonArray arr;
        arr.Add(str("v"));
        child = arr[0];
        ASSERT_EQ(child->getParentProperty(), &arr);
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(child->getRootProperty(), child.get());
}

TEST(JsonNodeLifetimeTests, RetainedValue_AfterAutomaticStorageObjectDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> value;
    {
        JsonObject obj;
        obj.Add("k", str("v"));
        value = obj["k"];
        ASSERT_EQ(value->getParentProperty(), &obj);
    }
    EXPECT_EQ(value->getParentProperty(), nullptr);
}

// Probe case J02: getParentProperty()->GetValueKind() dispatched a virtual call through freed
// storage. With no parent to dispatch through, the call is simply not made.
TEST(JsonNodeLifetimeTests, RetainedChild_AfterOwnerDestroyed_ParentIsNotDereferenceable) {
    std::shared_ptr<JsonNode> child;
    {
        JsonArray arr;
        arr.Add(str("v"));
        child = arr[0];
    }
    JsonNode* parent = child->getParentProperty();
    ASSERT_EQ(parent, nullptr);
    EXPECT_EQ(child->GetValueKind(), JsonValueKind::String);
}

// --- Nested trees (J04) ------------------------------------------------------------------

// Probe case J04: a three-level tree released at the root. The detach cascades structurally -
// releasing the root's children runs their destructors, which detach theirs - so no recursive
// walk is written anywhere.
TEST(JsonNodeLifetimeTests, RetainedGrandchild_AfterWholeTreeDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> leaf;
    std::shared_ptr<JsonNode> mid;
    {
        auto root = std::make_shared<JsonObject>();
        auto inner = std::make_shared<JsonArray>();
        inner->Add(str("leaf"));
        leaf = (*inner)[0];
        mid = inner;
        root->Add("inner", inner);
        ASSERT_EQ(leaf->getRootProperty(), root.get());
    }
    EXPECT_EQ(mid->getParentProperty(), nullptr);
    EXPECT_EQ(leaf->getParentProperty(), mid.get());
    EXPECT_EQ(leaf->getRootProperty(), mid.get());
}

// The same tree, but only the deepest node is retained: every intermediate container dies, and
// the leaf must still end up parentless rather than pointing at the freed middle container.
TEST(JsonNodeLifetimeTests, RetainedLeafOnly_AfterWholeTreeDestroyed_HasNoParent) {
    std::shared_ptr<JsonNode> leaf;
    {
        auto root = std::make_shared<JsonArray>();
        auto mid = std::make_shared<JsonObject>();
        auto inner = std::make_shared<JsonArray>();
        inner->Add(str("leaf"));
        leaf = (*inner)[0];
        mid->Add("inner", inner);
        root->Add(mid);
        ASSERT_EQ(leaf->getRootProperty(), root.get());
    }
    EXPECT_EQ(leaf->getParentProperty(), nullptr);
    EXPECT_EQ(leaf->getRootProperty(), leaf.get());
}

// --- The three defects #1888 closed (J08, J09, J13) ---------------------------------------
//
// These three cases used to PIN the defects, each with a `NOLINT - deliberate` marker saying so.
// #1888 landed on 2026-08-19 and they are INVERTED: what they assert now is that the spelling is
// gone, and that the lifetime behaviour they were really protecting still holds by other means.

TEST(JsonNodeLifetimeTests, Fix1888_ANodeCannotBePutIntoTwoContainers) {
    // WAS: ChildOwnedByAnotherContainer_KeepsItsParentWhenTheFormerOwnerDies (probe case J13).
    // DetachParent() was PUBLIC, so a caller could clear a node's parent link and hand the same
    // node to a second container -- leaving one container holding a node whose parent named
    // another. The old case pinned the destructor's behaviour in that state.
    //
    // The state is now unreachable: DetachParent is protected, with JsonArray and JsonObject as
    // friends. That is the reachability .NET has -- its DetachParent is a PRIVATE helper on each
    // container (JsonObject.cs:316, JsonArray.IList.cs:231) whose body is `item?.Parent = null`,
    // and `Parent`'s setter is internal. There is no DetachParent on JsonNode.cs at all.
    static_assert(!detail1888::HasPublicDetachParent<JsonArray>,
                  "#1888: DetachParent must not be publicly callable");

    // What the old case really protected -- a container releasing only its OWN children -- still
    // holds, and is reached the supported way: Remove detaches, and the former owner's death
    // leaves the surviving container's link alone.
    JsonArray live;
    std::shared_ptr<JsonNode> child;
    {
        JsonArray stale;
        stale.Add(str("v"));
        child = stale[0];
        stale.RemoveAt(0);                       // the supported detach
        ASSERT_EQ(child->getParentProperty(), nullptr);
        live.Add(child);
        ASSERT_EQ(child->getParentProperty(), &live);
    }
    EXPECT_EQ(child->getParentProperty(), &live);
    EXPECT_EQ(child->getRootProperty(), &live);
}

TEST(JsonNodeLifetimeTests, Fix1888_CopyAndAssignmentAreGoneAndDeepCloneReplacesThem) {
    // WAS: CopyConstructedArrayDestroyed_LeavesTheOriginalsParentLinkIntact (J08) and
    // OriginalDestroyedBeforeItsCopy_DetachesOnceAndTheCopyIsHarmless (J09). Both carried a
    // `NOLINT - deliberate: pins today's implicit copy` marker.
    //
    // A .NET JsonNode is a REFERENCE type, so there was never an object copy to translate --
    // these four members were a C++ artefact, and both were wrong for a parented node: the copy
    // shared the original's children, each still reporting the ORIGINAL as its parent, and
    // assignment SLICED, rewriting parent_ on a node still stored in a container.
    static_assert(!std::is_copy_constructible_v<JsonArray>);
    static_assert(!std::is_copy_assignable_v<JsonArray>);
    static_assert(!std::is_move_constructible_v<JsonArray>);
    static_assert(!std::is_move_assignable_v<JsonArray>);
    static_assert(!std::is_copy_constructible_v<JsonObject>);
    static_assert(!std::is_copy_constructible_v<JsonNode>);

    // XObject already deleted all four, so this ended an asymmetry inside the port. That is NOT
    // asserted here: modules/text-json does not depend on modules/xml-linq, and adding the edge
    // for a test convenience is what the module-boundary rule exists to stop -- the same rule that
    // blocked #1997's A-2. XLinqLifetimeTests owns the XObject half.

    // DeepClone is the replacement, and it does what the implicit copy did NOT: the clone's
    // children are its own, and they name the CLONE as their parent.
    JsonArray original;
    original.Add(str("v"));
    auto child = original[0];
    const auto clone = original.DeepClone();
    auto* asArray = dynamic_cast<JsonArray*>(clone.get());
    ASSERT_NE(asArray, nullptr);
    ASSERT_EQ(asArray->getCountProperty(), 1);
    EXPECT_NE((*asArray)[0].get(), child.get()) << "a deep clone shares no child with the original";
    EXPECT_EQ((*asArray)[0]->getParentProperty(), asArray)
        << "and the clone's children name the CLONE -- which is exactly what the implicit copy "
           "got wrong";
    EXPECT_EQ(child->getParentProperty(), &original) << "the original is untouched";
}

// --- Structural mutation before destruction ----------------------------------------------

TEST(JsonNodeLifetimeTests, ChildRemovedBeforeOwnerDestroyed_StaysDetached) {
    std::shared_ptr<JsonNode> child;
    {
        JsonArray arr;
        arr.Add(str("v"));
        child = arr[0];
        arr.RemoveAt(0);
        EXPECT_EQ(child->getParentProperty(), nullptr);
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
}

TEST(JsonNodeLifetimeTests, ChildReplacedBeforeOwnerDestroyed_StaysDetachedAndTheNewOneDetaches) {
    std::shared_ptr<JsonNode> replaced;
    std::shared_ptr<JsonNode> current;
    {
        JsonArray arr;
        arr.Add(str("old"));
        replaced = arr[0];
        arr.SetItem(0, str("new"));
        current = arr[0];
        EXPECT_EQ(replaced->getParentProperty(), nullptr);
        EXPECT_EQ(current->getParentProperty(), &arr);
    }
    EXPECT_EQ(replaced->getParentProperty(), nullptr);
    EXPECT_EQ(current->getParentProperty(), nullptr);
}

TEST(JsonNodeLifetimeTests, ObjectValueReplacedBeforeOwnerDestroyed_BothEndDetached) {
    std::shared_ptr<JsonNode> replaced;
    std::shared_ptr<JsonNode> current;
    {
        JsonObject obj;
        obj.Add("k", str("old"));
        replaced = obj["k"];
        obj.SetItem("k", str("new"));
        current = obj["k"];
    }
    EXPECT_EQ(replaced->getParentProperty(), nullptr);
    EXPECT_EQ(current->getParentProperty(), nullptr);
}

// Moving a child between owners is supported only as remove-then-add, because AssignParent
// rejects an already-parented node (matching .NET). The old owner's destructor must not reach
// into the new owner's child.
TEST(JsonNodeLifetimeTests, ChildMovedToAnotherOwner_KeepsTheNewParentAfterTheOldOwnerDies) {
    JsonObject destination;
    std::shared_ptr<JsonNode> child;
    {
        JsonArray source;
        source.Add(str("v"));
        child = source[0];
        source.RemoveAt(0);
        destination.Add("k", child);
        ASSERT_EQ(child->getParentProperty(), &destination);
    }
    EXPECT_EQ(child->getParentProperty(), &destination);
}

TEST(JsonNodeLifetimeTests, ClearedOwnerDestroyed_RetainedChildRemainsDetached) {
    std::shared_ptr<JsonNode> child;
    {
        JsonObject obj;
        obj.Add("k", str("v"));
        child = obj["k"];
        obj.Clear();
        EXPECT_EQ(child->getParentProperty(), nullptr);
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
}

// --- Degenerate owners --------------------------------------------------------------------

TEST(JsonNodeLifetimeTests, EmptyOwnersDestroyCleanly) {
    { JsonArray arr; EXPECT_EQ(arr.getCountProperty(), 0); }
    { JsonObject obj; EXPECT_EQ(obj.getCountProperty(), 0); }
    { auto arr = std::make_shared<JsonArray>(); EXPECT_EQ(arr->getCountProperty(), 0); }
    { auto obj = std::make_shared<JsonObject>(); EXPECT_EQ(obj->getCountProperty(), 0); }
    SUCCEED();
}

// JSON null is a null shared_ptr slot, not a node, so the detach loop must tolerate holes.
TEST(JsonNodeLifetimeTests, NullSlotsAreSkipped_AndRealChildrenStillDetach) {
    std::vector<std::shared_ptr<JsonNode>> retained;
    {
        JsonArray arr;
        arr.Add(nullptr);
        arr.Add(str("a"));
        arr.Add(nullptr);
        arr.Add(str("b"));
        retained.push_back(arr[1]);
        retained.push_back(arr[3]);
        ASSERT_EQ(arr.getCountProperty(), 4);
    }
    for (const auto& node : retained) EXPECT_EQ(node->getParentProperty(), nullptr);
}

TEST(JsonNodeLifetimeTests, EveryChildOfAMultiChildOwnerIsDetached) {
    std::vector<std::shared_ptr<JsonNode>> retained;
    {
        JsonArray arr;
        for (int i = 0; i < 8; ++i) arr.Add(str("v" + std::to_string(i)));
        for (SharpRuntime::intcs i = 0; i < arr.getCountProperty(); ++i) retained.push_back(arr[i]);
    }
    ASSERT_EQ(retained.size(), 8u);
    for (const auto& node : retained) {
        EXPECT_EQ(node->getParentProperty(), nullptr);
        EXPECT_EQ(node->getRootProperty(), node.get());
    }
}

TEST(JsonNodeLifetimeTests, EveryValueOfAMultiPropertyOwnerIsDetached) {
    std::vector<std::shared_ptr<JsonNode>> retained;
    {
        JsonObject obj;
        for (int i = 0; i < 8; ++i) obj.Add("k" + std::to_string(i), str("v"));
        for (int i = 0; i < 8; ++i) retained.push_back(obj["k" + std::to_string(i)]);
    }
    ASSERT_EQ(retained.size(), 8u);
    for (const auto& node : retained) EXPECT_EQ(node->getParentProperty(), nullptr);
}

// --- Throwing paths -----------------------------------------------------------------------

// A failed insertion must leave the child attached to whoever legitimately owns it, and the
// owner that rejected it must not detach it when it dies.
TEST(JsonNodeLifetimeTests, RejectedInsertion_LeavesTheChildWithItsRealOwnerAfterTheRejecterDies) {
    JsonArray owner;
    owner.Add(str("v"));
    auto child = owner[0];
    {
        JsonArray rejecter;
        EXPECT_THROW(rejecter.Add(child), System::InvalidOperationException);
        EXPECT_EQ(rejecter.getCountProperty(), 0);
    }
    EXPECT_EQ(child->getParentProperty(), &owner);
}

// JsonArray::SetItem assigns the new parent before detaching the old value (plan section 9.1),
// so a rejected replacement leaves the stored value's link untouched - and the destructor then
// clears exactly that link.
TEST(JsonNodeLifetimeTests, FailedArraySetItem_LeavesTheStoredValueOwnedAndItDetachesOnDestruction) {
    JsonArray other;
    other.Add(str("attached"));
    auto attached = other[0];

    std::shared_ptr<JsonNode> stored;
    {
        JsonArray arr;
        arr.Add(str("stored"));
        stored = arr[0];
        EXPECT_THROW(arr.SetItem(0, attached), System::InvalidOperationException);
        EXPECT_EQ(stored->getParentProperty(), &arr);
        EXPECT_EQ(arr[0], stored);
    }
    EXPECT_EQ(stored->getParentProperty(), nullptr);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

TEST(JsonNodeLifetimeTests, FailedObjectAdd_DuplicateKey_LeavesTheStoredValueOwnedThenDetached) {
    std::shared_ptr<JsonNode> stored;
    {
        JsonObject obj;
        obj.Add("k", str("v"));
        stored = obj["k"];
        EXPECT_THROW(obj.Add("k", str("other")), System::ArgumentException);
        EXPECT_EQ(obj.getCountProperty(), 1);
        EXPECT_EQ(stored->getParentProperty(), &obj);
    }
    EXPECT_EQ(stored->getParentProperty(), nullptr);
}

TEST(JsonNodeLifetimeTests, OutOfRangeSetItem_DoesNotDisturbTheDetachContract) {
    std::shared_ptr<JsonNode> stored;
    {
        JsonArray arr;
        arr.Add(str("v"));
        stored = arr[0];
        EXPECT_THROW(arr.SetItem(5, str("x")), System::ArgumentOutOfRangeException);
    }
    EXPECT_EQ(stored->getParentProperty(), nullptr);
}

// The destructor must be non-throwing in practice: it runs during stack unwinding, where a
// throw would call std::terminate.
TEST(JsonNodeLifetimeTests, OwnerDestroyedDuringExceptionUnwinding_DetachesWithoutTerminating) {
    std::shared_ptr<JsonNode> child;
    bool caught = false;
    try {
        JsonArray arr;
        arr.Add(str("v"));
        child = arr[0];
        throw System::InvalidOperationException("unwind");
    } catch (const System::InvalidOperationException&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->getParentProperty(), nullptr);
}

TEST(JsonNodeLifetimeTests, NestedOwnersDestroyedDuringExceptionUnwinding_DetachEveryLevel) {
    std::shared_ptr<JsonNode> leaf;
    try {
        JsonObject outer;
        auto inner = std::make_shared<JsonArray>();
        inner->Add(str("leaf"));
        leaf = (*inner)[0];
        outer.Add("inner", inner);
        throw System::InvalidOperationException("unwind");
    } catch (const System::InvalidOperationException&) {
    }
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->getParentProperty(), nullptr);
}

// --- Re-attachment (plan section 13.3 row 3) ----------------------------------------------

// Before #1886 this threw "The node already has a parent." forever: the stale non-null parent_
// made a retained child permanently unusable, which is the half of SR-AUD-327 that produced a
// wrong answer with no diagnostic at all.
TEST(JsonNodeLifetimeTests, RetainedChild_IsReAttachableAfterItsOwnerIsDestroyed) {
    std::shared_ptr<JsonNode> child;
    {
        JsonArray arr;
        arr.Add(str("v"));
        child = arr[0];
    }
    JsonArray fresh;
    EXPECT_NO_THROW(fresh.Add(child));
    EXPECT_EQ(fresh.getCountProperty(), 1);
    EXPECT_EQ(child->getParentProperty(), &fresh);
    EXPECT_EQ(fresh.ToJsonString(), "[\"v\"]");
}

TEST(JsonNodeLifetimeTests, RetainedObjectValue_IsReAttachableAfterItsOwnerIsDestroyed) {
    std::shared_ptr<JsonNode> value;
    {
        JsonObject obj;
        obj.Add("k", str("v"));
        value = obj["k"];
    }
    JsonObject fresh;
    EXPECT_NO_THROW(fresh.Add("k2", value));
    EXPECT_EQ(value->getParentProperty(), &fresh);
    EXPECT_EQ(fresh.ToJsonString(), "{\"k2\":\"v\"}");
}

// A retained sub-container must be re-attachable too, and its own children must survive the
// move with their links intact.
TEST(JsonNodeLifetimeTests, RetainedSubtree_IsReAttachableWithItsOwnChildrenIntact) {
    std::shared_ptr<JsonNode> subtree;
    {
        JsonObject root;
        auto inner = std::make_shared<JsonArray>();
        inner->Add(str("a"));
        inner->Add(str("b"));
        root.Add("inner", inner);
        subtree = inner;
    }
    ASSERT_EQ(subtree->getParentProperty(), nullptr);
    EXPECT_EQ(subtree->ToJsonString(), "[\"a\",\"b\"]");

    JsonArray fresh;
    EXPECT_NO_THROW(fresh.Add(subtree));
    EXPECT_EQ(fresh.ToJsonString(), "[[\"a\",\"b\"]]");
    EXPECT_EQ(subtree->getRootProperty(), &fresh);
}

// --- Allocator reuse (J16) ----------------------------------------------------------------

// Probe case J16 is the shape a sanitizer's quarantine hides: the freed owner's storage is
// reused by an unrelated node at the same address, and the retained child then reported that
// squatter as its parent with the wrong GetValueKind. A null parent link cannot name a
// squatter, whether or not the allocator happens to reuse the block on this run.
TEST(JsonNodeLifetimeTests, RetainedChild_ReportsNoParentEvenIfTheFreedOwnersStorageIsReused) {
    std::shared_ptr<JsonNode> child;
    {
        auto arr = std::make_shared<JsonArray>();
        arr->Add(str("v"));
        child = (*arr)[0];
    }
    std::vector<std::shared_ptr<JsonObject>> squatters;
    for (int i = 0; i < 16; ++i) {
        squatters.push_back(std::make_shared<JsonObject>());
        squatters.back()->Add("squatter", str("s"));
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(child->getRootProperty(), child.get());
    EXPECT_EQ(child->GetValueKind(), JsonValueKind::String);
}

// --- Representation invariants ------------------------------------------------------------

// #1886's repair was layout-identical by construction, and these assertions fail the BUILD rather
// than a test run if a member appears. They did exactly that when #1889 landed on 2026-08-19, which
// is the evidence they are load-bearing rather than decorative -- so the two container figures are
// UPDATED here, and the two that did not move are left as #1886 wrote them.
//
// #1889 added one System::Collections::detail::MutationCounter (8 bytes) to each CONTAINER, for
// fail-fast enumeration. JsonNode and JsonValue are untouched, which is what shows the counter
// went where the enumerators are and nowhere else.
static_assert(sizeof(JsonNode) == 24, "#1886: JsonNode must stay 24 bytes");
static_assert(sizeof(JsonArray) == 56, "#1886/#1889: JsonArray is 56 bytes (was 48)");
static_assert(sizeof(JsonObject) == 56, "#1886/#1889: JsonObject is 56 bytes (was 48)");
static_assert(sizeof(JsonValue) == 40, "#1886: JsonValue must stay 40 bytes");

// The growth is exactly one counter on each, asserted as a RELATIONSHIP so a second member added
// later cannot hide behind a literal somebody updated by hand.
static_assert(sizeof(JsonArray) == 48 + sizeof(System::Collections::detail::MutationVersion));
static_assert(sizeof(JsonObject) == 48 + sizeof(System::Collections::detail::MutationVersion));
static_assert(alignof(JsonArray) == 8 && alignof(JsonObject) == 8, "#1886: alignment unchanged");
static_assert(std::has_virtual_destructor_v<JsonNode>, "#1886: destruction stays virtual");
static_assert(std::is_polymorphic_v<JsonArray> && std::is_polymorphic_v<JsonObject>,
              "#1886: the added destructors must not change polymorphism");
static_assert(std::is_nothrow_destructible_v<JsonArray>, "#1886: ~JsonArray must be noexcept");
static_assert(std::is_nothrow_destructible_v<JsonObject>, "#1886: ~JsonObject must be noexcept");

TEST(JsonNodeLifetimeTests, PublicLayoutIsUnchangedByTheDetachContract) {
    EXPECT_EQ(sizeof(JsonNode), 24u);
    // #1889 added one MutationCounter to each container; the detach contract itself still
    // costs no layout, which is what this case is about, so it is written relative to that.
    EXPECT_EQ(sizeof(JsonArray), 48u + sizeof(System::Collections::detail::MutationVersion));
    EXPECT_EQ(sizeof(JsonObject), 48u + sizeof(System::Collections::detail::MutationVersion));
    EXPECT_EQ(sizeof(JsonValue), 40u);
}

// =================================================================================================
// #1889 -- fail-fast enumeration. The two defects it closes were MEASURED, not theorised.
// =================================================================================================

TEST(JsonNodeLifetimeTests, Fix1889_AnIteratorHeldAcrossAReallocatingAddIsRefusedNotDangling) {
    // Probe case J11. begin()/end() used to hand out RAW std::vector iterators, so an iterator
    // held across an Add that reallocated the buffer was an ASan-confirmed heap-use-after-free --
    // a SIGSEGV in a build with no sanitizer. It is now an InvalidOperationException.
    JsonArray arr;
    arr.Add(str("a"));
    auto it = arr.begin();

    // Enough insertions to force at least one reallocation whatever the initial capacity.
    for (int i = 0; i < 64; ++i) arr.Add(str("x"));

    EXPECT_THROW((void)*it, System::InvalidOperationException)
        << "#1889: a stale dereference must be refused, not read from freed storage";
    EXPECT_THROW((void)++it, System::InvalidOperationException)
        << "advancing a stale enumerator is refused too -- not just dereferencing it";
}

TEST(JsonNodeLifetimeTests, Fix1889_AnIteratorHeldAcrossClearIsRefusedRatherThanSilentlyWrong) {
    // Probe case J12, and the worse of the two: Clear() does not reallocate, so the old iterator
    // read DESTROYED STORAGE and returned a plausible value WITH NO DIAGNOSTIC IN ANY BUILD --
    // not even under a sanitizer, in the general case.
    JsonArray arr;
    arr.Add(str("a"));
    arr.Add(str("b"));
    auto it = arr.begin();
    arr.Clear();
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    JsonObject obj;
    obj.Add("k", str("v"));
    auto oit = obj.begin();
    obj.Clear();
    EXPECT_THROW((void)*oit, System::InvalidOperationException);
}

TEST(JsonNodeLifetimeTests, Fix1889_EveryMutatingDoorInvalidatesAndNonMutatingReadsDoNot) {
    // A counter is only useful if EVERY door bumps it, and only usable if no read does. Both
    // halves are asserted, because a counter that over-fires is as wrong as one that under-fires.
    const auto stale = [](auto& container, auto&& mutate) {
        auto it = container.begin();
        mutate();
        return [it]() mutable { (void)*it; };
    };

    {   // Add
        JsonArray a; a.Add(str("x"));
        auto probe = stale(a, [&]{ a.Add(str("y")); });
        EXPECT_THROW(probe(), System::InvalidOperationException) << "Add";
    }
    {   // Insert
        JsonArray a; a.Add(str("x"));
        auto probe = stale(a, [&]{ a.Insert(0, str("y")); });
        EXPECT_THROW(probe(), System::InvalidOperationException) << "Insert";
    }
    {   // RemoveAt
        JsonArray a; a.Add(str("x")); a.Add(str("y"));
        auto probe = stale(a, [&]{ a.RemoveAt(1); });
        EXPECT_THROW(probe(), System::InvalidOperationException) << "RemoveAt";
    }
    {   // SetItem -- the door easiest to forget, because the element COUNT does not change
        JsonArray a; a.Add(str("x"));
        auto probe = stale(a, [&]{ a.SetItem(0, str("z")); });
        EXPECT_THROW(probe(), System::InvalidOperationException)
            << "SetItem: the count is unchanged, so only the counter can notice";
    }
    {   // JsonObject::Add and Remove
        JsonObject o; o.Add("k", str("v"));
        auto probe = stale(o, [&]{ o.Add("k2", str("v2")); });
        EXPECT_THROW(probe(), System::InvalidOperationException) << "JsonObject::Add";
    }
    {
        JsonObject o; o.Add("k", str("v")); o.Add("k2", str("v2"));
        auto probe = stale(o, [&]{ o.Remove("k2"); });
        EXPECT_THROW(probe(), System::InvalidOperationException) << "JsonObject::Remove";
    }
    {   // JsonObject::SetItem replacement -- count and key order are unchanged.
        JsonObject o; o.Add("k", str("v"));
        auto probe = stale(o, [&]{ o.SetItem("k", str("replacement")); });
        EXPECT_THROW(probe(), System::InvalidOperationException)
            << "JsonObject::SetItem replacement must invalidate a stale enumerator";
    }
    {   // Assigning the identical node is the documented no-op and must keep iterators current.
        auto value = str("v");
        JsonObject o; o.Add("k", value);
        auto it = o.begin();
        o.SetItem("k", value);
        EXPECT_NO_THROW((void)*it)
            << "JsonObject::SetItem of the same shared_ptr must remain a no-op";
    }

    // ...and a READ must not invalidate. If it did, no caller could iterate at all.
    {
        JsonArray a; a.Add(str("x")); a.Add(str("y"));
        auto it = a.begin();
        (void)a.getCountProperty();
        (void)a[0];
        (void)a.IndexOf(a[1]);
        EXPECT_NO_THROW((void)*it) << "#1889: reads must not bump the counter";
    }

    // AND begin()/end() THEMSELVES MUST NOT BUMP IT. A first cut of this case missed that: a
    // mutation making begin() bump went UNCAUGHT, because a single range-for still works -- the
    // one enumerator snapshots the version begin() just produced. It is observable only with TWO
    // enumerators over the same unmutated container, which is a legitimate thing to hold.
    {
        JsonArray a; a.Add(str("x")); a.Add(str("y"));
        auto first = a.begin();
        auto second = a.begin();
        const auto stop = a.end();
        EXPECT_NO_THROW((void)*first)
            << "#1889: taking a second enumerator must not invalidate the first";
        EXPECT_NO_THROW((void)*second);
        EXPECT_NE(first, stop);
    }
    {
        JsonObject o; o.Add("k", str("v"));
        auto first = o.begin();
        auto second = o.begin();
        EXPECT_NO_THROW((void)*first);
        EXPECT_NO_THROW((void)*second);
    }
}

TEST(JsonNodeLifetimeTests, Fix1889_AFullIterationOverAnUnmutatedContainerStillWorks) {
    // The guard must not break ordinary use -- including the `it->` form, which needs operator->
    // and which a first measurement of this ticket wrongly reported as having no callers.
    JsonArray arr;
    for (int i = 0; i < 5; ++i) arr.Add(str("v"));
    int seen = 0;
    for (const auto& item : arr) { EXPECT_NE(item, nullptr); ++seen; }
    EXPECT_EQ(seen, 5);

    JsonObject obj;
    obj.Add("a", str("1"));
    obj.Add("b", str("2"));
    std::string keys;
    for (auto it = obj.begin(); it != obj.end(); ++it) keys += it->first;
    EXPECT_EQ(keys, "ab") << "the `it->` form must work -- JsonNodeParseDepthTests uses it";
}
