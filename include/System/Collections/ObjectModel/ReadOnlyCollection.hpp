// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "System/Collections/Generic/IList.hpp"

namespace System::Collections::ObjectModel {

/**
 * @brief Provides a read-only wrapper around a generic list.
 *
 * C++ counterpart of .NET System.Collections.ObjectModel.ReadOnlyCollection<T>.
 * Backed by std::vector<T>; mutation methods throw std::runtime_error.
 *
 * @tparam T The type of elements in the collection.
 */
template<typename T>
class ReadOnlyCollection : public Generic::IList<T> {
private:
    std::vector<T> items_;

    class Enumerator : public Generic::IEnumerator<T> {
        const std::vector<T>& items_;
        int index_ = -1;
    public:
        explicit Enumerator(const std::vector<T>& items) : items_(items) {}
        bool MoveNext() override { return ++index_ < static_cast<int>(items_.size()); }
        void Reset() override { index_ = -1; }
        [[nodiscard]] const T& Current() const override {
            return items_[static_cast<size_t>(index_)];
        }
    };

public:
    /** @brief Default-constructs an empty ReadOnlyCollection. */
    ReadOnlyCollection() = default;

    /**
     * @brief Constructs a ReadOnlyCollection wrapping a copy of @p source.
     * @param source The vector to copy into the collection.
     */
    explicit ReadOnlyCollection(const std::vector<T>& source) : items_(source) {}

    /**
     * @brief Constructs a ReadOnlyCollection by moving @p source.
     * @param source The vector to move into the collection.
     */
    explicit ReadOnlyCollection(std::vector<T>&& source) : items_(std::move(source)) {}

    /** @brief Virtual destructor for safe polymorphic destruction. */
    ~ReadOnlyCollection() override = default;

    /**
     * @brief Gets the number of elements in the collection.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] int getCountProperty() const override {
        return static_cast<int>(items_.size());
    }

    /**
     * @brief Gets a value indicating whether the collection is read-only.
     *
     * C++ counterpart of .NET ICollection<T>.IsReadOnly.
     * @return Always true — ReadOnlyCollection<T> does not allow modifications.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return true; }

    /**
     * @brief Returns a const reference to the element at the specified index.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.Item[int] getter.
     * @param index The zero-based index.
     * @return A const reference to the element.
     * @throws std::out_of_range if index is out of range.
     */
    [[nodiscard]] const T& operator[](int index) const override {
        return items_.at(static_cast<size_t>(index));
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    T& operator[](int index) override {
        (void)index;
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.IndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index, or -1 if not found.
     */
    [[nodiscard]] int IndexOf(const T& item) const override {
        for (int i = 0; i < static_cast<int>(items_.size()); ++i)
            if (items_[static_cast<size_t>(i)] == item) return i;
        return -1;
    }

    /**
     * @brief Determines whether the collection contains the specified element.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const override {
        return IndexOf(item) >= 0;
    }

    /**
     * @brief Copies all elements to @p destination starting at @p index.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.CopyTo(T[], int).
     * @param destination The target vector to copy elements into.
     * @param index       The zero-based starting index in @p destination.
     * @throws std::out_of_range if destination is too small.
     */
    void CopyTo(std::vector<T>& destination, int index) const {
        if (index < 0 || index + getCountProperty() > static_cast<int>(destination.size()))
            throw std::out_of_range("Destination too small or index out of range.");
        for (int i = 0; i < getCountProperty(); ++i)
            destination[static_cast<size_t>(index + i)] = items_[static_cast<size_t>(i)];
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    void Add(const T&) override {
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    void Clear() override {
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    bool Remove(const T&) override {
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    void Insert(int, const T&) override {
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Throws because the collection is read-only.
     * @throws std::runtime_error always.
     */
    void RemoveAt(int) override {
        throw std::runtime_error("Collection is read-only.");
    }

    /**
     * @brief Returns a new enumerator that iterates through the collection.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.GetEnumerator().
     * @return A heap-allocated IEnumerator<T>; caller takes ownership.
     */
    Generic::IEnumerator<T>* GetEnumerator() override {
        return new Enumerator(items_);
    }

    /** @brief Returns an iterator to the beginning of the collection (STL interop). */
    auto begin()       { return items_.begin(); }
    /** @brief Returns an iterator past the end of the collection (STL interop). */
    auto end()         { return items_.end(); }
    /** @brief Returns a const iterator to the beginning of the collection (STL interop). */
    [[nodiscard]] auto begin() const { return items_.cbegin(); }
    /** @brief Returns a const iterator past the end of the collection (STL interop). */
    [[nodiscard]] auto end()   const { return items_.cend(); }
};

} // namespace System::Collections::ObjectModel
