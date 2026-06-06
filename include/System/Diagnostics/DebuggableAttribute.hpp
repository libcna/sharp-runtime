// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    class DebuggableAttribute : public System::Attribute {
    public:
        enum class DebuggingModes {
            None                       = 0,
            Default                    = 1,
            DisableOptimizations       = 256,
            IgnoreSymbolStoreSequencePoints = 2,
            EnableEditAndContinue      = 4,
        };

    private:
        bool isJITTrackingEnabled_ = false;
        bool isJITOptimizerDisabled_ = false;
        DebuggingModes debuggingModes_ = DebuggingModes::None;

    public:
        DebuggableAttribute(bool isJITTrackingEnabled, bool isJITOptimizerDisabled)
            : isJITTrackingEnabled_(isJITTrackingEnabled),
              isJITOptimizerDisabled_(isJITOptimizerDisabled) {
            debuggingModes_ = isJITTrackingEnabled ? DebuggingModes::Default : DebuggingModes::None;
            if (isJITOptimizerDisabled)
                debuggingModes_ = static_cast<DebuggingModes>(static_cast<int>(debuggingModes_) | static_cast<int>(DebuggingModes::DisableOptimizations));
        }
        explicit DebuggableAttribute(DebuggingModes modes) : debuggingModes_(modes) {}

        [[nodiscard]] bool            getIsJITTrackingEnabledProperty()   const { return isJITTrackingEnabled_; }
        [[nodiscard]] bool            getIsJITOptimizerDisabledProperty()  const { return isJITOptimizerDisabled_; }
        [[nodiscard]] DebuggingModes  getDebuggingFlagsProperty()          const { return debuggingModes_; }
    };

} // namespace System::Diagnostics
