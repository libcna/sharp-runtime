// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /**
     * @brief Options for URI creation to control canonicalization behavior.
     *
     * C++ counterpart of .NET System.UriCreationOptions (introduced in .NET 6).
     *
     * @warning **No System::Uri operation accepts this type.** The .NET original is
     *   consumed by `Uri(string, UriCreationOptions)` and by an options-bearing
     *   `Uri.TryCreate`; this port has neither overload, so a value of this type can be
     *   constructed and read but can never reach a URI operation at all. That is
     *   SR-AUD-149, and adding the consumer overloads is public API addition, gated as
     *   ticket #1997 group A-3. Ticket #1994 added this disclosure only — the type's
     *   behaviour is unchanged.
     */
    struct UriCreationOptions {
        /**
         * @brief When true, disables path and query canonicalization.
         *
         * Equivalent of .NET UriCreationOptions.DangerousDisablePathAndQueryCanonicalization.
         * Not enforced in this stub — present for API compatibility only.
         */
        bool DangerousDisablePathAndQueryCanonicalization = false;
    };

} // namespace System
