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
        Add    = 0,
        Remove = 1,
        Replace = 2,
        Move   = 3,
        Reset  = 4
    };

    /** @brief Event args for CollectionChanged. */
    template<typename T>
    struct NotifyCollectionChangedEventArgs {
        NotifyCollectionChangedAction Action;
        std::vector<T> NewItems;
        std::vector<T> OldItems;
        intcs           NewStartingIndex = -1;
        intcs           OldStartingIndex = -1;

        explicit NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action) : Action(action) {}
        NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction action, T newItem, intcs index = -1)
            : Action(action), NewItems({newItem}), NewStartingIndex(index) {}
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
        using ChangedHandler = std::function<void(void*, const NotifyCollectionChangedEventArgs<T>&)>;
        std::vector<ChangedHandler> CollectionChanged;

        ObservableCollection() = default;
        explicit ObservableCollection(std::vector<T> items) {
            for (auto& item : items) Add(item);
        }

        void Add(const T& item) override {
            intcs idx = this->getCountProperty();
            Collection<T>::Add(item);
            NotifyCollectionChangedEventArgs<T> addArgs(NotifyCollectionChangedAction::Add);
            addArgs.NewItems = {item};
            addArgs.NewStartingIndex = idx;
            Notify(addArgs);
        }

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
