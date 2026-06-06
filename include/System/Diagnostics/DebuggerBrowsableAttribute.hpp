// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    enum class DebuggerBrowsableState {
        Never       = 0,
        Collapsed   = 2,
        RootHidden  = 3,
    };

    class DebuggerBrowsableAttribute : public System::Attribute {
        DebuggerBrowsableState state_;
    public:
        explicit DebuggerBrowsableAttribute(DebuggerBrowsableState state) : state_(state) {}
        [[nodiscard]] DebuggerBrowsableState getStateProperty() const { return state_; }
    };

} // namespace System::Diagnostics
