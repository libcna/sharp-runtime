// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "System/MulticastAction.hpp"
#include "System/ComponentModel/PropertyChangingEventArgs.hpp"

namespace System::ComponentModel {

    /** Delegate type for PropertyChanging event handlers. */
    using PropertyChangingEventHandler = std::function<void(void*, const PropertyChangingEventArgs&)>;

    /**
     * Multicast storage for PropertyChanging handlers.
     *
     * `push_back` remains as a compatibility adapter for the previous vector-based event field;
     * `operator+=` is the preferred C#-style subscription API.
     */
    class PropertyChangingEvent final
        : public System::MulticastAction<void*, const PropertyChangingEventArgs&> {
        using Base = System::MulticastAction<void*, const PropertyChangingEventArgs&>;

    public:
        using Base::operator+=;

        /** Adds a handler using the legacy vector-like subscription spelling. */
        void push_back(PropertyChangingEventHandler handler) { this->Add(std::move(handler)); }
    };

    /**
     * Notifies clients that a property value is about to change.
     * C++ counterpart of .NET System.ComponentModel.INotifyPropertyChanging.
     */
    class INotifyPropertyChanging {
    public:
        /** Subscribers notified before a property changes. */
        PropertyChangingEvent PropertyChanging;
        virtual ~INotifyPropertyChanging() = default;

    protected:
        /** Raises PropertyChanging for a named property, or for all properties when no name is supplied. */
        void OnPropertyChanging(const std::optional<std::string>& propertyName) {
            PropertyChangingEventArgs args(propertyName);
            PropertyChanging.Invoke(this, args);
        }
    };

} // namespace System::ComponentModel
