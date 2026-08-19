// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/Collections/IEnumerator.hpp"
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"

namespace System::Text::Json::Nodes {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a mutable JSON array.
     *
     * C++ counterpart of .NET System.Text.Json.Nodes.JsonArray.
     */
    class JsonArray : public JsonNode {
        std::vector<std::shared_ptr<JsonNode>> items_;

        /// Fail-fast enumeration counter (#1889). CLAUDE.md's collection invariant is binding here:
        /// it must be `detail::MutationCounter` and never a bare integer -- `++` on a signed counter
        /// is undefined at INTCS_MAX, and the implicitly declared assignment operator would
        /// transplant the SOURCE's counter into the destination, leaving an enumerator apparently
        /// valid over storage the assignment destroyed. `MutationCounter` solves both.
        System::Collections::detail::MutationCounter version_;

    public:
        explicit JsonArray(JsonNodeOptions options = {}) : JsonNode(options) {}

        /**
         * @brief Destroys this array, first clearing the parent link of every child it still owns.
         *
         * Items are held by `std::shared_ptr`, so a child a caller retained outlives this array;
         * `JsonNode`'s `parent_` is a raw non-owning pointer that no destruction path used to
         * clear, so such a child kept pointing into freed storage and every parent-walking entry
         * point (`getParentProperty`, `getRootProperty`, `AssignParent`'s cycle guard) read it.
         * This loop runs before `items_` is released, so every child is still fully alive here.
         *
         * Only children whose parent link still names *this* array are detached: a child that was
         * moved to another container, or one shared with a copy-constructed array (whose children
         * still report the original as their parent), is left untouched. A retained child is
         * therefore left in exactly the state `RemoveAt`/`Remove`/`Clear` already produce — no
         * parent, its own root, and re-attachable. Non-throwing, so it is safe during exception
         * unwinding and after a partially constructed derived object.
         *
         * The remaining subtree is then released **iteratively** rather than by letting `items_`
         * recurse into it: a 20,000-deep nest used to overflow the stack while being released
         * (one frame per level). Depth is now bounded by the heap, not by the call stack. The
         * body is out of line in `JsonNode.cpp` because the worklist it feeds is shared with
         * `~JsonObject` and neither header can see the other's store.
         */
        ~JsonArray() override;

        /** @return The number of items in the array. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(items_.size()); }

        /** @return The item at @p index. @throws System::ArgumentOutOfRangeException if out of range. */
        [[nodiscard]] const std::shared_ptr<JsonNode>& operator[](intcs index) const {
            if (index < 0 || index >= static_cast<intcs>(items_.size()))
                throw System::ArgumentOutOfRangeException("index");
            return items_[static_cast<size_t>(index)];
        }

        /** @brief Replaces the item at @p index (adopts @p value as a child, clearing any prior parent link). */
        void SetItem(intcs index, std::shared_ptr<JsonNode> value) {
            if (index < 0 || index >= static_cast<intcs>(items_.size()))
                throw System::ArgumentOutOfRangeException("index");
            if (value) value->AssignParent(this);
            if (auto& old = items_[static_cast<size_t>(index)]) old->DetachParent();
            items_[static_cast<size_t>(index)] = std::move(value);
            ++version_;   // #1889
        }

        /** @brief Appends @p item to the end of the array. */
        void Add(std::shared_ptr<JsonNode> item) {
            if (item) item->AssignParent(this);
            items_.push_back(std::move(item));
            ++version_;   // #1889
        }

        /** @brief Inserts @p item at @p index. @throws System::ArgumentOutOfRangeException if out of range. */
        void Insert(intcs index, std::shared_ptr<JsonNode> item) {
            if (index < 0 || index > static_cast<intcs>(items_.size()))
                throw System::ArgumentOutOfRangeException("index");
            if (item) item->AssignParent(this);
            items_.insert(items_.begin() + index, std::move(item));
            ++version_;   // #1889
        }

        /** @brief Removes the item at @p index. @throws System::ArgumentOutOfRangeException if out of range. */
        void RemoveAt(intcs index) {
            if (index < 0 || index >= static_cast<intcs>(items_.size()))
                throw System::ArgumentOutOfRangeException("index");
            if (auto& item = items_[static_cast<size_t>(index)]) item->DetachParent();
            items_.erase(items_.begin() + index);
            ++version_;   // #1889
        }

        /**
         * @brief Removes the first occurrence of @p item (by pointer identity) from the array.
         * C++ counterpart of .NET JsonArray's IList&lt;JsonNode?&gt;.Remove(JsonNode?).
         * @return true if @p item was found and removed; false otherwise.
         */
        bool Remove(const std::shared_ptr<JsonNode>& item) {
            intcs idx = IndexOf(item);
            if (idx < 0) return false;
            RemoveAt(idx);
            return true;
        }

        /** @brief Removes all items from the array. */
        void Clear() {
            for (auto& item : items_) if (item) item->DetachParent();
            items_.clear();
            ++version_;   // #1889
        }

        /** @return The index of @p item within the array, or -1 if not found (pointer identity). */
        [[nodiscard]] intcs IndexOf(const std::shared_ptr<JsonNode>& item) const {
            for (size_t i = 0; i < items_.size(); ++i)
                if (items_[i] == item) return static_cast<intcs>(i);
            return -1;
        }

        /**
         * @brief Fail-fast enumerator over the array's elements.
         *
         * Ticket **#1889**. `begin()`/`end()` used to hand out **raw `std::vector` iterators with
         * no version guard**, and two measured defects followed:
         *   * an iterator held across a reallocating `Add` was an **ASan-confirmed
         *     heap-use-after-free** -- a SIGSEGV in a build without a sanitizer (probe case J11);
         *   * an iterator held across `Clear()` **silently returned a value from destroyed
         *     storage, with no diagnostic in any build** (J12).
         *
         * This is the repository's standard fail-fast idiom, the same one `List<T>` and `BitArray`
         * use: the container holds a `detail::MutationCounter`, the enumerator snapshots a
         * `detail::MutationVersion`, and a stale dereference or advance raises
         * `InvalidOperationException` rather than reading freed memory.
         */
        class Enumerator {
            const JsonArray* owner_;
            std::size_t index_;
            System::Collections::detail::MutationVersion version_;

            void requireCurrent() const {
                System::Collections::detail::requireUnmodified(version_ == owner_->version_);
            }

        public:
            Enumerator(const JsonArray* owner, std::size_t index)
                : owner_(owner), index_(index), version_(owner->version_) {}

            [[nodiscard]] const std::shared_ptr<JsonNode>& operator*() const {
                requireCurrent();
                return owner_->items_[index_];
            }
            /// Provided because callers iterate with `it->`; a forward iterator must offer it,
            /// and the guard runs here too rather than only on `operator*`.
            [[nodiscard]] auto operator->() const { return &**this; }

            Enumerator& operator++() {
                requireCurrent();
                ++index_;
                return *this;
            }
            [[nodiscard]] bool operator==(const Enumerator& other) const {
                return owner_ == other.owner_ && index_ == other.index_;
            }
            [[nodiscard]] bool operator!=(const Enumerator& other) const { return !(*this == other); }
        };

        [[nodiscard]] Enumerator begin() const { return Enumerator(this, 0); }
        [[nodiscard]] Enumerator end() const { return Enumerator(this, items_.size()); }

        [[nodiscard]] JsonValueKind GetValueKind() const override { return JsonValueKind::Array; }

        [[nodiscard]] nlohmann::ordered_json toNlohmann() const override {
            nlohmann::ordered_json arr = nlohmann::ordered_json::array();
            for (const auto& item : items_) arr.push_back(item ? item->toNlohmann() : nlohmann::ordered_json(nullptr));
            return arr;
        }

        [[nodiscard]] std::shared_ptr<JsonNode> DeepClone() const override {
            auto clone = std::make_shared<JsonArray>(getOptionsProperty());
            for (const auto& item : items_) clone->Add(item ? item->DeepClone() : nullptr);
            return clone;
        }
    };

} // namespace System::Text::Json::Nodes
