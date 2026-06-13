// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Diagnostics {

    /// @brief Provides information about a single frame in a call stack.
    ///
    /// Partial C++ counterpart of .NET System.Diagnostics.StackFrame.
    class StackFrame {
        std::string fileName_;
        int fileLineNumber_ = 0;
        int fileColumnNumber_ = 0;
        std::string methodName_;
        int nativeOffset_ = 0;
        int ilOffset_ = -1;

    public:
        static constexpr int OFFSET_UNKNOWN = -1; ///< Sentinel for an unknown offset.

        /// @brief Constructs an empty StackFrame with no location information.
        StackFrame() = default;

        /// @brief Constructs a StackFrame with source location information.
        /// @param fileName   Source file name.
        /// @param lineNumber Line number within the source file.
        /// @param columnNumber Column number within the line (0 if unknown).
        explicit StackFrame(const std::string& fileName, int lineNumber, int columnNumber = 0)
            : fileName_(fileName), fileLineNumber_(lineNumber), fileColumnNumber_(columnNumber) {}

        /// @return The source file name for this frame, or empty if unknown.
        [[nodiscard]] const std::string& getFileNameProperty()        const { return fileName_; }
        /// @return The line number in the source file, or 0 if unknown.
        [[nodiscard]] int                getFileLineNumberProperty()  const { return fileLineNumber_; }
        /// @return The column number in the source file, or 0 if unknown.
        [[nodiscard]] int                getFileColumnNumberProperty()const { return fileColumnNumber_; }
        /// @return The name of the method for this frame, or empty if unknown.
        [[nodiscard]] const std::string& getMethodNameProperty()      const { return methodName_; }
        /// @return The native code offset within the method, or OFFSET_UNKNOWN.
        [[nodiscard]] int                getNativeOffsetProperty()    const { return nativeOffset_; }
        /// @return The IL offset within the method, or OFFSET_UNKNOWN.
        [[nodiscard]] int                getILOffsetProperty()        const { return ilOffset_; }

        /// @return Human-readable representation of this frame.
        [[nodiscard]] std::string ToString() const {
            if (fileName_.empty()) return "<unknown>";
            return fileName_ + ":line " + std::to_string(fileLineNumber_);
        }
    };

} // namespace System::Diagnostics
