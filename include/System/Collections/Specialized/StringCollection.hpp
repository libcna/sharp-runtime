// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Specialized {

    class StringCollection {
        std::vector<std::string> data_;

    public:
        StringCollection() = default;

        [[nodiscard]] int getCountProperty() const { return static_cast<int>(data_.size()); }
        [[nodiscard]] bool getIsReadOnlyProperty() const { return false; }

        const std::string& operator[](int index) const {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            return data_[index];
        }
        std::string& operator[](int index) {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            return data_[index];
        }

        int Add(const std::string& value) {
            data_.push_back(value);
            return static_cast<int>(data_.size()) - 1;
        }

        void AddRange(const std::vector<std::string>& values) {
            for (auto& v : values) data_.push_back(v);
        }

        void Insert(int index, const std::string& value) {
            data_.insert(data_.begin() + index, value);
        }

        void Remove(const std::string& value) {
            auto it = std::find(data_.begin(), data_.end(), value);
            if (it != data_.end()) data_.erase(it);
        }

        void RemoveAt(int index) {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            data_.erase(data_.begin() + index);
        }

        void Clear() { data_.clear(); }

        [[nodiscard]] bool Contains(const std::string& value) const {
            return std::find(data_.begin(), data_.end(), value) != data_.end();
        }

        [[nodiscard]] int IndexOf(const std::string& value) const {
            auto it = std::find(data_.begin(), data_.end(), value);
            return it == data_.end() ? -1 : static_cast<int>(it - data_.begin());
        }

        void CopyTo(std::vector<std::string>& dest, int index) const {
            for (size_t i = 0; i < data_.size(); ++i)
                dest[index + static_cast<int>(i)] = data_[i];
        }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }
        auto begin()       { return data_.begin(); }
        auto end()         { return data_.end(); }
    };

} // namespace System::Collections::Specialized
