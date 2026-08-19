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
     * @note **Both consumer overloads now exist** — `Uri(string, const UriCreationOptions&)`
     *   and `Uri::TryCreate(string, const UriCreationOptions&, shared_ptr<Uri>&)` — added by
     *   ticket #1997 group A-3, which closed SR-AUD-149's consumer half. Ticket #1994 had
     *   previously added the disclosure that neither existed; that half of the warning is now
     *   obsolete and is replaced by this note.
     *
     * @warning **The option itself remains INERT, and that is the half SR-AUD-149 does not
     *   close.** .NET's flag disables validation and normalisation of the path and query, but
     *   this port's `Uri` performs **no path or query canonicalisation and no
     *   percent-encoding/decoding at all** — a declared limitation of that class
     *   (`docs/SystemUriNamespaceReviewPlan.md` §15). Turning off something that never happens
     *   changes nothing: a `Uri` built with the flag set and one built with it clear are
     *   byte-for-byte identical, and `UriCreationOptionsTest` asserts exactly that.
     *
     *   The overloads exist so ported C# declarations compile and so the caller's expressed
     *   intent is preserved and readable — not because the flag has an effect. Saying so is not
     *   optional: a header that describes an effect it cannot produce, without disclosing it, is
     *   the defect SR-AUD-168 recorded one module over.
     *
     * @note The flag is a **public data member** rather than a rule-5 accessor pair, and that is
     *   deliberate: .NET's is a settable auto-property with no validation, which a public field
     *   is observationally identical to. SA-8 reaches a representation .NET keeps private,
     *   readonly or absent — the same reasoning #1969 recorded for `ChannelOptions`' three base
     *   flags.
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
