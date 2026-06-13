// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /// Specifies whether a DateTime value is local, UTC, or unspecified.
    enum class DateTimeKind {
        /// The time is not specified as either local or UTC.
        Unspecified = 0,
        /// The time is expressed as UTC.
        Utc         = 1,
        /// The time is expressed as local time.
        Local       = 2,
    };

} // namespace System
