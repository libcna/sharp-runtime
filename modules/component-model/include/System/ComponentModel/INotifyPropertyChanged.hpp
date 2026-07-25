// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "System/MulticastAction.hpp"
#include "System/ComponentModel/PropertyChangedEventArgs.hpp"

namespace System::ComponentModel {

    /** Delegate type for PropertyChanged event handlers. */
    using PropertyChangedEventHandler = std::function<void(void*, const PropertyChangedEventArgs&)>;

    /**
     * Multicast storage for PropertyChanged handlers.
     *
     * `push_back` remains as a compatibility adapter for the previous vector-based event field;
     * `operator+=` is the preferred C#-style subscription API.
     */
    class PropertyChangedEvent final
        : public System::MulticastAction<void*, const PropertyChangedEventArgs&> {
        using Base = System::MulticastAction<void*, const PropertyChangedEventArgs&>;

    public:
        using Base::operator+=;

        /** Adds a handler using the legacy vector-like subscription spelling. */
        void push_back(PropertyChangedEventHandler handler) { this->Add(std::move(handler)); }
    };

    /**
     * @brief Notifies clients that a property value has changed.
     *
     * C++ counterpart of .NET System.ComponentModel.INotifyPropertyChanged.
     */
    class INotifyPropertyChanged {
    public:
        virtual ~INotifyPropertyChanged() = default;

        /** Subscribers notified after a property changes. */
        PropertyChangedEvent PropertyChanged;

    protected:
        /** Raises PropertyChanged for a named property, or for all properties when no name is supplied. */
        void OnPropertyChanged(const std::optional<std::string>& propertyName) {
            PropertyChangedEventArgs args(propertyName);
            PropertyChanged.Invoke(this, args);
        }
    };

} // namespace System::ComponentModel
