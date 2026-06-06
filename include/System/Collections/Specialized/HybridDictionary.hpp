// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include <unordered_map>

namespace System::Collections::Specialized {

    // HybridDictionary: uses a list for small sizes, hash for large.
    // Stub implementation uses unordered_map throughout.
    class HybridDictionary {
        std::unordered_map<std::string, std::any> data_;

    public:
        HybridDictionary() = default;
        explicit HybridDictionary(bool /*caseInsensitive*/) {}

        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        [[nodiscard]] bool getIsReadOnlyProperty() const { return false; }

        void Add(const std::string& key, const std::any& value) {
            data_[key] = value;
        }

        void Remove(const std::string& key) { data_.erase(key); }
        void Clear() { data_.clear(); }

        [[nodiscard]] bool Contains(const std::string& key) const {
            return data_.find(key) != data_.end();
        }

        std::any operator[](const std::string& key) const {
            auto it = data_.find(key);
            return (it != data_.end()) ? it->second : std::any{};
        }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
