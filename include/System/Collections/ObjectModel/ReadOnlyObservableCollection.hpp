// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "System/Collections/ObjectModel/ObservableCollection.hpp"

namespace System::Collections::ObjectModel {

    template<typename T>
    class ReadOnlyObservableCollection {
        const ObservableCollection<T>* source_;
        // Owns a copy when constructed from rvalue
        ObservableCollection<T> owned_;
        bool hasOwned_ = false;

    public:
        explicit ReadOnlyObservableCollection(ObservableCollection<T>& source)
            : source_(&source), hasOwned_(false) {}

        explicit ReadOnlyObservableCollection(ObservableCollection<T>&& source)
            : source_(nullptr), owned_(std::move(source)), hasOwned_(true) {
            source_ = &owned_;
        }

        [[nodiscard]] int  getCountProperty()  const { return source_->getCountProperty(); }
        [[nodiscard]] bool getIsEmptyProperty() const { return source_->getCountProperty() == 0; }

        const T& operator[](int index) const {
            return (*source_)[index];
        }

        [[nodiscard]] bool Contains(const T& item) const { return source_->Contains(item); }
        [[nodiscard]] int  IndexOf(const T& item)  const { return source_->IndexOf(item); }

        auto begin() const { return source_->begin(); }
        auto end()   const { return source_->end(); }
    };

} // namespace System::Collections::ObjectModel
