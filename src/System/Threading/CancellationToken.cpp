// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Threading/CancellationToken.hpp"
#include "System/OperationCanceledException.hpp"
namespace System::Threading {
    void CancellationToken::ThrowIfCancellationRequested() const {
        if (cancelled_->load())
            throw System::OperationCanceledException();
    }
} // namespace System::Threading
