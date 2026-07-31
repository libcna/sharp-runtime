// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1897 (CCF-019, probe case X28c): JsonNode::Parse builds its
// tree iteratively, so nesting depth costs heap rather than call stack.
//
// The defect this pins closed is the ONLY case in CCF-019 that was reachable from untrusted
// input. `fromNlohmann` called itself once per nesting level, so parsing text that nested about
// 17,000 deep (8 MiB stack, measured: 16,000 survived, 18,000 did not) killed the process with a
// stack overflow before it had built anything. The caller only had to hand `Parse` a string it
// did not write. Measured ASan frames, all in the port's own recursion rather than in nlohmann's
// parser: build-probe/1897_prefix_asan.log.
//
// The repair is option B of docs/RemainingApprovalDecisions.md §E.1, recorded in
// docs/OwnedTreeLifetimeContractPlan.md §44: an explicit worklist of suspended containers on the
// heap, the same shape #1895 used for teardown. It is COMPATIBLE — no input that parsed before
// stops parsing, no input that failed before starts succeeding, and every accepted document
// produces the identical tree. Roughly half the tests below exist to pin exactly that.
//
// What these tests deliberately do NOT cover, because each is a different root cause with its own
// ticket or its own open decision:
//   * J19c/X27c recursive teardown -> #1895, done, JsonNodeTeardownTests.cpp;
//   * J19d/X27d quadratic ancestor guards during programmatic Add -> #1896, declined;
//   * the depth BOUND (option A: reject beyond JsonDocumentOptions::DefaultMaxDepth, as .NET and
//     as this module's own JsonDocument::Parse already do) -> still an open decision. The
//     deviation it would close is pinned below by ParseAcceptsDepthThatJsonDocumentParseRejects,
//     so option A cannot land silently.
//   * toNlohmann()/ToJsonString() and DeepClone(), which are still recursive. Deep-tree tests
//     here therefore never serialise or clone; that would fail for a reason #1897 does not own.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

using System::Text::Json::JsonDocument;
using System::Text::Json::JsonException;
using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

namespace {

    // The depth probe case X28c uses, and the depth JsonNodeTeardownTests already uses for the
    // teardown half. Comfortably past the ~17,000 stack bound measured on this machine.
    constexpr int kDeep = 20000;

    // Five times past it, so a test that passed only because a future toolchain enlarged the
    // stack would still fail. 0.18 s and 76 MB under ASan (build-probe/1897_asan_cost.log).
    constexpr int kVeryDeep = 100000;

    std::string nestedArrays(int depth, const std::string& core) {
        std::string s;
        s.reserve(static_cast<std::size_t>(depth) * 2 + core.size());
        s.append(static_cast<std::size_t>(depth), '[');
        s += core;
        s.append(static_cast<std::size_t>(depth), ']');
        return s;
    }

    std::string nestedObjects(int depth, const std::string& core) {
        std::string s;
        for (int i = 0; i < depth; ++i) s += "{\"a\":";
        s += core;
        s.append(static_cast<std::size_t>(depth), '}');
        return s;
    }

    // Alternating [ { [ { ... so the worklist has to interleave both container stores.
    std::string nestedMixed(int depth, const std::string& core) {
        std::string s;
        for (int i = 0; i < depth; ++i) s += (i % 2 == 0) ? "[" : "{\"k\":";
        s += core;
        for (int i = depth; i-- > 0;) s += (i % 2 == 0) ? "]" : "}";
        return s;
    }

    // Walks the first-child chain ITERATIVELY, so measuring a deep tree cannot itself recurse.
    int measureDepth(const std::shared_ptr<JsonNode>& root) {
        int d = 0;
        const JsonNode* n = root.get();
        while (n != nullptr) {
            if (const auto* arr = dynamic_cast<const JsonArray*>(n)) {
                if (arr->getCountProperty() == 0) break;
                ++d;
                n = (*arr)[0].get();
            } else if (const auto* obj = dynamic_cast<const JsonObject*>(n)) {
                if (obj->getCountProperty() == 0) break;
                ++d;
                n = obj->begin()->second.get();
            } else {
                break;
            }
        }
        return d;
    }

    // Every container level's child must name that level as its parent. Walks iteratively.
    ::testing::AssertionResult parentChainIsConsistent(const std::shared_ptr<JsonNode>& root) {
        const JsonNode* n = root.get();
        if (n == nullptr) return ::testing::AssertionSuccess();
        if (n->getParentProperty() != nullptr)
            return ::testing::AssertionFailure() << "the root reports a parent";
        for (int level = 0; n != nullptr; ++level) {
            const JsonNode* child = nullptr;
            if (const auto* arr = dynamic_cast<const JsonArray*>(n)) {
                if (arr->getCountProperty() == 0) break;
                child = (*arr)[0].get();
            } else if (const auto* obj = dynamic_cast<const JsonObject*>(n)) {
                if (obj->getCountProperty() == 0) break;
                child = obj->begin()->second.get();
            } else {
                break;
            }
            if (child != nullptr && child->getParentProperty() != n)
                return ::testing::AssertionFailure() << "broken parent link at level " << level;
            n = child;
        }
        return ::testing::AssertionSuccess();
    }

    // The innermost *node* of a first-child chain, reached iteratively. A JSON null is a null
    // slot rather than a node, so a chain that bottoms out in one ends at the container holding
    // it -- which is exactly what a caller inspecting the bottom of such a tree can observe.
    const JsonNode* innermost(const std::shared_ptr<JsonNode>& root) {
        const JsonNode* n = root.get();
        while (n != nullptr) {
            const JsonNode* child = nullptr;
            if (const auto* arr = dynamic_cast<const JsonArray*>(n)) {
                if (arr->getCountProperty() == 0) return n;
                child = (*arr)[0].get();
            } else if (const auto* obj = dynamic_cast<const JsonObject*>(n)) {
                if (obj->getCountProperty() == 0) return n;
                child = obj->begin()->second.get();
            } else {
                return n;
            }
            if (child == nullptr) return n;
            n = child;
        }
        return nullptr;
    }

} // namespace

// --- X28c: the exact untrusted input that used to kill the process ------------------------

TEST(JsonNodeParseDepthTests, DeeplyNestedArrays_AreParsedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedArrays(kDeep, "1")); // used to be SIGSEGV / ASan stack-overflow
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(measureDepth(root), kDeep);
    EXPECT_TRUE(parentChainIsConsistent(root));
}

TEST(JsonNodeParseDepthTests, DeeplyNestedObjects_AreParsedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedObjects(kDeep, "1"));
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(measureDepth(root), kDeep);
    EXPECT_TRUE(parentChainIsConsistent(root));
}

TEST(JsonNodeParseDepthTests, DeeplyNestedMixedContainers_AreParsedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedMixed(kDeep, "true"));
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(measureDepth(root), kDeep);
    EXPECT_TRUE(parentChainIsConsistent(root));
}

// Five times past the measured stack bound: a future toolchain that merely enlarged the stack
// would not make this pass.
TEST(JsonNodeParseDepthTests, VeryDeeplyNestedArrays_AreParsedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedArrays(kVeryDeep, "1"));
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(measureDepth(root), kVeryDeep);
}

TEST(JsonNodeParseDepthTests, VeryDeeplyNestedObjects_AreParsedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedObjects(kVeryDeep, "1"));
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(measureDepth(root), kVeryDeep);
}

// Building it is only half the contract: the tree it produces has to be releasable too, which is
// what #1895's iterative teardown gives it. Parse-then-release is the combination a caller
// actually performs, and neither half may reintroduce the other's crash.
TEST(JsonNodeParseDepthTests, DeepParsedTree_IsAlsoReleasedWithoutExhaustingTheStack) {
    auto root = JsonNode::Parse(nestedArrays(kDeep, "1"));
    ASSERT_NE(root, nullptr);
    root.reset();
    SUCCEED();
}

TEST(JsonNodeParseDepthTests, DeepParsedTree_CarriesItsInnermostValue) {
    auto root = JsonNode::Parse(nestedArrays(kDeep, "\"bottom\""));
    ASSERT_NE(root, nullptr);
    const auto* leaf = innermost(root);
    ASSERT_NE(leaf, nullptr);
    const auto* value = dynamic_cast<const JsonValue*>(leaf);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->GetString(), "bottom");
}

TEST(JsonNodeParseDepthTests, DeepParsedTree_CarriesAnInnermostNull) {
    // A JSON null at the bottom is a null slot, not a node -- the innermost *container* is what
    // survives the walk, and it must hold exactly one empty slot.
    auto root = JsonNode::Parse(nestedArrays(kDeep, "null"));
    ASSERT_NE(root, nullptr);
    const auto* leaf = innermost(root);
    ASSERT_NE(leaf, nullptr);
    const auto* arr = dynamic_cast<const JsonArray*>(leaf);
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(arr->getCountProperty(), 1);
    EXPECT_EQ((*arr)[0], nullptr);
}

TEST(JsonNodeParseDepthTests, DeepParsedTree_KeepsEveryObjectPropertyName) {
    std::string text;
    for (int i = 0; i < 4096; ++i) text += "{\"level\":";
    text += "0";
    text.append(4096, '}');
    auto root = JsonNode::Parse(text);
    ASSERT_NE(root, nullptr);
    const JsonNode* n = root.get();
    for (int i = 0; i < 4096; ++i) {
        const auto* obj = dynamic_cast<const JsonObject*>(n);
        ASSERT_NE(obj, nullptr) << "not an object at level " << i;
        ASSERT_EQ(obj->getCountProperty(), 1);
        EXPECT_EQ(obj->begin()->first, "level");
        n = obj->begin()->second.get();
    }
    ASSERT_NE(n, nullptr);
    const auto* bottom = dynamic_cast<const JsonValue*>(n);
    ASSERT_NE(bottom, nullptr);
    EXPECT_EQ(bottom->GetInt32(), 0);
}

// --- Structurally equivalent INVALID deep input: rejected, not crashed --------------------

TEST(JsonNodeParseDepthTests, DeepMalformedInput_ThrowsJsonExceptionRatherThanCrashing) {
    const std::string opens(kDeep, '[');
    const std::string closes(kDeep, ']');
    const std::vector<std::pair<const char*, std::string>> cases{
        {"unterminated", opens},
        {"garbage core", opens + "@" + closes},
        {"trailing text", opens + "1" + closes + "junk"},
        {"one close missing", opens + "1" + std::string(kDeep - 1, ']')},
        {"one close too many", opens + "1" + closes + "]"},
        {"deep object unterminated", nestedObjects(kDeep, "")},
    };
    for (const auto& [name, text] : cases) {
        SCOPED_TRACE(name);
        EXPECT_THROW(static_cast<void>(JsonNode::Parse(text)), JsonException);
    }
}

// A refused parse must produce nothing at all -- no partially built tree handed back, and the
// caller's own variable untouched. Parse returns by value, so "no partial output" means the
// assignment never happens.
TEST(JsonNodeParseDepthTests, RefusedDeepParse_LeavesTheCallersHandleUntouched) {
    auto sentinel = JsonNode::Parse("[1,2,3]");
    ASSERT_NE(sentinel, nullptr);
    const JsonNode* before = sentinel.get();
    EXPECT_THROW(sentinel = JsonNode::Parse(std::string(kDeep, '[')), JsonException);
    ASSERT_NE(sentinel, nullptr);
    EXPECT_EQ(sentinel.get(), before);
    EXPECT_EQ(sentinel->AsArray().getCountProperty(), 3);
}

// Repeatedly refusing a deep parse must not accumulate anything. Under LSan this is a leak check;
// without it, it at least proves the failure path terminates and stays consistent.
TEST(JsonNodeParseDepthTests, RepeatedRefusedDeepParses_StayConsistent) {
    for (int i = 0; i < 32; ++i)
        EXPECT_THROW(static_cast<void>(JsonNode::Parse(std::string(4096, '['))), JsonException);
    auto ok = JsonNode::Parse("[1]");
    ASSERT_NE(ok, nullptr);
    EXPECT_EQ(ok->AsArray().getCountProperty(), 1);
}

// --- Minimum and boundary inputs ----------------------------------------------------------

TEST(JsonNodeParseDepthTests, MinimumInputs_ParseUnchanged) {
    EXPECT_EQ(JsonNode::Parse("null"), nullptr);
    EXPECT_EQ(JsonNode::Parse("0")->ToJsonString(), "0");
    EXPECT_EQ(JsonNode::Parse("true")->ToJsonString(), "true");
    EXPECT_EQ(JsonNode::Parse("false")->ToJsonString(), "false");
    EXPECT_EQ(JsonNode::Parse("\"\"")->ToJsonString(), "\"\"");
    EXPECT_EQ(JsonNode::Parse("[]")->ToJsonString(), "[]");
    EXPECT_EQ(JsonNode::Parse("{}")->ToJsonString(), "{}");
}

TEST(JsonNodeParseDepthTests, EmptyAndOneLevelContainers_ParseUnchanged) {
    EXPECT_EQ(JsonNode::Parse("[[]]")->ToJsonString(), "[[]]");
    EXPECT_EQ(JsonNode::Parse("[{}]")->ToJsonString(), "[{}]");
    EXPECT_EQ(JsonNode::Parse("{\"a\":[]}")->ToJsonString(), "{\"a\":[]}");
    EXPECT_EQ(JsonNode::Parse("{\"a\":{}}")->ToJsonString(), "{\"a\":{}}");
    EXPECT_EQ(JsonNode::Parse("[[],[[]],[[[]]]]")->ToJsonString(), "[[],[[]],[[[]]]]");
}

TEST(JsonNodeParseDepthTests, EmptyStringIsRejected) {
    EXPECT_THROW(static_cast<void>(JsonNode::Parse("")), JsonException);
}

// --- Behaviour that must be UNCHANGED: this repair accepts and rejects exactly what it did --

TEST(JsonNodeParseDepthTests, RoundTripTextIsByteIdentical) {
    static const char* const kShapes[] = {
        "null", "true", "false", "0", "-1", "1.5", "\"\"", "\"abc\"", "[]", "{}",
        "[null]", "[null,null]", "{\"a\":null}", "[1,2,3]", "{\"b\":1,\"a\":2}",
        "{\"a\":{\"b\":{\"c\":[1,{\"d\":[]},null]}}}",
        "[[],[[]],[[[]]]]",
        "[{\"x\":[1,2,{\"y\":null}]},[],{}]",
        "{\"nested\":{\"arr\":[{\"k\":\"v\"},[true,false]],\"n\":null}}",
    };
    for (const char* text : kShapes) {
        SCOPED_TRACE(text);
        auto node = JsonNode::Parse(text);
        EXPECT_EQ(node ? node->ToJsonString() : std::string("null"), std::string(text));
    }
}

// A worklist drained from the wrong end would reverse siblings. Nothing else in the suite would
// notice on a 3-element array, so this uses a thousand.
TEST(JsonNodeParseDepthTests, WideContainerKeepsDocumentOrder) {
    std::string text = "[";
    for (int i = 0; i < 1000; ++i) {
        if (i) text += ',';
        text += std::to_string(i);
    }
    text += ']';
    auto root = JsonNode::Parse(text);
    auto& arr = root->AsArray();
    ASSERT_EQ(arr.getCountProperty(), 1000);
    for (int i = 0; i < 1000; ++i) EXPECT_EQ(arr[i]->AsValue().GetInt32(), i);
}

TEST(JsonNodeParseDepthTests, ObjectPropertiesKeepDocumentOrder) {
    auto root = JsonNode::Parse("{\"z\":1,\"a\":2,\"m\":3}");
    auto& obj = root->AsObject();
    ASSERT_EQ(obj.getCountProperty(), 3);
    std::vector<std::string> names;
    for (auto it = obj.begin(); names.size() < 3; ++it) names.push_back(it->first);
    EXPECT_EQ(names, (std::vector<std::string>{"z", "a", "m"}));
}

TEST(JsonNodeParseDepthTests, NullSlotsSurviveInBothContainerKinds) {
    auto arr = JsonNode::Parse("[null,1,null]");
    auto& a = arr->AsArray();
    ASSERT_EQ(a.getCountProperty(), 3);
    EXPECT_EQ(a[0], nullptr);
    ASSERT_NE(a[1], nullptr);
    EXPECT_EQ(a[2], nullptr);

    auto obj = JsonNode::Parse("{\"a\":null,\"b\":2}");
    auto& o = obj->AsObject();
    ASSERT_EQ(o.getCountProperty(), 2);
    std::shared_ptr<JsonNode> va;
    std::shared_ptr<JsonNode> vb;
    EXPECT_TRUE(o.TryGetPropertyValue("a", va));
    EXPECT_EQ(va, nullptr);
    EXPECT_TRUE(o.TryGetPropertyValue("b", vb));
    EXPECT_NE(vb, nullptr);
}

TEST(JsonNodeParseDepthTests, ShallowMalformedInputsStillThrowJsonException) {
    for (const char* text : {"[", "]", "{", "}", "[1,", "{\"a\"}", "{\"a\":}", "@", "tru",
                             "[1 2]", "{\"a\":1,}", "\"unterminated"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW(static_cast<void>(JsonNode::Parse(text)), JsonException);
    }
}

TEST(JsonNodeParseDepthTests, ParentLinksAreAssignedThroughEveryPublicContainerPath) {
    auto root = JsonNode::Parse("{\"arr\":[1,{\"deep\":[true]}],\"obj\":{\"k\":\"v\"}}");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getParentProperty(), nullptr);
    auto& obj = root->AsObject();

    std::shared_ptr<JsonNode> arrNode;
    ASSERT_TRUE(obj.TryGetPropertyValue("arr", arrNode));
    ASSERT_NE(arrNode, nullptr);
    EXPECT_EQ(arrNode->getParentProperty(), root.get());
    EXPECT_EQ(arrNode->getRootProperty(), root.get());

    auto& arr = arrNode->AsArray();
    ASSERT_EQ(arr.getCountProperty(), 2);
    EXPECT_EQ(arr[0]->getParentProperty(), arrNode.get());
    EXPECT_EQ(arr[1]->getParentProperty(), arrNode.get());
    EXPECT_EQ(arr[1]->getRootProperty(), root.get());

    auto& inner = arr[1]->AsObject();
    std::shared_ptr<JsonNode> deep;
    ASSERT_TRUE(inner.TryGetPropertyValue("deep", deep));
    EXPECT_EQ(deep->getParentProperty(), &inner);
    EXPECT_EQ(deep->AsArray()[0]->getRootProperty(), root.get());
}

// A parsed node is an ordinary node: it must still detach, re-attach and refuse a cycle exactly
// as a programmatically built one does. The iterative build must not leave a stale parent link.
TEST(JsonNodeParseDepthTests, ParsedNodesRemainDetachableAndReattachable) {
    auto root = JsonNode::Parse("[[1]]");
    auto& arr = root->AsArray();
    auto child = arr[0];
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->getParentProperty(), &arr);

    arr.RemoveAt(0);
    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(child->getRootProperty(), child.get());

    JsonArray newOwner;
    newOwner.Add(child);
    EXPECT_EQ(child->getParentProperty(), &newOwner);
}

// Every node of a parsed tree really is released, not merely detached.
TEST(JsonNodeParseDepthTests, EveryNodeOfAParsedTreeIsActuallyDestroyed) {
    std::vector<std::weak_ptr<JsonNode>> observers;
    {
        auto root = JsonNode::Parse(nestedArrays(512, "1"));
        const JsonNode* n = root.get();
        while (const auto* arr = dynamic_cast<const JsonArray*>(n)) {
            if (arr->getCountProperty() == 0) break;
            observers.push_back((*arr)[0]);
            n = (*arr)[0].get();
        }
        ASSERT_EQ(observers.size(), 512u);
        for (const auto& w : observers) EXPECT_FALSE(w.expired());
    }
    for (const auto& w : observers) EXPECT_TRUE(w.expired());
}

// nlohmann's ordered_map collapses a repeated key, so JsonObject::Add's duplicate-key guard is
// never reached from Parse. Pinned because the iterative build calls that same Add.
TEST(JsonNodeParseDepthTests, DuplicateKeysInSourceTextCollapseWithoutThrowing) {
    auto root = JsonNode::Parse("{\"a\":1,\"a\":2}");
    ASSERT_NE(root, nullptr);
    auto& obj = root->AsObject();
    EXPECT_EQ(obj.getCountProperty(), 1);
    std::shared_ptr<JsonNode> v;
    ASSERT_TRUE(obj.TryGetPropertyValue("a", v));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->AsValue().GetInt32(), 2);
}

// --- The documented deviation option A would close ----------------------------------------

// JsonNode::Parse applies NO depth bound; JsonDocument::Parse in the same module applies
// JsonDocumentOptions::DefaultMaxDepth = 64, and so does .NET's JsonNode.Parse(string) through
// its JsonDocumentOptions parameter. #1897 option B deliberately did not change that, because
// bounding it changes which input is accepted. This test states the disagreement out loud so a
// future option-A ticket has to invert it on purpose rather than discover it.
TEST(JsonNodeParseDepthTests, ParseAcceptsDepthThatJsonDocumentParseRejects) {
    const std::string text = nestedArrays(65, "1");
    auto node = JsonNode::Parse(text);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(measureDepth(node), 65);
    EXPECT_THROW(static_cast<void>(JsonDocument::Parse(text)), JsonException);
}

TEST(JsonNodeParseDepthTests, BothParseEntryPointsAgreeInsideTheDefaultDepthBound) {
    const std::string text = nestedArrays(64, "1");
    auto node = JsonNode::Parse(text);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(measureDepth(node), 64);
    EXPECT_NO_THROW(static_cast<void>(JsonDocument::Parse(text)));
}
