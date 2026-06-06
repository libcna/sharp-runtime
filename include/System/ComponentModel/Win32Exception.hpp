// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Exception.hpp"

namespace System::ComponentModel {

    class Win32Exception : public System::Exception {
        int nativeErrorCode_;
    public:
        explicit Win32Exception(int errorCode)
            : System::Exception("Win32 error " + std::to_string(errorCode)),
              nativeErrorCode_(errorCode) {}
        Win32Exception(int errorCode, const std::string& message)
            : System::Exception(message), nativeErrorCode_(errorCode) {}

        [[nodiscard]] int getNativeErrorCodeProperty() const { return nativeErrorCode_; }
    };

} // namespace System::ComponentModel
