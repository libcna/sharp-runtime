// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Text/Json/JsonSerializerOptions.hpp"
#include "System/Text/Json/JsonValueKind.hpp"
#include "System/Text/Json/Nodes/JsonNodeOptions.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json::Nodes {

    using SharpRuntime::intcs;

    class JsonArray;
    class JsonObject;
    class JsonValue;

    /**
     * @brief The abstract base class representing a mutable single node within a JSON tree.
     *
     * C++ counterpart of .NET System.Text.Json.Nodes.JsonNode.
     *
     * @note JSON `null` is represented the same way as in .NET: a `nullptr` `shared_ptr<JsonNode>`
     * slot in a parent JsonObject/JsonArray, not a distinct node instance.
     */
    class JsonNode {
        JsonNode* parent_ = nullptr; // non-owning; children are owned by their parent container
        JsonNodeOptions options_;

    protected:
        explicit JsonNode(JsonNodeOptions options = {}) : options_(options) {}

    public:
        virtual ~JsonNode() = default;

        // Ticket #1888. A .NET `JsonNode` is a REFERENCE type, so there is no object copy to
        // translate -- assigning one C# variable to another copies a reference. C++ generates the
        // copy and move members implicitly here, and all four are wrong for a parented node:
        //
        //   * copy construction gave a second container sharing the SAME children, each of which
        //     still reported the ORIGINAL as its parent (probe case J08);
        //   * copy assignment SLICED, rewriting parent_ on a node that was still stored in a
        //     container (J09).
        //
        // `System::Xml::Linq::XObject` already deletes all four for the same reason, so this ends an
        // asymmetry inside the port rather than inventing a restriction. Use DeepClone() for a copy.
        //
        // HONEST RECORD: deleting the two MOVE members is currently an EQUIVALENCE, and it is kept
        // deliberately rather than because a test can see it. Measured -- restoring them as
        // `= default` changes no observable, for two independent reasons:
        //   * `JsonNode` is ABSTRACT (three pure virtuals), so `is_move_constructible_v<JsonNode>`
        //     is false whatever these declarations say;
        //   * `JsonArray` and `JsonObject` each have a USER-DECLARED destructor (#1895's iterative
        //     teardown), which suppresses their implicit move constructors, so their
        //     move-constructibility falls back to the copy constructor -- already deleted.
        // The deletion states the intent and becomes load-bearing the day a container drops its
        // user-declared destructor. It is not load-bearing today, and the mutation that restores
        // it is reported as uncaught rather than dressed up.
        JsonNode(const JsonNode&) = delete;
        JsonNode& operator=(const JsonNode&) = delete;
        JsonNode(JsonNode&&) = delete;
        JsonNode& operator=(JsonNode&&) = delete;

        /** @return The options this node was constructed with. */
        [[nodiscard]] JsonNodeOptions getOptionsProperty() const { return options_; }

        /** @return The parent node, or nullptr if this is a root node. */
        [[nodiscard]] JsonNode* getParentProperty() const { return parent_; }

        /** @return The root node of the tree this node belongs to. */
        [[nodiscard]] JsonNode* getRootProperty() {
            JsonNode* n = this;
            while (n->parent_) n = n->parent_;
            return n;
        }

        /** @return The kind of JSON value this node represents. */
        [[nodiscard]] virtual JsonValueKind GetValueKind() const = 0;

        /** @brief Internal: converts this node's subtree to an nlohmann::ordered_json value. Not part of .NET's public surface. */
        [[nodiscard]] virtual nlohmann::ordered_json toNlohmann() const = 0;

        /**
         * @brief Internal: attaches this node to a parent container. Not part of .NET's public
         * surface (mirrors JsonNode.cs's internal AssignParent).
         * @throws System::InvalidOperationException if this node already has a parent (would
         * silently detach it from whichever container actually still holds it), or if @p parent
         * is this node itself or one of this node's own descendants (would create a cycle).
         */
        void AssignParent(JsonNode* parent);

    protected:
        /**
         * @brief Clears the parent container pointer.
         *
         * @note **Protected since ticket #1888, and the header's previous note about it was wrong.**
         * It said this "mirrors JsonNode.cs's internal DetachParent" -- there is **no
         * `DetachParent` on `JsonNode.cs` at all**. .NET puts it on the *containers*, as a
         * **private** helper on each (`JsonObject.cs:316`, `JsonArray.IList.cs:231`), whose whole
         * body is `item?.Parent = null` -- and `Parent`'s setter is `internal`. So in .NET a
         * consumer can neither call it nor reach what it does.
         *
         * Public here, it let a caller put one node into **two** containers (probe case J13). It is
         * now protected, with the two containers as friends, which is the same reachability .NET
         * has expressed in C++.
         */
        void DetachParent() { parent_ = nullptr; }

        friend class JsonArray;
        friend class JsonObject;

    private:
        /** @return true if this node currently contains other nodes. #1896's cycle-guard
         *  short-circuit; see AssignParent. Non-virtual by design -- a virtual would be a vtable
         *  change this repair does not need. */
        [[nodiscard]] bool hasChildren() const;

    public:


        /** @return This node cast to JsonArray. @throws System::InvalidOperationException if this isn't a JsonArray. */
        [[nodiscard]] JsonArray& AsArray();
        /** @return This node cast to JsonObject. @throws System::InvalidOperationException if this isn't a JsonObject. */
        [[nodiscard]] JsonObject& AsObject();
        /** @return This node cast to JsonValue. @throws System::InvalidOperationException if this isn't a JsonValue. */
        [[nodiscard]] JsonValue& AsValue();

        /** @return A deep copy of this node (and, if a container, its whole subtree) with no parent. */
        [[nodiscard]] virtual std::shared_ptr<JsonNode> DeepClone() const = 0;

        /** @brief Serializes this node to a JSON string, optionally formatted per @p options. */
        [[nodiscard]] std::string ToJsonString(const JsonSerializerOptions& options = JsonSerializerOptions::Default()) const;

        /** @return Same as ToJsonString() with default options. */
        [[nodiscard]] std::string ToString() const { return ToJsonString(); }

        /** @return true if @p node1 and @p node2 are both nullptr, or both non-null and deeply equal. */
        [[nodiscard]] static bool DeepEquals(const JsonNode* node1, const JsonNode* node2);

        /**
         * @brief Parses JSON text into a JsonNode tree.
         * @return The root node, or nullptr if @p json is the literal "null".
         * @throws System::Text::Json::JsonException if @p json is not a single valid JSON value.
         *
         * @note The tree is built **iteratively** (ticket #1897): nesting depth costs heap, not
         * call stack, so text this method accepts is also built and released successfully however
         * deeply it nests. It used to recurse once per level and crash the process with a stack
         * overflow at roughly 17,000 levels on an 8&nbsp;MiB stack — reachable from untrusted
         * text, since the caller need only pass a string it did not write.
         *
         * @note **Deliberate, documented deviation from .NET.** `JsonNode.Parse(string)` in .NET
         * takes a `JsonDocumentOptions` whose `MaxDepth` defaults to 64 and therefore *rejects*
         * more deeply nested text with a `JsonException`. This method applies no depth bound and
         * accepts it, which is also why it disagrees with its own sibling
         * `System::Text::Json::JsonDocument::Parse`, which does apply
         * `JsonDocumentOptions::DefaultMaxDepth`. Adopting that bound here would change which
         * input is accepted, so it is a separate decision recorded in
         * `docs/OwnedTreeLifetimeContractPlan.md` §44 and not taken by #1897.
         */
        [[nodiscard]] static std::shared_ptr<JsonNode> Parse(const std::string& json, JsonNodeOptions options = {});
    };

} // namespace System::Text::Json::Nodes
