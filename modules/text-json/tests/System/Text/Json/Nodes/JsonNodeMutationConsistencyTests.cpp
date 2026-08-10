// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1887 (SR-AUD-327, CCF-019 section 31 item 2): every
// mutating entry point of JsonArray and JsonObject either completes or leaves the container
// exactly as it found it. The defect this pins closed is probe case J10:
// JsonObject::SetItem detached the value it was about to replace *before* AssignParent could
// reject the incoming value, so a rejected call left the object still holding a value that
// reported no parent - and any other container then accepted that value, giving one node two
// owners, each of which would clear the other's link on destruction.
//
// The contract is documented in docs/OwnedTreeLifetimeContractPlan.md sections 13.5 (J-2), 25
// (#1887) and 35. The probe case identifiers (J10 for the defect, J18 for the JsonArray
// contrast that was always correct) are quoted so the two records stay linked.
//
// These tests deliberately assert the *whole* container state after a rejection - count, slot
// identity, the stored value's parent link, and whether a second container accepts it - because
// the pre-fix defect was invisible in every one of those except the last two.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

namespace {

    std::shared_ptr<JsonNode> str(const std::string& s) { return JsonValue::Create(s); }

    // A node that already belongs to `owner`, i.e. one every AssignParent must reject.
    std::shared_ptr<JsonNode> attachedTo(JsonArray& owner, const std::string& s) {
        owner.Add(str(s));
        return owner[owner.getCountProperty() - 1];
    }

} // namespace

// --- J10: the rejected JsonObject::SetItem ----------------------------------------------

// The headline case. Pre-fix: `stored` came out of this with a null parent while still sitting
// in `obj`, and `thief` accepted it. Post-fix: nothing moved at all.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_LeavesTheReplacedValueOwned) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonObject obj;
    obj.Add("k", str("stored"));
    auto stored = obj["k"];

    EXPECT_THROW(obj.SetItem("k", attached), System::InvalidOperationException);

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(obj["k"], stored);
    EXPECT_EQ(stored->getParentProperty(), &obj);
    EXPECT_EQ(stored->getRootProperty(), &obj);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

// The half of J10 that made it a double-ownership defect rather than a cosmetic one: a second
// container must now refuse the value the rejected call left in place.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_TheReplacedValueIsStillRefusedElsewhere) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonObject obj;
    obj.Add("k", str("stored"));
    auto stored = obj["k"];
    EXPECT_THROW(obj.SetItem("k", attached), System::InvalidOperationException);

    JsonArray thief;
    EXPECT_THROW(thief.Add(stored), System::InvalidOperationException);
    EXPECT_EQ(thief.getCountProperty(), 0);
    EXPECT_EQ(stored->getParentProperty(), &obj);
    EXPECT_EQ(obj.getCountProperty(), 1);
}

// The same rejection through the other door AssignParent has: a cycle.
TEST(JsonNodeMutationConsistencyTests, CycleRejectedObjectSetItem_LeavesTheReplacedValueOwned) {
    auto root = std::make_shared<JsonObject>();
    auto mid = std::make_shared<JsonObject>();
    root->Add("mid", mid);
    mid->Add("k", str("stored"));
    auto stored = (*mid)["k"];

    // Adopting `root` into its own descendant would close a cycle.
    EXPECT_THROW(mid->SetItem("k", root), System::InvalidOperationException);

    EXPECT_EQ(mid->getCountProperty(), 1);
    EXPECT_EQ((*mid)["k"], stored);
    EXPECT_EQ(stored->getParentProperty(), mid.get());
    EXPECT_EQ(root->getParentProperty(), nullptr);
}

// Rejecting a *new* key must not add the key either.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_NewKey_AddsNothing) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonObject obj;
    obj.Add("k", str("stored"));

    EXPECT_THROW(obj.SetItem("fresh", attached), System::InvalidOperationException);

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_FALSE(obj.ContainsKey("fresh"));
    EXPECT_EQ(obj["fresh"], nullptr);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

// Setting a key to a value that already belongs to *this same object under another key* is
// rejected too, and neither key moves.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_ValueOwnedBySameObject_MovesNothing) {
    JsonObject obj;
    obj.Add("a", str("a"));
    obj.Add("b", str("b"));
    auto a = obj["a"];
    auto b = obj["b"];

    EXPECT_THROW(obj.SetItem("b", a), System::InvalidOperationException);

    EXPECT_EQ(obj.getCountProperty(), 2);
    EXPECT_EQ(obj["a"], a);
    EXPECT_EQ(obj["b"], b);
    EXPECT_EQ(a->getParentProperty(), &obj);
    EXPECT_EQ(b->getParentProperty(), &obj);
}

// --- The success paths the reorder must not disturb --------------------------------------

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_ReplacingAValue_DetachesTheOldAndAdoptsTheNew) {
    JsonObject obj;
    obj.Add("k", str("old"));
    auto old = obj["k"];
    auto fresh = str("new");

    obj.SetItem("k", fresh);

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(obj["k"], fresh);
    EXPECT_EQ(fresh->getParentProperty(), &obj);
    EXPECT_EQ(old->getParentProperty(), nullptr);
    EXPECT_EQ(old->getRootProperty(), old.get());
}

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_ReplacedValueIsReAttachableElsewhere) {
    JsonObject obj;
    obj.Add("k", str("old"));
    auto old = obj["k"];
    obj.SetItem("k", str("new"));

    JsonArray newOwner;
    newOwner.Add(old);
    EXPECT_EQ(old->getParentProperty(), &newOwner);
    EXPECT_EQ(newOwner.getCountProperty(), 1);
}

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_SameNodeSameKey_IsANoOp) {
    JsonObject obj;
    obj.Add("k", str("v"));
    auto stored = obj["k"];

    obj.SetItem("k", stored); // must not throw: the early identity check comes first

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(obj["k"], stored);
    EXPECT_EQ(stored->getParentProperty(), &obj);
}

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_NewKey_AddsAndAdopts) {
    JsonObject obj;
    auto fresh = str("v");
    obj.SetItem("k", fresh);
    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(obj["k"], fresh);
    EXPECT_EQ(fresh->getParentProperty(), &obj);
}

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_NullValue_DetachesTheOldAndStoresJsonNull) {
    JsonObject obj;
    obj.Add("k", str("old"));
    auto old = obj["k"];

    obj.SetItem("k", nullptr);

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_TRUE(obj.ContainsKey("k"));
    EXPECT_EQ(obj["k"], nullptr);
    EXPECT_EQ(old->getParentProperty(), nullptr);
}

TEST(JsonNodeMutationConsistencyTests, ObjectSetItem_OverANullSlot_AdoptsWithoutTouchingAnythingElse) {
    JsonObject obj;
    obj.Add("k", nullptr);
    auto fresh = str("v");
    obj.SetItem("k", fresh);
    EXPECT_EQ(obj["k"], fresh);
    EXPECT_EQ(fresh->getParentProperty(), &obj);
    EXPECT_EQ(obj.getCountProperty(), 1);
}

// --- J18: the JsonArray contrast, which was always correct and must stay so ---------------

TEST(JsonNodeMutationConsistencyTests, RejectedArraySetItem_LeavesTheReplacedValueOwned) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonArray arr;
    arr.Add(str("stored"));
    auto stored = arr[0];

    EXPECT_THROW(arr.SetItem(0, attached), System::InvalidOperationException);

    EXPECT_EQ(arr.getCountProperty(), 1);
    EXPECT_EQ(arr[0], stored);
    EXPECT_EQ(stored->getParentProperty(), &arr);
    EXPECT_EQ(attached->getParentProperty(), &other);

    JsonArray thief;
    EXPECT_THROW(thief.Add(stored), System::InvalidOperationException);
    EXPECT_EQ(thief.getCountProperty(), 0);
}

TEST(JsonNodeMutationConsistencyTests, OutOfRangeArraySetItem_RejectsBeforeTouchingEitherNode) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonArray arr;
    arr.Add(str("stored"));
    auto stored = arr[0];

    EXPECT_THROW(arr.SetItem(5, attached), System::ArgumentOutOfRangeException);

    EXPECT_EQ(arr.getCountProperty(), 1);
    EXPECT_EQ(stored->getParentProperty(), &arr);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

// --- The remaining mutating entry points: none of them may partially mutate ---------------

TEST(JsonNodeMutationConsistencyTests, RejectedObjectAdd_DuplicateKey_AddsNothing) {
    JsonObject obj;
    obj.Add("k", str("v"));
    auto stored = obj["k"];
    auto fresh = str("other");

    EXPECT_THROW(obj.Add("k", fresh), System::ArgumentException);

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(obj["k"], stored);
    EXPECT_EQ(fresh->getParentProperty(), nullptr);
}

TEST(JsonNodeMutationConsistencyTests, RejectedObjectAdd_AlreadyParented_AddsNothing) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonObject obj;
    EXPECT_THROW(obj.Add("k", attached), System::InvalidOperationException);

    EXPECT_EQ(obj.getCountProperty(), 0);
    EXPECT_FALSE(obj.ContainsKey("k"));
    EXPECT_EQ(attached->getParentProperty(), &other);
}

TEST(JsonNodeMutationConsistencyTests, RejectedArrayAdd_AlreadyParented_AddsNothing) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonArray arr;
    EXPECT_THROW(arr.Add(attached), System::InvalidOperationException);

    EXPECT_EQ(arr.getCountProperty(), 0);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

TEST(JsonNodeMutationConsistencyTests, RejectedArrayInsert_AlreadyParented_InsertsNothing) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonArray arr;
    arr.Add(str("a"));
    EXPECT_THROW(arr.Insert(0, attached), System::InvalidOperationException);

    EXPECT_EQ(arr.getCountProperty(), 1);
    EXPECT_EQ(attached->getParentProperty(), &other);
}

TEST(JsonNodeMutationConsistencyTests, RejectedArrayInsert_OutOfRange_InsertsNothing) {
    JsonArray arr;
    arr.Add(str("a"));
    auto fresh = str("b");
    EXPECT_THROW(arr.Insert(7, fresh), System::ArgumentOutOfRangeException);
    EXPECT_EQ(arr.getCountProperty(), 1);
    EXPECT_EQ(fresh->getParentProperty(), nullptr);
}

TEST(JsonNodeMutationConsistencyTests, ObjectRemove_DetachesExactlyTheRemovedValue) {
    JsonObject obj;
    obj.Add("a", str("a"));
    obj.Add("b", str("b"));
    auto a = obj["a"];
    auto b = obj["b"];

    EXPECT_TRUE(obj.Remove("a"));

    EXPECT_EQ(obj.getCountProperty(), 1);
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(b->getParentProperty(), &obj);
    EXPECT_FALSE(obj.Remove("missing"));
    EXPECT_EQ(obj.getCountProperty(), 1);
}

TEST(JsonNodeMutationConsistencyTests, ObjectClear_DetachesEveryValue) {
    JsonObject obj;
    obj.Add("a", str("a"));
    obj.Add("b", nullptr);
    obj.Add("c", str("c"));
    auto a = obj["a"];
    auto c = obj["c"];

    obj.Clear();

    EXPECT_EQ(obj.getCountProperty(), 0);
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(c->getParentProperty(), nullptr);
    JsonArray newOwner;
    newOwner.Add(a);
    newOwner.Add(c);
    EXPECT_EQ(newOwner.getCountProperty(), 2);
}

// --- The reorder must compose with #1886's owner-side detachment --------------------------

// After a rejected SetItem the object still owns the value, so the #1886 destructor - which
// only clears a link that still names the destroying owner - is the thing that detaches it.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_ThenOwnerDestroyed_DetachesTheValue) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    std::shared_ptr<JsonNode> stored;
    {
        JsonObject obj;
        obj.Add("k", str("stored"));
        stored = obj["k"];
        EXPECT_THROW(obj.SetItem("k", attached), System::InvalidOperationException);
        EXPECT_EQ(stored->getParentProperty(), &obj);
    }
    EXPECT_EQ(stored->getParentProperty(), nullptr);
    EXPECT_EQ(stored->getRootProperty(), stored.get());

    JsonArray newOwner;
    newOwner.Add(stored);
    EXPECT_EQ(stored->getParentProperty(), &newOwner);
}

// Serialisation is the observable proof that a rejected call changed no content at all.
TEST(JsonNodeMutationConsistencyTests, RejectedObjectSetItem_LeavesTheSerialisedFormUnchanged) {
    JsonArray other;
    auto attached = attachedTo(other, "attached");

    JsonObject obj;
    obj.Add("k", str("stored"));
    const std::string before = obj.ToJsonString();

    EXPECT_THROW(obj.SetItem("k", attached), System::InvalidOperationException);

    EXPECT_EQ(obj.ToJsonString(), before);
    EXPECT_EQ(other.ToJsonString(), "[\"attached\"]");
}
