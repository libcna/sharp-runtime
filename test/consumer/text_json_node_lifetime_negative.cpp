// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1888 (SR-AUD-327, CCF-019).
//
// THIS IS THE FIXTURE #1894 SPENT SEVEN WEEKS UNABLE TO WRITE. Its acceptance criteria named this
// exact file, and its notes recorded, correctly, that it "cannot be started, not merely should not
// be", because no CCF-019 repair had outlawed any spelling -- every landed one was
// source-compatible by construction. #1888 was the source break that would create them, and it was
// declined. It was approved on 2026-08-19, so there is finally something to reject.
//
// WHAT #1888 OUTLAWED, and why each was a defect rather than a restriction:
//
//   * JsonNode's COPY and MOVE members. A .NET JsonNode is a REFERENCE type, so there was never
//     an object copy to translate -- C++ generated all four implicitly. Copy construction gave a
//     second container sharing the SAME children, each still reporting the ORIGINAL as its parent
//     (probe case J08); copy assignment SLICED, rewriting parent_ on a node that was still stored
//     in a container (J09). `System::Xml::Linq::XObject` already deleted all four, so this ended
//     an asymmetry inside the port.
//
//   * PUBLIC DetachParent(). It let a caller sever the link a container believes it owns and hand
//     the same node to a second one, leaving a node in two containers (J13). .NET's DetachParent
//     is a PRIVATE helper on each container (JsonObject.cs:316, JsonArray.IList.cs:231) whose body
//     is `item?.Parent = null`, and Parent's setter is internal -- and there is NO DetachParent on
//     JsonNode.cs at all, which the port's own header used to claim it mirrored.
//
// MIGRATION: use DeepClone() for a copy, and move a node between containers with Remove/Add. Both
// keep the parent links and the containers in step; neither implicit copy did.
//
// Records: docs/Migration-JsonNodeOwnershipIsNotCopyable.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Text.Json
#include <memory>
#include <string>
#include <utility>

#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

namespace {

    void copyConstructAnArray() {
        JsonArray original;
        original.Add(JsonValue::Create(SharpRuntime::intcs{1}));
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
        // NEGATIVE(jsonarray-copy-construct): use of deleted function
        //     | is private within this context
        //     | no matching function for call to
        JsonArray aliasing = original;
        (void)aliasing;
#else
        const auto clone = original.DeepClone();
        (void)clone;
#endif
    }

    void copyAssignAnArray() {
        JsonArray a;
        JsonArray b;
        b.Add(JsonValue::Create(SharpRuntime::intcs{2}));
#if SHARP_RUNTIME_NEGATIVE_SITE == 2
        // NEGATIVE(jsonarray-copy-assign): use of deleted function
        //     | no match for ‘operator=’
        //     | is private within this context
        a = b;
#else
        (void)a;
        (void)b;
#endif
    }

    void moveConstructAnObject() {
        JsonObject original;
        original.Add("k", JsonValue::Create(SharpRuntime::intcs{3}));
#if SHARP_RUNTIME_NEGATIVE_SITE == 3
        // THE SPELLING MOST LIKELY TO SURVIVE A CARELESS MIGRATION: a move looks like a transfer
        // of ownership and reads as safe.
        //
        // PRECISE ABOUT WHY IT IS REJECTED, because the obvious reading is wrong: this site would
        // be rejected even WITHOUT #1888's `JsonNode(JsonNode&&) = delete`. JsonObject has a
        // user-declared destructor (#1895's iterative teardown), which suppresses its implicit
        // move constructor, so move-constructibility falls back to the deleted copy. #1888's
        // deletion of the move members is measured to be an equivalence today; it states the
        // intent and becomes load-bearing if a container ever drops that destructor. The site is
        // pinned because the SPELLING must stay rejected, whichever rule is doing the rejecting.
        // NEGATIVE(jsonobject-move-construct): use of deleted function
        //     | no matching function for call to
        JsonObject moved = std::move(original);
        (void)moved;
#else
        (void)original;
#endif
    }

    void detachParentFromOutside() {
        JsonObject owner;
        owner.Add("k", JsonValue::Create(SharpRuntime::intcs{4}));
        const std::shared_ptr<JsonNode> child = owner["k"];
#if SHARP_RUNTIME_NEGATIVE_SITE == 4
        // NEGATIVE(detachparent-not-public): is protected within this context
        //     | within this context
        //     | is private within this context
        child->DetachParent();
#else
        owner.Remove("k");
        (void)child;
#endif
    }

    /// A consumer type deriving from JsonNode cannot re-publish DetachParent either -- protected
    /// grants access to a DERIVED object's own member, not to an arbitrary node's. Pinned because
    /// re-exporting through a subclass is the obvious way around the change.
    struct Sneaky : JsonArray {
        void reexport(JsonNode& other) {
#if SHARP_RUNTIME_NEGATIVE_SITE == 5
            // NEGATIVE(detachparent-not-reachable-via-subclass): is protected within this context
            //     | within this context
            //     | cannot call member function
            other.DetachParent();
#else
            (void)other;
#endif
        }
    };

} // namespace

int main() {
    copyConstructAnArray();
    copyAssignAnArray();
    moveConstructAnObject();
    detachParentFromOutside();
    JsonArray other;
    Sneaky{}.reexport(other);
    return 0;
}
