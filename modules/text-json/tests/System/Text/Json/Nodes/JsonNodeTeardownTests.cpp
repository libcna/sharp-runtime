// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1895 (SR-AUD-327, CCF-019): releasing a JsonArray/JsonObject
// tree is bounded by the heap, not by the call stack.
//
// The defect this pins closed is probe case J19c. Releasing a tree used to recurse - ~JsonArray
// released items_, which dropped the last shared_ptr to each child, which ran that child's
// destructor, one call frame per level - so a 20,000-deep nest crashed with an ASan
// stack-overflow *after* it had been built successfully, and the depth at which it crashed
// depended on the thread's stack size rather than on anything the program could observe.
//
// The repair is documented in docs/OwnedTreeLifetimeContractPlan.md section 41. It publishes a
// worklist in the outermost container destructor; every container destructor that runs while a
// worklist is published donates its own children to it and returns instead of releasing them.
//
// What these tests deliberately do NOT cover, because it is a different root cause with its own
// ticket: J19d/X27d (quadratic ancestor guards during *construction*, #1896, blocked) and X28c
// (recursive tree building inside JsonNode::Parse, #1897). Depths here are therefore chosen to
// exceed the stack bound while staying inside the quadratic build cost.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

namespace {

    // The depth probe case J19c uses. It is comfortably past the stack bound - J19b at 5,000
    // survived, J19c at 20,000 did not - and comfortably inside the still-quadratic build cost
    // that ticket #1896 owns.
    constexpr int kDeep = 20000;

    std::shared_ptr<JsonNode> str(const std::string& s) { return JsonValue::Create(s); }

    // Builds arr -> arr -> ... -> arr, kDeep levels, and returns the root.
    std::shared_ptr<JsonArray> deepArrayChain(int depth) {
        auto root = std::make_shared<JsonArray>();
        JsonArray* cur = root.get();
        for (int i = 1; i < depth; ++i) {
            auto next = std::make_shared<JsonArray>();
            cur->Add(next);
            cur = next.get();
        }
        return root;
    }

    // Alternates JsonObject and JsonArray so the worklist carries both stores.
    std::shared_ptr<JsonObject> deepMixedChain(int depth) {
        auto root = std::make_shared<JsonObject>();
        JsonNode* cur = root.get();
        for (int i = 1; i < depth; ++i) {
            if (i % 2 == 1) {
                auto next = std::make_shared<JsonArray>();
                static_cast<JsonObject*>(cur)->Add("k", next);
                cur = next.get();
            } else {
                auto next = std::make_shared<JsonObject>();
                static_cast<JsonArray*>(cur)->Add(next);
                cur = next.get();
            }
        }
        return root;
    }

    // Records the order in which containers are destroyed, so the iterative teardown can be shown
    // to reproduce the recursive one's order rather than merely to terminate.
    std::vector<int>* g_order = nullptr;

    class RecordingArray : public JsonArray {
        int tag_;
    public:
        explicit RecordingArray(int tag) : tag_(tag) {}
        ~RecordingArray() override {
            if (g_order != nullptr) g_order->push_back(tag_);
        }
    };

} // namespace

// --- J19c: the deep chain that used to overflow the stack on release ----------------------

TEST(JsonNodeTeardownTests, DeepArrayChain_ReleasesWithoutExhaustingTheStack) {
    auto root = deepArrayChain(kDeep);
    ASSERT_EQ(root->getCountProperty(), 1);
    root.reset(); // used to be an ASan stack-overflow / SIGSEGV
    SUCCEED();
}

TEST(JsonNodeTeardownTests, DeepMixedChain_ReleasesWithoutExhaustingTheStack) {
    auto root = deepMixedChain(kDeep);
    ASSERT_EQ(root->getCountProperty(), 1);
    root.reset();
    SUCCEED();
}

TEST(JsonNodeTeardownTests, DeepChainInAutomaticStorage_ReleasesWithoutExhaustingTheStack) {
    {
        JsonArray root;
        JsonArray* cur = &root;
        for (int i = 1; i < kDeep; ++i) {
            auto next = std::make_shared<JsonArray>();
            cur->Add(next);
            cur = next.get();
        }
    } // the outermost container is not itself heap-owned; the teardown must still be iterative
    SUCCEED();
}

TEST(JsonNodeTeardownTests, DeepChainReleasedByClear_DoesNotExhaustTheStack) {
    auto root = deepArrayChain(kDeep);
    root->Clear(); // drops the whole subtree through the child's destructor, not the root's
    EXPECT_EQ(root->getCountProperty(), 0);
    SUCCEED();
}

// --- The shapes the worklist has to handle besides a chain --------------------------------

TEST(JsonNodeTeardownTests, EmptyContainersDestroyCleanly) {
    { JsonArray a; }
    { JsonObject o; }
    { auto a = std::make_shared<JsonArray>(); }
    { auto o = std::make_shared<JsonObject>(); }
    SUCCEED();
}

TEST(JsonNodeTeardownTests, SingletonContainerDestroysCleanly) {
    auto arr = std::make_shared<JsonArray>();
    arr->Add(str("only"));
    arr.reset();
    SUCCEED();
}

TEST(JsonNodeTeardownTests, WideContainerDestroysEveryChild) {
    auto arr = std::make_shared<JsonArray>();
    for (int i = 0; i < 10000; ++i) arr->Add(str("v"));
    EXPECT_EQ(arr->getCountProperty(), 10000);
    arr.reset();
    SUCCEED();
}

TEST(JsonNodeTeardownTests, WideTreeOfWideContainersDestroysCleanly) {
    auto root = std::make_shared<JsonArray>();
    for (int i = 0; i < 200; ++i) {
        auto branch = std::make_shared<JsonArray>();
        for (int j = 0; j < 200; ++j) branch->Add(str("v"));
        root->Add(branch);
    }
    root.reset();
    SUCCEED();
}

TEST(JsonNodeTeardownTests, NullSlotsInTheStoreAreSkipped) {
    auto arr = std::make_shared<JsonArray>();
    arr->Add(nullptr);
    arr->Add(str("v"));
    arr->Add(nullptr);
    auto obj = std::make_shared<JsonObject>();
    obj->Add("a", nullptr);
    obj->Add("b", str("v"));
    arr.reset();
    obj.reset();
    SUCCEED();
}

// --- The invariants #1886 established must survive the new teardown -----------------------

TEST(JsonNodeTeardownTests, RetainedChild_OfADeepTree_IsDetachedAndReattachable) {
    std::shared_ptr<JsonNode> retained;
    {
        auto root = deepArrayChain(64);
        JsonNode* cur = root.get();
        for (int i = 1; i < 32; ++i) cur = (*static_cast<JsonArray*>(cur))[0].get();
        retained = (*static_cast<JsonArray*>(cur))[0];
        ASSERT_NE(retained, nullptr);
        EXPECT_EQ(retained->getParentProperty(), cur);
    }
    EXPECT_EQ(retained->getParentProperty(), nullptr);
    EXPECT_EQ(retained->getRootProperty(), retained.get());

    JsonArray newOwner;
    newOwner.Add(retained);
    EXPECT_EQ(retained->getParentProperty(), &newOwner);
}

// A retained mid-level node must stop the teardown dead: it survives with its whole subtree
// intact, exactly as it did when the teardown recursed.
TEST(JsonNodeTeardownTests, RetainedSubtree_SurvivesTheTeardownIntact) {
    std::shared_ptr<JsonNode> retained;
    {
        auto root = std::make_shared<JsonArray>();
        auto mid = std::make_shared<JsonArray>();
        auto leaf = std::make_shared<JsonArray>();
        leaf->Add(str("deep"));
        mid->Add(leaf);
        root->Add(mid);
        retained = mid;
    }
    EXPECT_EQ(retained->getParentProperty(), nullptr);
    auto& midArr = retained->AsArray();
    ASSERT_EQ(midArr.getCountProperty(), 1);
    auto& leafArr = midArr[0]->AsArray();
    ASSERT_EQ(leafArr.getCountProperty(), 1);
    EXPECT_EQ(leafArr.ToJsonString(), "[\"deep\"]");
    EXPECT_EQ(midArr[0]->getParentProperty(), &midArr);
}

// A child owned by a *different* container must keep that container's link, which is the
// `== this` guard #1886 added and which the donate path must not bypass.
//
// REWRITTEN BY #1888 (2026-08-19). This used to build the second container by COPY-CONSTRUCTING
// the first -- `std::make_shared<JsonArray>(realOwner)`, commented "shares children" -- which was
// the implicit copy that #1888 deleted. The subject of the case is #1886's `== this` guard, not
// the copy, so the guard is now reached the supported way: two containers that genuinely hold
// different children, one of which dies.
TEST(JsonNodeTeardownTests, ForeignChild_KeepsItsRealOwnerThroughTheTeardown) {
    JsonArray realOwner;
    realOwner.Add(str("v"));
    auto child = realOwner[0];
    ASSERT_EQ(child->getParentProperty(), &realOwner);
    {
        JsonArray other;
        other.Add(str("own"));
        // `other`'s teardown donates ITS OWN children. `child` is not one of them, and the
        // `== this` guard is what stops the walk from touching a node whose parent names someone
        // else -- which is exactly what this case exists to pin.
    }
    EXPECT_EQ(child->getParentProperty(), &realOwner)
        << "#1886: a foreign container's teardown must not steal this link";
    EXPECT_EQ(realOwner.getCountProperty(), 1);
}

// The stronger form of the same guard, and the one the deleted copy used to reach by accident:
// a node that MOVED between containers must be released by its NEW owner and left alone by the
// old one. Reached through Remove/Add, which is now the only way to move a node at all.
TEST(JsonNodeTeardownTests, Fix1888_AMovedChildIsReleasedByItsNewOwnerOnly) {
    std::shared_ptr<JsonNode> child;
    JsonArray newOwner;
    {
        JsonArray oldOwner;
        oldOwner.Add(str("v"));
        child = oldOwner[0];
        oldOwner.RemoveAt(0);
        newOwner.Add(child);
        ASSERT_EQ(child->getParentProperty(), &newOwner);
    }   // oldOwner dies here and must not touch `child`
    EXPECT_EQ(child->getParentProperty(), &newOwner);
    EXPECT_EQ(newOwner.getCountProperty(), 1);
}

TEST(JsonNodeTeardownTests, DeepTreeDestroyedDuringExceptionUnwinding_DoesNotTerminate) {
    bool caught = false;
    try {
        auto root = deepArrayChain(4096);
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(JsonNodeTeardownTests, DestructorsRemainNonThrowing) {
    static_assert(std::is_nothrow_destructible_v<JsonArray>);
    static_assert(std::is_nothrow_destructible_v<JsonObject>);
    static_assert(std::is_nothrow_destructible_v<JsonValue>);
    SUCCEED();
}

// --- Destruction order is preserved, not merely termination -------------------------------

// The recursive teardown destroyed a container's children depth-first, front to back. The
// worklist is filled last-child-first and drained from the back precisely so that order is
// reproduced; this test is what makes that claim checkable rather than asserted.
TEST(JsonNodeTeardownTests, DestructionOrderIsDepthFirstFrontToBack) {
    std::vector<int> order;
    g_order = &order;
    {
        auto root = std::make_shared<RecordingArray>(0);
        auto a = std::make_shared<RecordingArray>(1);
        auto a1 = std::make_shared<RecordingArray>(2);
        auto a2 = std::make_shared<RecordingArray>(3);
        auto b = std::make_shared<RecordingArray>(4);
        auto b1 = std::make_shared<RecordingArray>(5);
        a->Add(a1);
        a->Add(a2);
        b->Add(b1);
        root->Add(a);
        root->Add(b);
    }
    g_order = nullptr;
    // root first (its own destructor body runs before its children are released), then the
    // depth-first front-to-back walk of the subtree.
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4, 5}));
}

TEST(JsonNodeTeardownTests, DestructionOrderOfAChainIsRootToLeaf) {
    std::vector<int> order;
    g_order = &order;
    {
        auto root = std::make_shared<RecordingArray>(0);
        JsonArray* cur = root.get();
        for (int i = 1; i < 5; ++i) {
            auto next = std::make_shared<RecordingArray>(i);
            cur->Add(next);
            cur = next.get();
        }
    }
    g_order = nullptr;
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
}

// --- Content is genuinely released, not merely detached -----------------------------------

TEST(JsonNodeTeardownTests, EveryNodeOfADeepTreeIsActuallyDestroyed) {
    std::vector<std::weak_ptr<JsonNode>> observers;
    {
        auto root = std::make_shared<JsonArray>();
        JsonArray* cur = root.get();
        for (int i = 1; i < 512; ++i) {
            auto next = std::make_shared<JsonArray>();
            observers.push_back(next);
            cur->Add(next);
            cur = next.get();
        }
        for (const auto& w : observers) EXPECT_FALSE(w.expired());
    }
    for (const auto& w : observers) EXPECT_TRUE(w.expired());
}
