// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "System/Collections/ObjectModel/Collection.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

    using SharpRuntime::intcs;

    /** @brief Describes the action that caused a CollectionChanged event. */
    enum class NotifyCollectionChangedAction {
        /** An item was added. */
        Add    = 0,
        /** An item was removed. */
        Remove = 1,
        /** An item was replaced. */
        Replace = 2,
        /** An item was moved. */
        Move   = 3,
        /** The collection was reset. */
        Reset  = 4
    };

    /** @brief Event args for CollectionChanged. */
    template<typename T>
    struct NotifyCollectionChangedEventArgs {
        /** The action that caused the event. */
        NotifyCollectionChangedAction Action;
        /** New items involved in the change. */
        std::vector<T> NewItems;
        /** Old items involved in the change. */
        std::vector<T> OldItems;
        /** Zero-based index at which the change occurred in the new list. */
        intcs           NewStartingIndex = -1;
        /** Zero-based index at which the change occurred in the old list. */
        intcs           OldStartingIndex = -1;

        /** Constructs event args for a Reset or similarly simple action. */
        explicit NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action) : Action(action) {}
        /** Constructs event args with a single new item and optional starting index. */
        NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action, T newItem, intcs index = -1)
            : Action(action), NewItems({newItem}), NewStartingIndex(index) {}
        /** Constructs event args with a new item, an old item, and optional starting index. */
        NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action, T newItem, T oldItem, intcs index = -1)
            : Action(action), NewItems({newItem}), OldItems({oldItem}), NewStartingIndex(index) {}
    };

    /**
     * @brief A dynamic data collection that provides notifications when items get added,
     * removed, or when the whole list is refreshed.
     *
     * Partial C++ counterpart of .NET System.Collections.ObjectModel.ObservableCollection<T>.
     *
     * @note Status: Partial
     */
    template<typename T>
    class ObservableCollection : public Collection<T> {
    public:
        /** Handler type for CollectionChanged subscribers. */
        using ChangedHandler = std::function<void(void*, const NotifyCollectionChangedEventArgs<T>&)>;
        /** List of CollectionChanged event subscribers. */
        std::vector<ChangedHandler> CollectionChanged;

        /** Default-constructs an empty ObservableCollection. */
        ObservableCollection() = default;
        /** Constructs an ObservableCollection pre-populated with the given items. */
        explicit ObservableCollection(std::vector<T> items) {
            for (auto& item : items) Add(item);
        }

        /** Adds item and fires a CollectionChanged Add notification. */
        void Add(const T& item) override {
            intcs idx = this->getCountProperty();
            Collection<T>::Add(item);
            NotifyCollectionChangedEventArgs<T> addArgs(NotifyCollectionChangedAction::Add);
            addArgs.NewItems = {item};
            addArgs.NewStartingIndex = idx;
            Notify(addArgs);
        }

        /** Removes item and fires a CollectionChanged Remove notification; returns true if found. */
        bool Remove(const T& item) override {
            auto& items = this->items_;
            for (intcs i = 0; i < static_cast<intcs>(items.size()); ++i) {
                if (items[i] == item) {
                    T old = items[i];
                    items.erase(items.begin() + i);
                    NotifyCollectionChangedEventArgs<T> args(NotifyCollectionChangedAction::Remove);
                    args.OldItems = {old};
                    args.OldStartingIndex = i;
                    Notify(args);
                    return true;
                }
            }
            return false;
        }

        /** Removes all items and fires a CollectionChanged Reset notification. */
        void Clear() override {
            Collection<T>::Clear();
            Notify(NotifyCollectionChangedEventArgs<T>(NotifyCollectionChangedAction::Reset));
        }

    private:
        void Notify(const NotifyCollectionChangedEventArgs<T>& args) {
            for (auto& h : CollectionChanged) h(this, args);
        }
    };

} // namespace System::Collections::ObjectModel
