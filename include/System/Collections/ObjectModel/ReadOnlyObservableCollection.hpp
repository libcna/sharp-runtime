// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <vector>
#include "System/Collections/ObjectModel/ObservableCollection.hpp"

namespace System::Collections::ObjectModel {

/**
 * @brief Provides a read-only wrapper around an ObservableCollection that still
 *        propagates CollectionChanged notifications to subscribers.
 *
 * C++ counterpart of .NET System.Collections.ObjectModel.ReadOnlyObservableCollection<T>.
 * Takes ownership of the wrapped ObservableCollection and forwards its CollectionChanged
 * events to any handlers registered on this instance. Mutation is not permitted.
 *
 * @tparam T The type of elements in the collection.
 */
template<typename T>
class ReadOnlyObservableCollection {
public:
    /** @brief Handler type for CollectionChanged subscribers. */
    using ChangedHandler = std::function<void(void*, const NotifyCollectionChangedEventArgs<T>&)>;

    /** @brief List of CollectionChanged event subscribers. */
    std::vector<ChangedHandler> CollectionChanged;

private:
    ObservableCollection<T> owned_;
    const ObservableCollection<T>* source_;

    void subscribeToSource() {
        owned_.CollectionChanged.push_back(
            [this](void* s, const NotifyCollectionChangedEventArgs<T>& args) {
                for (auto& h : CollectionChanged) h(s, args);
            });
    }

    ReadOnlyObservableCollection() : source_(&owned_) { subscribeToSource(); }

public:
    /**
     * @brief Constructs a ReadOnlyObservableCollection by copying @p source.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>(ObservableCollection<T>).
     * @param source The ObservableCollection to copy into this instance.
     */
    explicit ReadOnlyObservableCollection(const ObservableCollection<T>& source)
        : owned_(source), source_(&owned_) {
        subscribeToSource();
    }

    /**
     * @brief Constructs a ReadOnlyObservableCollection that takes ownership of @p source.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>(ObservableCollection<T>).
     * @param source The ObservableCollection to move into this instance.
     */
    explicit ReadOnlyObservableCollection(ObservableCollection<T>&& source)
        : owned_(std::move(source)), source_(&owned_) {
        subscribeToSource();
    }

    /**
     * @brief Gets an empty ReadOnlyObservableCollection instance.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>.Empty.
     * @return A reference to a shared, permanently-empty instance.
     */
    static ReadOnlyObservableCollection<T>& Empty() {
        static ReadOnlyObservableCollection<T> empty;
        return empty;
    }

    /**
     * @brief Gets the number of elements in the collection.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] int getCountProperty() const { return source_->getCountProperty(); }

    /**
     * @brief Gets a value indicating whether the collection contains no elements.
     *
     * C++ counterpart of checking Count == 0 on .NET ReadOnlyObservableCollection<T>.
     * @return true if the collection is empty; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return source_->getCountProperty() == 0; }

    /**
     * @brief Returns a const reference to the element at the specified index.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>.Item[int] getter.
     * @param index The zero-based index.
     * @return A const reference to the element.
     */
    [[nodiscard]] const T& operator[](int index) const { return (*source_)[index]; }

    /**
     * @brief Determines whether the collection contains the specified element.
     *
     * C++ counterpart of .NET ReadOnlyObservableCollection<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const { return source_->Contains(item); }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ReadOnlyCollection<T>.IndexOf(T), inherited by ReadOnlyObservableCollection.
     * @param item The element to locate.
     * @return The zero-based index, or -1 if not found.
     */
    [[nodiscard]] int IndexOf(const T& item) const { return source_->IndexOf(item); }

    /** @brief Returns a const iterator to the beginning of the collection (STL interop). */
    auto begin() const { return source_->begin(); }
    /** @brief Returns a const iterator past the end of the collection (STL interop). */
    auto end()   const { return source_->end(); }
};

} // namespace System::Collections::ObjectModel
