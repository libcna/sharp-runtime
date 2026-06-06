// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    class Attribute {
    public:
        virtual ~Attribute() = default;

        virtual bool getIsDefaultAttributeProperty() const { return false; }

        virtual bool Match(const Attribute& obj) const {
            return this == &obj;
        }
    };

} // namespace System
