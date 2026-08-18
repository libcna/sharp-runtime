// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Environment.hpp"

#include <algorithm>
#include <array>

namespace System::detail {

    /**
     * @brief Whether @p folder is one of `Environment::SpecialFolder`'s declared values.
     *
     * Ticket #2378, split out of #2321 once `/rv` settled the question that ticket was deferred
     * on -- and the answer is **platform-dependent**, which #2321 did not anticipate.
     *
     *   - **Unix.** `GetSpecialFolder` is a switch returning `null` for anything it does not
     *     handle, and `GetFolderPathCore` turns `null` into `""`
     *     (`Environment.GetFolderPathCore.Unix.cs:20-24`), under a comment that says outright
     *     *"No need to validate if 'folder' is defined"*. So an undefined folder and a
     *     defined-but-unmapped one are **deliberately indistinguishable**, and both answer `""`.
     *     This predicate is not used there.
     *   - **Windows.** The switch ends in
     *     `throw new ArgumentOutOfRangeException(nameof(folder), folder, SR.Format(SR.Arg_EnumIllegalVal, folder))`
     *     (`Environment.Windows.cs:768-770`), above a `Debug.Assert(!Enum.IsDefined(folder))`
     *     asserting that every **defined** value is handled before it. Reproducing that needs a
     *     definedness table, because 32 of the defined values legitimately reach the default arm
     *     on this port's POSIX mapping and must keep answering `""` there.
     *
     * .NET spells this `Enum.IsDefined`, which is reflection and a permanent deviation here, so
     * the value set is transcribed. It is the enum's own declaration and nothing else: **47
     * enumerators over 46 distinct values**, with 14 undefined holes inside `0x00`-`0x3B` and
     * everything outside that range undefined too.
     *
     * The table lives in a `detail` header rather than inside the `#ifdef _WIN32` arm so that it
     * is **testable on the platform this repository's gate actually runs**. The behaviour it
     * governs is Windows-only; the data is not, and a typo in it is the failure mode worth
     * catching.
     */
    [[nodiscard]] inline bool IsDefinedSpecialFolder(Environment::SpecialFolder folder) noexcept {
        // Sorted, so the lookup is a binary search and a duplicate or an out-of-order edit is
        // visible on sight.
        static constexpr std::array<int, 46> kDefined = {
            0x00, 0x02, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0B,
            0x0D, 0x0E, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16,
            0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x20, 0x21,
            0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
            0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x35,
            0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B
        };
        const int value = static_cast<int>(folder);
        return std::binary_search(kDefined.begin(), kDefined.end(), value);
    }

} // namespace System::detail
