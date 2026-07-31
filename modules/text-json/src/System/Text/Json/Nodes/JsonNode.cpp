// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include <string>
#include <utility>
#include <vector>
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

        /**
         * One suspended container while a tree is being built **iteratively** (ticket #1897).
         *
         * `fromNlohmann` used to call itself once per nesting level, so `JsonNode::Parse` of
         * 20,000 nested arrays crashed with a stack overflow before it had built anything —
         * the measured ASan frames were the port's own `fromNlohmann`, not nlohmann's parser
         * (`build-probe/1897_prefix_asan.log`). Unlike the four other deep shapes in this
         * family, that one is reachable from **untrusted text**: the caller only has to hand
         * `Parse` a string it did not write.
         *
         * A frame records where the source container had got to, the port node being filled
         * from it, and the key that node will be attached under once it is complete. Frames
         * live in a heap `std::vector`, so the only thing that grows with nesting depth is the
         * heap — the C++ call stack stays at one frame however deep the text is.
         */
        struct BuildFrame {
            nlohmann::ordered_json::const_iterator next; ///< the next source child to consume
            nlohmann::ordered_json::const_iterator last; ///< one past the last source child
            std::shared_ptr<JsonNode> node;              ///< the JsonObject/JsonArray being filled
            std::string key;                             ///< key to attach `node` under; empty unless the parent is an object
            bool isObject;                               ///< whether `node` is a JsonObject rather than a JsonArray
        };

        /**
         * Appends `child` to the container `frame` is filling, in document order.
         *
         * The static_cast is safe by construction: `isObject` is recorded from the same source
         * value that decided which node type was created, and nothing between then and here can
         * change either.
         */
        void appendToFrame(BuildFrame& frame, const std::string& key, std::shared_ptr<JsonNode> child) {
            if (frame.isObject)
                static_cast<JsonObject&>(*frame.node).Add(key, std::move(child));
            else
                static_cast<JsonArray&>(*frame.node).Add(std::move(child));
        }

        /** Creates the empty port container matching the source container `j`, and opens a frame over it. */
        BuildFrame openContainer(const nlohmann::ordered_json& j, JsonNodeOptions options, std::string key) {
            const bool isObject = j.is_object();
            std::shared_ptr<JsonNode> node =
                isObject ? std::static_pointer_cast<JsonNode>(std::make_shared<JsonObject>(options))
                         : std::static_pointer_cast<JsonNode>(std::make_shared<JsonArray>(options));
            return BuildFrame{j.begin(), j.end(), std::move(node), std::move(key), isObject};
        }

        /**
         * Builds the port's node tree for one parsed nlohmann value, without recursion.
         *
         * The result is the tree the recursive version built, node for node: a container is
         * created before its children (pre-order allocation, as before), each child is completed
         * before it is attached (as before), and children are attached front to back (as before),
         * so document order, `ToJsonString` output and every parent link are unchanged.
         *
         * Attaching bottom-up also keeps `JsonNode::AssignParent`'s cycle guard **O(1) per
         * attach**, exactly as the recursive version did: a container is attached to its own
         * parent only after that parent has stopped being anyone's child, so the guard's ancestor
         * walk always terminates after one step. Building top-down instead would have made this
         * quadratic — that is the separate, still-open defect #1896 owns, and this repair must
         * not import it.
         *
         * Exception safety is unchanged too. If an allocation throws part-way, `stack` is
         * destroyed by unwinding, which releases every partially built subtree through the
         * iterative teardown ticket #1895 installed. Nothing is leaked and nothing is
         * double-released, because a node is only ever owned by one frame or by one parent.
         */
        std::shared_ptr<JsonNode> fromNlohmann(const nlohmann::ordered_json& j, JsonNodeOptions options) {
            if (j.is_null()) return nullptr;
            if (!j.is_object() && !j.is_array()) return JsonValue::FromNlohmann(j, options);

            std::vector<BuildFrame> stack;
            stack.push_back(openContainer(j, options, std::string()));

            std::shared_ptr<JsonNode> root;
            while (!stack.empty()) {
                BuildFrame& frame = stack.back();

                if (frame.next == frame.last) {
                    // This container is complete: hand it to its parent, or return it as the root.
                    std::shared_ptr<JsonNode> finished = std::move(frame.node);
                    const std::string finishedKey = std::move(frame.key);
                    stack.pop_back();
                    if (stack.empty())
                        root = std::move(finished);
                    else
                        appendToFrame(stack.back(), finishedKey, std::move(finished));
                    continue;
                }

                const nlohmann::ordered_json& child = *frame.next;
                const std::string childKey = frame.isObject ? frame.next.key() : std::string();
                ++frame.next;

                if (child.is_object() || child.is_array()) {
                    // `frame` is invalidated by this push_back if the vector reallocates, so
                    // everything it is needed for has already happened above.
                    stack.push_back(openContainer(child, options, childKey));
                } else if (child.is_null()) {
                    appendToFrame(frame, childKey, nullptr);
                } else {
                    appendToFrame(frame, childKey, JsonValue::FromNlohmann(child, options));
                }
            }
            return root;
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
