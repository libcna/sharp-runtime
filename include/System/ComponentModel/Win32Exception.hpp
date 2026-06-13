// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Exception.hpp"

namespace System::ComponentModel {

    /// Represents an exception thrown by a Win32 API call, carrying the native error code.
    class Win32Exception : public System::Exception {
        int nativeErrorCode_;
    public:
        /// Constructs a Win32Exception from a numeric Windows error code.
        /// @param errorCode The Win32 error code (e.g. from GetLastError()).
        explicit Win32Exception(int errorCode)
            : System::Exception("Win32 error " + std::to_string(errorCode)),
              nativeErrorCode_(errorCode) {}

        /// Constructs a Win32Exception with a custom message and error code.
        /// @param errorCode The Win32 error code.
        /// @param message   A human-readable description of the error.
        Win32Exception(int errorCode, const std::string& message)
            : System::Exception(message), nativeErrorCode_(errorCode) {}

        /// @return The underlying Win32 error code.
        [[nodiscard]] int getNativeErrorCodeProperty() const { return nativeErrorCode_; }
    };

} // namespace System::ComponentModel
