// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "System/Collections/ObjectModel/ObservableCollection.hpp"

namespace System::Collections::ObjectModel {

    /** A read-only wrapper around an ObservableCollection that still propagates change notifications. */
    template<typename T>
    class ReadOnlyObservableCollection {
        const ObservableCollection<T>* source_;
        // Owns a copy when constructed from rvalue
        ObservableCollection<T> owned_;
        bool hasOwned_ = false;

    public:
        /** Constructs a read-only view over the given observable collection (by reference). */
        explicit ReadOnlyObservableCollection(ObservableCollection<T>& source)
            : source_(&source), hasOwned_(false) {}

        /** Constructs a read-only view that takes ownership of the given observable collection (by move). */
        explicit ReadOnlyObservableCollection(ObservableCollection<T>&& source)
            : source_(nullptr), owned_(std::move(source)), hasOwned_(true) {
            source_ = &owned_;
        }

        /** Gets the number of elements in the underlying collection. */
        [[nodiscard]] int  getCountProperty()  const { return source_->getCountProperty(); }
        /** Returns true if the underlying collection contains no elements. */
        [[nodiscard]] bool getIsEmptyProperty() const { return source_->getCountProperty() == 0; }

        /** Returns a const reference to the element at the specified index. */
        const T& operator[](int index) const {
            return (*source_)[index];
        }

        /** Returns true if the underlying collection contains the specified item. */
        [[nodiscard]] bool Contains(const T& item) const { return source_->Contains(item); }
        /** Returns the zero-based index of the specified item, or -1 if not found. */
        [[nodiscard]] int  IndexOf(const T& item)  const { return source_->IndexOf(item); }

        /** Returns a const iterator to the beginning of the underlying collection. */
        auto begin() const { return source_->begin(); }
        /** Returns a const iterator past the end of the underlying collection. */
        auto end()   const { return source_->end(); }
    };

} // namespace System::Collections::ObjectModel
