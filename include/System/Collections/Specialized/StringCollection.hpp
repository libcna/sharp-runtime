// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Specialized {

    /** A strongly-typed collection of strings. */
    class StringCollection {
        std::vector<std::string> data_;

    public:
        /** Default-constructs an empty StringCollection. */
        StringCollection() = default;

        /** Gets the number of strings in the collection. */
        [[nodiscard]] int getCountProperty() const { return static_cast<int>(data_.size()); }
        /** Returns false (this collection is not read-only). */
        [[nodiscard]] bool getIsReadOnlyProperty() const { return false; }

        /** Returns a const reference to the string at the given index. */
        const std::string& operator[](int index) const {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            return data_[index];
        }
        /** Returns a reference to the string at the given index. */
        std::string& operator[](int index) {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            return data_[index];
        }

        /** Adds a string to the end of the collection; returns the new index. */
        int Add(const std::string& value) {
            data_.push_back(value);
            return static_cast<int>(data_.size()) - 1;
        }

        /** Appends all strings in values to the collection. */
        void AddRange(const std::vector<std::string>& values) {
            for (auto& v : values) data_.push_back(v);
        }

        /** Inserts a string at the specified index. */
        void Insert(int index, const std::string& value) {
            data_.insert(data_.begin() + index, value);
        }

        /** Removes the first occurrence of the given string. */
        void Remove(const std::string& value) {
            auto it = std::find(data_.begin(), data_.end(), value);
            if (it != data_.end()) data_.erase(it);
        }

        /** Removes the string at the specified index. */
        void RemoveAt(int index) {
            if (index < 0 || index >= static_cast<int>(data_.size()))
                throw std::out_of_range("Index out of range.");
            data_.erase(data_.begin() + index);
        }

        /** Removes all strings from the collection. */
        void Clear() { data_.clear(); }

        /** Returns true if the collection contains the given string. */
        [[nodiscard]] bool Contains(const std::string& value) const {
            return std::find(data_.begin(), data_.end(), value) != data_.end();
        }

        /** Returns the zero-based index of the first occurrence of value, or -1 if not found. */
        [[nodiscard]] int IndexOf(const std::string& value) const {
            auto it = std::find(data_.begin(), data_.end(), value);
            return it == data_.end() ? -1 : static_cast<int>(it - data_.begin());
        }

        /** Copies the collection elements into dest starting at the given index. */
        void CopyTo(std::vector<std::string>& dest, int index) const {
            for (size_t i = 0; i < data_.size(); ++i)
                dest[index + static_cast<int>(i)] = data_[i];
        }

        /** Returns a const iterator to the beginning of the collection. */
        auto begin() const { return data_.begin(); }
        /** Returns a const iterator past the end of the collection. */
        auto end()   const { return data_.end(); }
        /** Returns an iterator to the beginning of the collection. */
        auto begin()       { return data_.begin(); }
        /** Returns an iterator past the end of the collection. */
        auto end()         { return data_.end(); }
    };

} // namespace System::Collections::Specialized
