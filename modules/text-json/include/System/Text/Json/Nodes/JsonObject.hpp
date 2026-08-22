// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/Collections/IEnumerator.hpp"
#include <string>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/String.hpp"
#include "System/StringComparison.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"

namespace System::Text::Json::Nodes {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a mutable JSON object (an ordered collection of name/value pairs).
     *
     * C++ counterpart of .NET System.Text.Json.Nodes.JsonObject.
     *
     * @note Backed by a `std::vector<std::pair<...>>` (linear lookup) rather than .NET's hybrid
     * dictionary — preserves insertion order (which .NET's JsonObject also guarantees) without a
     * separate name-to-index hash map; fine for the modestly-sized config/content JSON this
     * runtime's game code realistically works with.
     */
    class JsonObject : public JsonNode {
        std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>> properties_;

        /// Fail-fast enumeration counter (#1889); see JsonArray for why it must be
        /// `detail::MutationCounter` rather than a bare integer.
        System::Collections::detail::MutationCounter version_;

        // Verified against JsonObject.IDictionary.cs's CreateDictionary(): real .NET's backing
        // dictionary uses StringComparer.OrdinalIgnoreCase (when PropertyNameCaseInsensitive is
        // set) as the comparer for every operation -- lookup, ContainsKey, the duplicate-key
        // check in Add(), Remove(), the indexer -- not just reads. Since findIndex() is this
        // port's single choke point for all of those, fixing it here covers every operation
        // uniformly, matching .NET's design.
        [[nodiscard]] intcs findIndex(const std::string& propertyName) const {
            bool caseInsensitive = getOptionsProperty().PropertyNameCaseInsensitive;
            for (size_t i = 0; i < properties_.size(); ++i) {
                bool match = caseInsensitive
                    ? System::String::Equals(properties_[i].first, propertyName, System::StringComparison::OrdinalIgnoreCase)
                    : properties_[i].first == propertyName;
                if (match) return static_cast<intcs>(i);
            }
            return -1;
        }

    public:
        explicit JsonObject(JsonNodeOptions options = {}) : JsonNode(options) {}

        /**
         * @brief Destroys this object, first clearing the parent link of every value it still owns.
         *
         * Property values are held by `std::shared_ptr`, so a value a caller retained outlives
         * this object; `JsonNode`'s `parent_` is a raw non-owning pointer that no destruction path
         * used to clear, so such a value kept pointing into freed storage and every parent-walking
         * entry point (`getParentProperty`, `getRootProperty`, `AssignParent`'s cycle guard) read
         * it. This loop runs before `properties_` is released, so every value is still fully alive
         * here.
         *
         * Only values whose parent link still names *this* object are detached. Copy and move are
         * deleted for JsonNode containers, so this guard records the ownership invariant rather
         * than implying a copy-constructed sharing mode. A retained value is therefore left in
         * exactly the state `Remove`/`SetItem`/`Clear` produce — no parent, its own root, and
         * re-attachable. Non-throwing, so it is safe during exception unwinding and after a
         * partially constructed derived object.
         *
         * The remaining subtree is then released **iteratively** rather than by letting
         * `properties_` recurse into it: a deeply nested tree used to overflow the stack while
         * being released (one frame per level). Depth is now bounded by the heap, not by the call
         * stack. The body is out of line in `JsonNode.cpp` because the worklist it feeds is shared
         * with `~JsonArray` and neither header can see the other's store.
         */
        ~JsonObject() override;

        /** @return The number of properties in the object. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(properties_.size()); }

        /** @return true if a property named @p propertyName exists. */
        [[nodiscard]] bool ContainsKey(const std::string& propertyName) const { return findIndex(propertyName) >= 0; }

        /** @brief Tries to get the value of @p propertyName; returns false if not present. */
        [[nodiscard]] bool TryGetPropertyValue(const std::string& propertyName, std::shared_ptr<JsonNode>& value) const {
            intcs idx = findIndex(propertyName);
            if (idx < 0) return false;
            value = properties_[static_cast<size_t>(idx)].second;
            return true;
        }

        /** @brief Adds a new property. @throws System::ArgumentException if @p propertyName already exists. */
        void Add(const std::string& propertyName, std::shared_ptr<JsonNode> value) {
            if (ContainsKey(propertyName))
                throw System::ArgumentException("An item with the same key has already been added.", "propertyName");
            if (value) value->AssignParent(this);
            properties_.emplace_back(propertyName, std::move(value));
            ++version_;   // #1889
        }

        /** @brief Removes the property named @p propertyName. @return true if it was present. */
        bool Remove(const std::string& propertyName) {
            intcs idx = findIndex(propertyName);
            if (idx < 0) return false;
            if (auto& value = properties_[static_cast<size_t>(idx)].second) value->DetachParent();
            properties_.erase(properties_.begin() + idx);
            ++version_;   // #1889
            return true;
        }

        /** @return The value of @p propertyName, or nullptr if not present (verified against JsonNode.cs's string indexer). */
        [[nodiscard]] std::shared_ptr<JsonNode> operator[](const std::string& propertyName) const {
            intcs idx = findIndex(propertyName);
            if (idx < 0) return nullptr;
            return properties_[static_cast<size_t>(idx)].second;
        }

        /**
         * @brief Sets the value of @p propertyName, adding it if not already present.
         *
         * @p value is adopted first and the replaced value is only detached once that adoption
         * has succeeded, so a rejected call leaves this object exactly as it found it. Doing it
         * the other way round — the ordering this method used to have, and the ordering .NET's
         * own `JsonObject.SetItem` still has (`JsonObject.cs`: `DetachParent(replacedValue);
         * dict.SetAt(index, value); … value?.AssignParent(this);`) — detaches the stored value
         * before `AssignParent` can throw, which leaves this object holding a value that reports
         * **no** parent. Any other container then accepts that value, so one node ends up owned
         * by two containers, each of which will clear the other's link on destruction. This is
         * the ordering `JsonArray::SetItem` has always had.
         *
         * @throws System::InvalidOperationException if @p value already has a parent, or if
         * adopting it would close a cycle. Neither case mutates this object.
         */
        void SetItem(const std::string& propertyName, std::shared_ptr<JsonNode> value) {
            intcs idx = findIndex(propertyName);
            if (idx >= 0) {
                auto& slot = properties_[static_cast<size_t>(idx)].second;
                if (slot == value) return;
                if (value) value->AssignParent(this);
                if (slot) slot->DetachParent();
                slot = std::move(value);
            } else {
                if (value) value->AssignParent(this);
                properties_.emplace_back(propertyName, std::move(value));
            }
            ++version_;   // #1889: replacement changes the enumerated value even at equal count.
        }

        /** @brief Removes all properties from the object. */
        void Clear() {
            for (auto& [name, value] : properties_) if (value) value->DetachParent();
            properties_.clear();
            ++version_;   // #1889
        }

        /**
         * @brief Fail-fast enumerator over the object's properties (#1889).
         *
         * The same repository idiom as `JsonArray::Enumerator`, for the same two measured
         * defects (J11, J12): raw `std::vector` iterators had no version guard, so one held
         * across a reallocating `Add` was an ASan-confirmed heap-use-after-free and one held
         * across `Clear()` silently read destroyed storage with no diagnostic in any build.
         */
        class Enumerator {
            const JsonObject* owner_;
            std::size_t index_;
            System::Collections::detail::MutationVersion version_;

            void requireCurrent() const {
                System::Collections::detail::requireUnmodified(version_ == owner_->version_);
            }

        public:
            Enumerator(const JsonObject* owner, std::size_t index)
                : owner_(owner), index_(index), version_(owner->version_) {}

            [[nodiscard]] const std::pair<std::string, std::shared_ptr<JsonNode>>& operator*() const {
                requireCurrent();
                return owner_->properties_[index_];
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
        [[nodiscard]] Enumerator end() const { return Enumerator(this, properties_.size()); }

        [[nodiscard]] JsonValueKind GetValueKind() const override { return JsonValueKind::Object; }

        [[nodiscard]] nlohmann::ordered_json toNlohmann() const override {
            nlohmann::ordered_json obj = nlohmann::ordered_json::object();
            for (const auto& [name, value] : properties_) obj[name] = value ? value->toNlohmann() : nlohmann::ordered_json(nullptr);
            return obj;
        }

        [[nodiscard]] std::shared_ptr<JsonNode> DeepClone() const override {
            auto clone = std::make_shared<JsonObject>(getOptionsProperty());
            for (const auto& [name, value] : properties_) clone->Add(name, value ? value->DeepClone() : nullptr);
            return clone;
        }
    };

} // namespace System::Text::Json::Nodes
