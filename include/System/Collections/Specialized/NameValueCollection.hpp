// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Specialized {

    using SharpRuntime::intcs;

    /**
     * @brief A collection that associates string keys with string values (one key can have multiple values).
     *
     * Partial C++ counterpart of .NET System.Collections.Specialized.NameValueCollection.
     *
     * @note Status: Partial
     */
    class NameValueCollection {
        std::vector<std::string>                                keys_;
        std::unordered_map<std::string, std::vector<std::string>> map_;
    public:
        /// Default-constructs an empty NameValueCollection.
        NameValueCollection() = default;

        /// Gets the number of unique keys in the collection.
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(keys_.size()); }

        /// Adds a name/value pair; multiple values can be associated with the same name.
        void Add(const std::string& name, const std::string& value) {
            if (!map_.count(name)) keys_.push_back(name);
            map_[name].push_back(value);
        }

        /// Sets name to a single value, replacing any existing values.
        void Set(const std::string& name, const std::string& value) {
            if (!map_.count(name)) keys_.push_back(name);
            map_[name] = { value };
        }

        /// Removes all values for the given name.
        void Remove(const std::string& name) {
            map_.erase(name);
            keys_.erase(std::remove(keys_.begin(), keys_.end(), name), keys_.end());
        }

        /// Removes all entries from the collection.
        void Clear() { keys_.clear(); map_.clear(); }

        /** @brief Returns the first value associated with name, or "" if not found. */
        [[nodiscard]] std::string Get(const std::string& name) const {
            auto it = map_.find(name);
            if (it == map_.end() || it->second.empty()) return "";
            return it->second[0];
        }

        /** @brief Returns all values associated with name. */
        [[nodiscard]] std::vector<std::string> GetValues(const std::string& name) const {
            auto it = map_.find(name);
            if (it == map_.end()) return {};
            return it->second;
        }

        /// Returns the key at the given zero-based index.
        [[nodiscard]] const std::string& GetKey(intcs index) const { return keys_.at(static_cast<size_t>(index)); }

        /// Returns true if the collection has at least one key.
        [[nodiscard]] bool HasKeys() const { return !keys_.empty(); }

        /// Returns a const reference to the ordered list of all keys.
        [[nodiscard]] const std::vector<std::string>& AllKeys() const { return keys_; }

        /// Returns the first value for the given name, or "" if not found.
        std::string operator[](const std::string& name) const { return Get(name); }
        /// Returns the first value for the key at the given index, or "" if not found.
        std::string operator[](intcs index) const { return Get(GetKey(index)); }
    };

} // namespace System::Collections::Specialized
