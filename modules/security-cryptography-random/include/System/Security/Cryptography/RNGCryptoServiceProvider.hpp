// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"

namespace System::Security::Cryptography {

    /**
     * @brief Legacy CSP-backed random number generator — delegates to the same OS-backed
     * generator as `RandomNumberGenerator::Create()` in this runtime.
     *
     * C++ counterpart of .NET System.Security.Cryptography.RNGCryptoServiceProvider.
     *
     * @deprecated .NET marks this type `[Obsolete]` with diagnostic id **SYSLIB0023**
     * (`RNGCryptoServiceProvider.cs:8`, message at `Obsoletions.cs:82`), and #2399 transcribes
     * that. Under this repository's `-Wall -Wextra -Werror` a use is a hard **error**, not a
     * warning — that is what the attribute means here, and it is the boundary #2289 measured and
     * the user then approved when this repository took its first `[[deprecated]]`.
     *
     * @note `final`, because .NET's is `public sealed class` (`RNGCryptoServiceProvider.cs:10`).
     * Measured before landing: zero derivations first-party, zero sites of any kind in `cna` and
     * `mobile-eggbert`.
     *
     * @note **Three of .NET's four constructors, not four.** The `(string)` and `(byte[])`
     * overloads exist here and, as in .NET, **ignore their argument entirely** — both chain to
     * `this((CspParameters?)null)` there and both simply construct the default generator here.
     * They are `explicit` because a C# constructor never participates in an implicit conversion,
     * so `explicit` is the faithful translation rather than a narrowing. **`(CspParameters?)` is
     * deliberately absent**: `CspParameters` does not exist in this port, and inventing it to
     * carry a type whose only behaviour is `if (cspParams != null) throw new
     * PlatformNotSupportedException()` would be inventing public surface rather than porting it.
     * A later ticket that adds the overload has to justify adding the type first.
     *
     * @note .NET additionally overrides `GetBytes(byte[], int, int)`, `GetBytes(Span)`,
     * `GetNonZeroBytes(byte[])` and `GetNonZeroBytes(Span)` to forward to its inner generator.
     * This port overrides only `GetBytes(std::vector<bytecs>&)` and inherits the rest, whose base
     * implementations call that same virtual — behaviourally identical for every reachable call,
     * recorded so a later reader does not "complete" a set that is already complete.
     */
    class [[deprecated(
        "RNGCryptoServiceProvider is obsolete. To generate a random number, use one of the "
        "RandomNumberGenerator static methods instead.")]] RNGCryptoServiceProvider final
        : public RandomNumberGenerator {
        std::shared_ptr<RandomNumberGenerator> inner_ = RandomNumberGenerator::Create();

    public:
        RNGCryptoServiceProvider() = default;

        /** @brief Constructs the default generator; @p str is ignored, as .NET ignores it. */
        explicit RNGCryptoServiceProvider(const std::string& str) { (void)str; }

        /** @brief Constructs the default generator; @p rgb is ignored, as .NET ignores it. */
        explicit RNGCryptoServiceProvider(const std::vector<bytecs>& rgb) { (void)rgb; }

        void GetBytes(std::vector<bytecs>& data) override { inner_->GetBytes(data); }
    };

} // namespace System::Security::Cryptography
