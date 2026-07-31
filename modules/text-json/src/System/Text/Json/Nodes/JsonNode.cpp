// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

namespace System::Text::Json::Nodes {

    namespace {

        /**
         * Worklist for the iterative container teardown (ticket #1895).
         *
         * Releasing a JSON tree used to recurse: `~JsonArray` released `items_`, which dropped
         * the last `shared_ptr` to each child, which ran that child's destructor, one call frame
         * per level. A 20,000-deep nest therefore crashed with a stack overflow *after* it had
         * been built successfully (probe case J19c), and the depth at which it crashed depended
         * on the thread's stack size rather than on anything the program could see.
         *
         * The fix inverts the flow instead of deepening it. The **outermost** container
         * destructor publishes a worklist here and then drains it in a loop; every container
         * destructor that runs while a worklist is published hands its own children over to that
         * worklist and returns, so it never releases them itself. Stack depth is constant, and the
         * only thing that grows is the worklist — i.e. the bound is the heap, not the stack.
         *
         * `thread_local` because the worklist belongs to one in-progress teardown on one thread.
         * It is never shared between threads, so it needs no synchronisation and introduces no
         * race: two threads tearing trees down at the same time each publish their own.
         */
        thread_local std::vector<std::shared_ptr<JsonNode>>* pendingRelease = nullptr;

        /** Publishes `pending` for the duration of the outermost teardown, and unpublishes it. */
        class PendingReleaseScope {
        public:
            explicit PendingReleaseScope(std::vector<std::shared_ptr<JsonNode>>& pending) noexcept {
                pendingRelease = &pending;
            }
            ~PendingReleaseScope() { pendingRelease = nullptr; }
            PendingReleaseScope(const PendingReleaseScope&) = delete;
            PendingReleaseScope& operator=(const PendingReleaseScope&) = delete;
        };

        /**
         * Grows the worklist once for a whole donation instead of letting `push_back` grow it a
         * child at a time. Growth stays geometric: reserving the exact requirement every time
         * would make a container that repeatedly refills a full worklist reallocate on every
         * donation, i.e. it would trade a stack-depth problem for a quadratic-copying one.
         */
        template <class T>
        void reserveForDonation(std::vector<T>& worklist, size_t incoming) {
            const size_t needed = worklist.size() + incoming;
            if (needed > worklist.capacity())
                worklist.reserve(needed > worklist.capacity() * 2 ? needed : worklist.capacity() * 2);
        }

        /**
         * Moves every non-null child onto the worklist, **last child first**, so that draining
         * the worklist from its back reproduces exactly the front-to-back depth-first order the
         * recursive teardown produced. Anything not moved stays where it is.
         *
         * The only way this can fail is `std::bad_alloc` from the worklist's own growth. That is
         * caught rather than allowed to escape a destructor: whatever is still in `store` keeps
         * its (already detached) children and is released by `store`'s own destructor, which is
         * precisely the recursive behaviour this repair replaces. No node is lost, leaked,
         * double-detached or released twice — the worklist and `store` never hold the same
         * `shared_ptr`, because each one is *moved* across.
         */
        void donateChildren(std::vector<std::shared_ptr<JsonNode>>& store,
                            std::vector<std::shared_ptr<JsonNode>>& worklist) noexcept {
            try {
                reserveForDonation(worklist, store.size());
                for (auto it = store.rbegin(); it != store.rend(); ++it)
                    if (*it) worklist.push_back(std::move(*it));
            } catch (...) {
            }
        }

        /** As donateChildren, for JsonObject's name/value store. */
        void donateValues(std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>>& store,
                          std::vector<std::shared_ptr<JsonNode>>& worklist) noexcept {
            try {
                reserveForDonation(worklist, store.size());
                for (auto it = store.rbegin(); it != store.rend(); ++it)
                    if (it->second) worklist.push_back(std::move(it->second));
            } catch (...) {
            }
        }

        /**
         * Drains the worklist. Each `shared_ptr` released here re-enters a container destructor,
         * which sees a published worklist and donates instead of recursing, so this loop is the
         * only stack frame the whole teardown needs.
         */
        void drainPendingRelease(std::vector<std::shared_ptr<JsonNode>>& worklist) noexcept {
            while (!worklist.empty()) {
                std::shared_ptr<JsonNode> node = std::move(worklist.back());
                worklist.pop_back();
                // `node` is released at the end of this iteration.
            }
        }

    } // namespace

    JsonArray::~JsonArray() {
        for (const auto& item : items_)
            if (item && item->getParentProperty() == this) item->DetachParent();

        if (items_.empty()) return;
        if (pendingRelease != nullptr) {
            donateChildren(items_, *pendingRelease);
            return;
        }
        std::vector<std::shared_ptr<JsonNode>> worklist;
        PendingReleaseScope scope(worklist);
        donateChildren(items_, worklist);
        drainPendingRelease(worklist);
    }

    JsonObject::~JsonObject() {
        for (const auto& [name, value] : properties_)
            if (value && value->getParentProperty() == this) value->DetachParent();

        if (properties_.empty()) return;
        if (pendingRelease != nullptr) {
            donateValues(properties_, *pendingRelease);
            return;
        }
        std::vector<std::shared_ptr<JsonNode>> worklist;
        PendingReleaseScope scope(worklist);
        donateValues(properties_, worklist);
        drainPendingRelease(worklist);
    }

    void JsonNode::AssignParent(JsonNode* parent) {
        // Verified against JsonNode.cs's internal AssignParent: real .NET throws
        // InvalidOperationException both when this node already has a parent (attaching it
        // elsewhere without detaching first would leave the original container holding a
        // dangling reference to a node that now silently reports a different parent) and when
        // walking up from `parent` reaches `this` (attaching would close a cycle: `this` is
        // already an ancestor of, or equal to, `parent`).
        if (getParentProperty())
            throw System::InvalidOperationException("The node already has a parent.");
        for (JsonNode* p = parent; p; p = p->getParentProperty()) {
            if (p == this)
                throw System::InvalidOperationException("A node cycle was detected.");
        }
        parent_ = parent;
    }

    JsonArray& JsonNode::AsArray() {
        auto* p = dynamic_cast<JsonArray*>(this);
        if (!p) throw System::InvalidOperationException("The node is not a JsonArray.");
        return *p;
    }

    JsonObject& JsonNode::AsObject() {
        auto* p = dynamic_cast<JsonObject*>(this);
        if (!p) throw System::InvalidOperationException("The node is not a JsonObject.");
        return *p;
    }

    JsonValue& JsonNode::AsValue() {
        auto* p = dynamic_cast<JsonValue*>(this);
        if (!p) throw System::InvalidOperationException("The node is not a JsonValue.");
        return *p;
    }

    std::string JsonNode::ToJsonString(const JsonSerializerOptions& options) const {
        auto j = toNlohmann();
        return options.getWriteIndentedProperty()
                   ? j.dump(static_cast<int>(options.getIndentSizeProperty()), options.getIndentCharacterProperty())
                   : j.dump();
    }

    bool JsonNode::DeepEquals(const JsonNode* node1, const JsonNode* node2) {
        if (node1 == node2) return true;
        if (node1 == nullptr || node2 == nullptr) return false;
        return node1->toNlohmann() == node2->toNlohmann();
    }

    namespace {
        std::shared_ptr<JsonNode> fromNlohmann(const nlohmann::ordered_json& j, JsonNodeOptions options) {
            if (j.is_null()) return nullptr;
            if (j.is_object()) {
                auto obj = std::make_shared<JsonObject>(options);
                for (const auto& [key, val] : j.items()) obj->Add(key, fromNlohmann(val, options));
                return obj;
            }
            if (j.is_array()) {
                auto arr = std::make_shared<JsonArray>(options);
                for (const auto& item : j) arr->Add(fromNlohmann(item, options));
                return arr;
            }
            return JsonValue::FromNlohmann(j, options);
        }
    } // namespace

    std::shared_ptr<JsonNode> JsonNode::Parse(const std::string& json, JsonNodeOptions options) {
        try {
            auto j = nlohmann::ordered_json::parse(json);
            return fromNlohmann(j, options);
        } catch (const nlohmann::ordered_json::parse_error& e) {
            throw JsonException(std::string("'") + e.what() + "'.");
        }
    }

} // namespace System::Text::Json::Nodes
