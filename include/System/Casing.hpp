// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

/**
 * @brief Specifies the casing style used in string comparison and conversion.
 *
 * C++ counterpart of .NET System.Casing (internal enum).
 * Used by globalization routines to distinguish upper-case from lower-case operations.
 */
enum class Casing {
    /** @brief Indicates upper-casing. */
    Upper = 0,
    /** @brief Indicates lower-casing. */
    Lower = 2,
};

} // namespace System
