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
        NameValueCollection() = default;

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(keys_.size()); }

        void Add(const std::string& name, const std::string& value) {
            if (!map_.count(name)) keys_.push_back(name);
            map_[name].push_back(value);
        }

        void Set(const std::string& name, const std::string& value) {
            if (!map_.count(name)) keys_.push_back(name);
            map_[name] = { value };
        }

        void Remove(const std::string& name) {
            map_.erase(name);
            keys_.erase(std::remove(keys_.begin(), keys_.end(), name), keys_.end());
        }

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

        [[nodiscard]] const std::string& GetKey(intcs index) const { return keys_.at(static_cast<size_t>(index)); }

        [[nodiscard]] bool HasKeys() const { return !keys_.empty(); }

        [[nodiscard]] const std::vector<std::string>& AllKeys() const { return keys_; }

        std::string operator[](const std::string& name) const { return Get(name); }
        std::string operator[](intcs index) const { return Get(GetKey(index)); }
    };

} // namespace System::Collections::Specialized
