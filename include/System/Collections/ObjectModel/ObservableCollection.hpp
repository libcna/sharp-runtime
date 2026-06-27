// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <vector>
#include "System/Collections/ObjectModel/Collection.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

using SharpRuntime::intcs;

/**
 * @brief Describes the action that caused a CollectionChanged event.
 *
 * C++ counterpart of .NET System.Collections.Specialized.NotifyCollectionChangedAction.
 */
enum class NotifyCollectionChangedAction {
    Add     = 0, ///< An item was added to the collection.
    Remove  = 1, ///< An item was removed from the collection.
    Replace = 2, ///< An item was replaced in the collection.
    Move    = 3, ///< An item was moved within the collection.
    Reset   = 4  ///< The content of the collection changed dramatically.
};

/**
 * @brief Provides data for the CollectionChanged event.
 *
 * C++ counterpart of .NET System.Collections.Specialized.NotifyCollectionChangedEventArgs.
 *
 * @tparam T The type of elements in the collection.
 */
template<typename T>
struct NotifyCollectionChangedEventArgs {
    /** @brief The action that caused the event. */
    NotifyCollectionChangedAction Action;
    /** @brief New items involved in the change (Add, Replace). */
    std::vector<T> NewItems;
    /** @brief Old items involved in the change (Remove, Replace). */
    std::vector<T> OldItems;
    /** @brief Zero-based index at which the change occurred in the new list. */
    intcs NewStartingIndex = -1;
    /** @brief Zero-based index at which the change occurred in the old list. */
    intcs OldStartingIndex = -1;

    /**
     * @brief Constructs event args for a Reset or similarly simple action.
     * @param action The action that caused the event.
     */
    explicit NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action)
        : Action(action) {}

    /**
     * @brief Constructs event args with a single new item and optional starting index.
     * @param action  The action that caused the event.
     * @param newItem The new item involved in the change.
     * @param index   The zero-based starting index of the change.
     */
    NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action,
                                     T newItem, intcs index = -1)
        : Action(action), NewItems({newItem}), NewStartingIndex(index) {}

    /**
     * @brief Constructs event args with a new item, an old item, and optional starting index.
     * @param action  The action that caused the event.
     * @param newItem The new item involved in the change.
     * @param oldItem The old item involved in the change.
     * @param index   The zero-based starting index of the change.
     */
    NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action,
                                     T newItem, T oldItem, intcs index = -1)
        : Action(action), NewItems({newItem}), OldItems({oldItem}), NewStartingIndex(index) {}
};

/**
 * @brief A dynamic data collection that provides notifications when items are added,
 *        removed, moved, or when the whole list is refreshed.
 *
 * C++ counterpart of .NET System.Collections.ObjectModel.ObservableCollection<T>.
 * Subscribers register via the CollectionChanged vector of handler callbacks.
 *
 * @tparam T The type of elements in the collection.
 */
template<typename T>
class ObservableCollection : public Collection<T> {
public:
    /** @brief Handler type for CollectionChanged subscribers. */
    using ChangedHandler = std::function<void(void*, const NotifyCollectionChangedEventArgs<T>&)>;

    /** @brief List of CollectionChanged event subscribers. */
    std::vector<ChangedHandler> CollectionChanged;

    /** @brief Default-constructs an empty ObservableCollection. */
    ObservableCollection() = default;

    /**
     * @brief Constructs an ObservableCollection pre-populated with the given items.
     * @param items The initial items; each fires an Add notification.
     */
    explicit ObservableCollection(std::vector<T> items) {
        for (auto& item : items) Add(item);
    }

    /**
     * @brief Adds @p item to the end of the collection and fires a CollectionChanged Add event.
     *
     * C++ counterpart of .NET ObservableCollection<T>.Add(T) (via Collection<T>.Add).
     * @param item The element to add.
     */
    void Add(const T& item) override {
        intcs idx = this->getCountProperty();
        Collection<T>::Add(item);
        NotifyCollectionChangedEventArgs<T> args(NotifyCollectionChangedAction::Add);
        args.NewItems = {item};
        args.NewStartingIndex = idx;
        notify(args);
    }

    /**
     * @brief Removes the first occurrence of @p item and fires a CollectionChanged Remove event.
     *
     * C++ counterpart of .NET ObservableCollection<T>.Remove(T).
     * @param item The element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const T& item) override {
        auto& items = this->items_;
        for (intcs i = 0; i < static_cast<intcs>(items.size()); ++i) {
            if (items[i] == item) {
                T old = items[i];
                items.erase(items.begin() + i);
                NotifyCollectionChangedEventArgs<T> args(
                    NotifyCollectionChangedAction::Remove, old, old, i);
                args.OldItems = {old};
                args.OldStartingIndex = i;
                notify(args);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Removes all items and fires a CollectionChanged Reset event.
     *
     * C++ counterpart of .NET ObservableCollection<T>.Clear() (via Collection<T>.Clear).
     */
    void Clear() override {
        Collection<T>::Clear();
        notify(NotifyCollectionChangedEventArgs<T>(NotifyCollectionChangedAction::Reset));
    }

    /**
     * @brief Moves the item at @p oldIndex to @p newIndex and fires a CollectionChanged Move event.
     *
     * C++ counterpart of .NET ObservableCollection<T>.Move(int, int).
     * @param oldIndex The zero-based index of the item to move.
     * @param newIndex The zero-based index to move the item to.
     */
    void Move(int oldIndex, int newIndex) {
        auto& items = this->items_;
        T item = items[static_cast<size_t>(oldIndex)];
        items.erase(items.begin() + oldIndex);
        items.insert(items.begin() + newIndex, item);
        NotifyCollectionChangedEventArgs<T> args(NotifyCollectionChangedAction::Move);
        args.NewItems = {item};
        args.OldItems = {item};
        args.NewStartingIndex = static_cast<intcs>(newIndex);
        args.OldStartingIndex = static_cast<intcs>(oldIndex);
        notify(args);
    }

private:
    void notify(const NotifyCollectionChangedEventArgs<T>& args) {
        for (auto& h : CollectionChanged) h(this, args);
    }
};

} // namespace System::Collections::ObjectModel
