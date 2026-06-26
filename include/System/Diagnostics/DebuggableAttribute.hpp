// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    /**
     * @brief Modifies code generation for the just-in-time (JIT) debugger.
     * 
     * C++ counterpart of .NET System.Diagnostics.DebuggableAttribute.
     */
    class DebuggableAttribute : public System::Attribute {
    public:
        /** @brief Flags that control debugger behavior for generated code. */
        enum class DebuggingModes {
            None                            = 0,   ///< No special debugging modes.
            Default                         = 1,   ///< Enable default debugging support.
            DisableOptimizations            = 256, ///< Disable JIT optimizations.
            IgnoreSymbolStoreSequencePoints = 2,   ///< Ignore PDB sequence points.
            EnableEditAndContinue           = 4,   ///< Enable edit-and-continue debugging.
        };

    private:
        bool isJITTrackingEnabled_ = false;
        bool isJITOptimizerDisabled_ = false;
        DebuggingModes debuggingModes_ = DebuggingModes::None;

    public:
        /**
         * @brief Constructs the attribute controlling JIT tracking and optimization.
         * @param isJITTrackingEnabled    true to enable JIT tracking.
         * @param isJITOptimizerDisabled  true to disable JIT optimizations.
         */
        DebuggableAttribute(bool isJITTrackingEnabled, bool isJITOptimizerDisabled)
            : isJITTrackingEnabled_(isJITTrackingEnabled),
              isJITOptimizerDisabled_(isJITOptimizerDisabled) {
            debuggingModes_ = isJITTrackingEnabled ? DebuggingModes::Default : DebuggingModes::None;
            if (isJITOptimizerDisabled)
                debuggingModes_ = static_cast<DebuggingModes>(static_cast<int>(debuggingModes_) | static_cast<int>(DebuggingModes::DisableOptimizations));
        }

        /**
         * @brief Constructs the attribute with explicit debugging mode flags.
         * @param modes Combination of DebuggingModes values.
         */
        explicit DebuggableAttribute(DebuggingModes modes) : debuggingModes_(modes) {}

        /** @return true if JIT tracking is enabled. */
        [[nodiscard]] bool            getIsJITTrackingEnabledProperty()   const { return isJITTrackingEnabled_; }
        /** @return true if JIT optimizations are disabled. */
        [[nodiscard]] bool            getIsJITOptimizerDisabledProperty()  const { return isJITOptimizerDisabled_; }
        /** @return The active combination of DebuggingModes flags. */
        [[nodiscard]] DebuggingModes  getDebuggingFlagsProperty()          const { return debuggingModes_; }
    };

} // namespace System::Diagnostics
