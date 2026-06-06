// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Buffers {

    enum class OperationStatus {
        Done                = 0,
        DestinationTooSmall = 1,
        NeedMoreData        = 2,
        InvalidData         = 3,
    };

} // namespace System::Buffers
