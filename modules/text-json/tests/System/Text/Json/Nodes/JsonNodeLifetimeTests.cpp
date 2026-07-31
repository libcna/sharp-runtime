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

// --- Only the destroying owner's own links are cleared (J08, J13) -------------------------

// Probe case J13: DetachParent() is public today, so a node can legitimately end up stored in
// one container while its parent link names another. The destructor of the container that no
// longer owns it must leave that link alone.
TEST(JsonNodeLifetimeTests, ChildOwnedByAnotherContainer_KeepsItsParentWhenTheFormerOwnerDies) {
    JsonArray live;
    std::shared_ptr<JsonNode> child;
    {
        JsonArray stale;
        stale.Add(str("v"));
        child = stale[0];
        child->DetachParent();
        live.Add(child);
        ASSERT_EQ(child->getParentProperty(), &live);
    }
    EXPECT_EQ(child->getParentProperty(), &live);
    EXPECT_EQ(child->getRootProperty(), &live);
}

// Probe case J08: JsonNode's copy operations are still implicitly generated (ticket #1888, not
// approved), so a copy-constructed array shares the original's children, which keep reporting
// the original as their parent. Destroying the copy must not steal that link.
TEST(JsonNodeLifetimeTests, CopyConstructedArrayDestroyed_LeavesTheOriginalsParentLinkIntact) {
    JsonArray original;
    original.Add(str("v"));
    auto child = original[0];
    {
        JsonArray aliasing = original;  // NOLINT - deliberate: pins today's implicit copy
        ASSERT_EQ(aliasing.getCountProperty(), 1);
        ASSERT_EQ(child->getParentProperty(), &original);
    }
    EXPECT_EQ(child->getParentProperty(), &original);
    EXPECT_EQ(child->getRootProperty(), &original);
}

// The reverse order: the original dies first (detaching the child), then the aliasing copy
// dies. The copy must find a link that no longer names it and do nothing - in particular it
// must not re-clear, re-read, or otherwise touch an already detached child.
TEST(JsonNodeLifetimeTests, OriginalDestroyedBeforeItsCopy_DetachesOnceAndTheCopyIsHarmless) {
    std::shared_ptr<JsonNode> child;
    {
        JsonArray aliasing;
        {
            JsonArray original;
            original.Add(str("v"));
            child = original[0];
            aliasing = original;  // NOLINT - deliberate: pins today's implicit copy-assign
        }
        EXPECT_EQ(child->getParentProperty(), nullptr);
    }
    EXPECT_EQ(child->getParentProperty(), nullptr);
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

// The repair is layout-identical by construction; these assertions fail the build rather than a
// test run if a future change adds a member (plan section 20 and ticket #1886's acceptance
// criteria).
static_assert(sizeof(JsonNode) == 24, "#1886: JsonNode must stay 24 bytes");
static_assert(sizeof(JsonArray) == 48, "#1886: JsonArray must stay 48 bytes");
static_assert(sizeof(JsonObject) == 48, "#1886: JsonObject must stay 48 bytes");
static_assert(sizeof(JsonValue) == 40, "#1886: JsonValue must stay 40 bytes");
static_assert(alignof(JsonArray) == 8 && alignof(JsonObject) == 8, "#1886: alignment unchanged");
static_assert(std::has_virtual_destructor_v<JsonNode>, "#1886: destruction stays virtual");
static_assert(std::is_polymorphic_v<JsonArray> && std::is_polymorphic_v<JsonObject>,
              "#1886: the added destructors must not change polymorphism");
static_assert(std::is_nothrow_destructible_v<JsonArray>, "#1886: ~JsonArray must be noexcept");
static_assert(std::is_nothrow_destructible_v<JsonObject>, "#1886: ~JsonObject must be noexcept");

TEST(JsonNodeLifetimeTests, PublicLayoutIsUnchangedByTheDetachContract) {
    EXPECT_EQ(sizeof(JsonNode), 24u);
    EXPECT_EQ(sizeof(JsonArray), 48u);
    EXPECT_EQ(sizeof(JsonObject), 48u);
    EXPECT_EQ(sizeof(JsonValue), 40u);
}
