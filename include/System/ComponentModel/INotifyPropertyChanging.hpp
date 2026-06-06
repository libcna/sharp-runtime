// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace System::ComponentModel {

    struct PropertyChangingEventArgs {
        std::string PropertyName;
        explicit PropertyChangingEventArgs(const std::string& name) : PropertyName(name) {}
    };

    using PropertyChangingEventHandler = std::function<void(void*, const PropertyChangingEventArgs&)>;

    class INotifyPropertyChanging {
    public:
        std::vector<PropertyChangingEventHandler> PropertyChanging;
        virtual ~INotifyPropertyChanging() = default;

    protected:
        void OnPropertyChanging(const std::string& name) {
            PropertyChangingEventArgs args(name);
            for (auto& h : PropertyChanging) h(this, args);
        }
    };

} // namespace System::ComponentModel
