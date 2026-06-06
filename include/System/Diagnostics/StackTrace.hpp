// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <sstream>
#include <string>
#include <vector>
#include "System/Diagnostics/StackFrame.hpp"

namespace System::Diagnostics {

    class StackTrace {
        std::vector<StackFrame> frames_;

    public:
        StackTrace() = default;
        explicit StackTrace(const std::vector<StackFrame>& frames) : frames_(frames) {}

        [[nodiscard]] int getFrameCountProperty() const { return static_cast<int>(frames_.size()); }

        [[nodiscard]] const StackFrame* GetFrame(int index) const {
            if (index < 0 || index >= static_cast<int>(frames_.size())) return nullptr;
            return &frames_[index];
        }

        [[nodiscard]] const std::vector<StackFrame>& GetFrames() const { return frames_; }

        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            for (const auto& f : frames_) {
                oss << "   at " << f.ToString() << "\n";
            }
            return oss.str();
        }
    };

} // namespace System::Diagnostics
