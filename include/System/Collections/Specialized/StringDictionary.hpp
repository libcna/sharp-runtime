// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>

namespace System::Collections::Specialized {

    class StringDictionary {
        std::unordered_map<std::string, std::string> data_;

        static std::string lower(const std::string& s) {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), ::tolower);
            return out;
        }

    public:
        StringDictionary() = default;

        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        [[nodiscard]] bool getIsSynchronizedProperty() const { return false; }

        std::string& operator[](const std::string& key) { return data_[lower(key)]; }

        void Add(const std::string& key, const std::string& value) {
            data_[lower(key)] = value;
        }

        void Remove(const std::string& key) { data_.erase(lower(key)); }

        void Clear() { data_.clear(); }

        [[nodiscard]] bool ContainsKey(const std::string& key) const {
            return data_.find(lower(key)) != data_.end();
        }

        [[nodiscard]] bool ContainsValue(const std::string& value) const {
            for (auto& [k, v] : data_) if (v == value) return true;
            return false;
        }

        [[nodiscard]] std::string GetValue(const std::string& key) const {
            auto it = data_.find(lower(key));
            return it != data_.end() ? it->second : std::string{};
        }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
