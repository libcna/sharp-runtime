// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::ComponentModel
{
    class IChangeTracking
    {
    public:
        virtual ~IChangeTracking() = default;

        [[nodiscard]] virtual bool getIsChangedProperty() const = 0;
        virtual void AcceptChanges() = 0;
    };
}
