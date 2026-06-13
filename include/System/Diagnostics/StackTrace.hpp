// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <sstream>
#include <string>
#include <vector>
#include "System/Diagnostics/StackFrame.hpp"

namespace System::Diagnostics {

    /// @brief Represents an ordered collection of stack frames.
    ///
    /// Partial C++ counterpart of .NET System.Diagnostics.StackTrace.
    class StackTrace {
        std::vector<StackFrame> frames_;

    public:
        /// @brief Constructs an empty StackTrace.
        StackTrace() = default;

        /// @brief Constructs a StackTrace from an existing list of frames.
        /// @param frames Ordered list of StackFrame objects (outermost first).
        explicit StackTrace(const std::vector<StackFrame>& frames) : frames_(frames) {}

        /// @return The number of frames in the stack trace.
        [[nodiscard]] int getFrameCountProperty() const { return static_cast<int>(frames_.size()); }

        /// @brief Returns the frame at the specified @p index.
        /// @param index Zero-based frame index.
        /// @return Pointer to the StackFrame, or nullptr if @p index is out of range.
        [[nodiscard]] const StackFrame* GetFrame(int index) const {
            if (index < 0 || index >= static_cast<int>(frames_.size())) return nullptr;
            return &frames_[index];
        }

        /// @return All frames as an immutable vector reference.
        [[nodiscard]] const std::vector<StackFrame>& GetFrames() const { return frames_; }

        /// @return Multi-line string listing each frame prefixed with "   at ".
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            for (const auto& f : frames_) {
                oss << "   at " << f.ToString() << "\n";
            }
            return oss.str();
        }
    };

} // namespace System::Diagnostics
