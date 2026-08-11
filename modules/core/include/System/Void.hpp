// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    /**
     * @brief Names the .NET `System.Void` type — the metadata type that stands
     * for the `void` return type.
     *
     * @warning **The value API below is this project's own C++ extension; no C#
     * source can use `System.Void` the way this struct can be used.** .NET does
     * publish `System.Void`, and it declares it — as here — with no fields, but
     * it is a *metadata* type and nothing more: it names the `void` return type.
     * C# rejects every ordinary use of it. Constructing one is rejected (CS0143;
     * the struct declares no constructor), and naming it as a value or as a
     * generic argument is rejected outright (CS0673) — which is exactly the
     * `Nullable<System.Void>` and `Task<System.Void>` shape an earlier version of
     * this comment described as the reason the type exists. **No compiling C#
     * source exists for those patterns**, so there is nothing to port from and
     * this header no longer claims to support porting them; C# names the type
     * object with `typeof(void)` instead.
     *
     * What this port actually provides is an ordinary empty C++ struct. It may
     * be declared as an object, copied, compared, stored in a container and
     * passed as a template argument wherever a complete object type is required
     * — every one of which C# forbids for `System.Void`. `ToString()`,
     * `operator==` and `operator!=` are extensions in the same sense: .NET's
     * struct declares no member at all, so none of the three has a .NET
     * counterpart it could match or diverge from, and the empty string
     * `ToString()` returns is a project decision rather than a parity value.
     * Unlike .NET's `ValueType`-derived struct this one has no base class, so
     * there is no inherited object text, no `GetHashCode` and no
     * `Equals(object)`; no ordering and no `std::hash` specialisation are
     * provided either.
     *
     * The only property held in common with .NET is the shape the compiler can
     * see: an empty, field-less, non-polymorphic value type.
     *
     * It has **no first-party production consumer** — nothing in this repository
     * declares, returns or stores a `Void` — so it is a vocabulary for ported
     * code rather than a runtime contract. It stays published, with all three
     * members, because `System/Void.hpp` is already an independently includable
     * public header in the `Core.Base` include tree: withdrawing the header, or
     * withdrawing a member, would be a source break for consumers this
     * repository is not permitted to inspect.
     *
     * SR-AUD-136, tickets #2285 (review) and #2286.
     */
    struct Void {
        /**
         * @brief Returns an empty string.
         *
         * C++ extension. .NET's `System.Void` declares no method, and C# cannot
         * obtain a `System.Void` value on which to call one, so this has no .NET
         * counterpart and the empty result is a project decision.
         */
        [[nodiscard]] std::string ToString() const { return ""; }

        /** @brief Two Void values are always equal. C++ extension; see above. */
        bool operator==(const Void&) const noexcept { return true; }
        /** @brief Two Void values are never unequal. C++ extension; see above. */
        bool operator!=(const Void&) const noexcept { return false; }
    };

} // namespace System
