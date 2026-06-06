// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Diagnostics {

    class StackFrame {
        std::string fileName_;
        int fileLineNumber_ = 0;
        int fileColumnNumber_ = 0;
        std::string methodName_;
        int nativeOffset_ = 0;
        int ilOffset_ = -1;

    public:
        static constexpr int OFFSET_UNKNOWN = -1;

        StackFrame() = default;
        explicit StackFrame(const std::string& fileName, int lineNumber, int columnNumber = 0)
            : fileName_(fileName), fileLineNumber_(lineNumber), fileColumnNumber_(columnNumber) {}

        [[nodiscard]] const std::string& getFileNameProperty()        const { return fileName_; }
        [[nodiscard]] int                getFileLineNumberProperty()  const { return fileLineNumber_; }
        [[nodiscard]] int                getFileColumnNumberProperty()const { return fileColumnNumber_; }
        [[nodiscard]] const std::string& getMethodNameProperty()      const { return methodName_; }
        [[nodiscard]] int                getNativeOffsetProperty()    const { return nativeOffset_; }
        [[nodiscard]] int                getILOffsetProperty()        const { return ilOffset_; }

        [[nodiscard]] std::string ToString() const {
            if (fileName_.empty()) return "<unknown>";
            return fileName_ + ":line " + std::to_string(fileLineNumber_);
        }
    };

} // namespace System::Diagnostics
