// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::ComponentModel {

    enum class EditorBrowsableState {
        Always   = 0,
        Never    = 1,
        Advanced = 2,
    };

    /** Specifies whether an editor should display the attributed API. */
    class EditorBrowsableAttribute final : public System::Attribute {
        EditorBrowsableState state_;
    public:
        /** Initializes the attribute with the supplied editor-browsing state. */
        explicit EditorBrowsableAttribute(EditorBrowsableState state = EditorBrowsableState::Always)
            : state_(state) {}

        /** Gets the editor-browsing state. */
        [[nodiscard]] EditorBrowsableState getStateProperty() const noexcept { return state_; }

        /** Compares attributes by their editor-browsing state. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const EditorBrowsableAttribute*>(&other);
            return attribute != nullptr && attribute->state_ == state_;
        }
    };

} // namespace System::ComponentModel
