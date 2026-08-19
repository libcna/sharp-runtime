// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2399.
//
// #2399 gave System::Security::Cryptography::RNGCryptoServiceProvider the shape .NET
// declares for it (RNGCryptoServiceProvider.cs:8-10):
//
//   [Obsolete(Obsoletions.RNGCryptoServiceProviderMessage, DiagnosticId = "SYSLIB0023", ...)]
//   [EditorBrowsable(EditorBrowsableState.Never)]
//   public sealed class RNGCryptoServiceProvider : RandomNumberGenerator
//
// This port had a non-final class with no deprecation. Two spellings are therefore
// outlawed, and they are outlawed for two DIFFERENT reasons, which is why each gets
// its own site rather than one "it does not compile" assertion:
//
//   * deriving from it is rejected because the class is now `final`;
//   * naming it at all is rejected because it is now `[[deprecated]]` and this
//     repository builds with -Wall -Wextra -Werror, where a deprecation is a hard
//     ERROR. That boundary is exactly the one #2289 measured before this repository
//     took its first [[deprecated]], and it applies unchanged here.
//
// Measured before landing: ZERO derivations first-party, and ZERO sites of any kind in
// cna and in mobile-eggbert.
//
// Migration: .NET's own message says what to use instead -- the RandomNumberGenerator
// static members (Fill, GetBytes, GetInt32) or RandomNumberGenerator::Create(). A
// consumer that must keep the legacy spelling for a while can wrap its uses in
// `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`, which is what this
// repository's own remaining use does; the suppression is a deliberate, visible
// acknowledgement rather than a silent one.
//
// WHAT THIS FIXTURE DOES NOT CLAIM. #2399 also declined to add .NET's
// `(CspParameters?)` constructor, because CspParameters does not exist in this port and
// inventing it to carry a type whose only behaviour is a throw would be inventing
// public surface. An absence cannot be proved by a fixture -- a fixture proves a
// spelling is REJECTED -- so that one is pinned inside the repository instead.
//
// Records: docs/Migration-RngCryptoServiceProviderSealedAndObsolete.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Security.Cryptography.Random
#include <vector>
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"
#include "System/Security/Cryptography/RNGCryptoServiceProvider.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(rngcryptoserviceprovider-is-final): cannot derive from 'final' base
    //     | is final
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    // The deprecation is suppressed here ON PURPOSE, so that this site fails for the SEAL
    // and only for the seal. Without the suppression the two reasons would be
    // indistinguishable and the site would pass for the wrong one.
    class Derived : public System::Security::Cryptography::RNGCryptoServiceProvider {
    public:
        void GetBytes(std::vector<SharpRuntime::bytecs>& data) override { (void)data; }
    };
#pragma GCC diagnostic pop
    Derived d;
    std::vector<SharpRuntime::bytecs> buffer(4);
    d.GetBytes(buffer);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(rngcryptoserviceprovider-is-deprecated): is deprecated
    //     | deprecated-declarations
    System::Security::Cryptography::RNGCryptoServiceProvider rng;
    std::vector<SharpRuntime::bytecs> buffer(4);
    rng.GetBytes(buffer);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // The (string) constructor is deprecated with the type, and it is worth its own site
    // because a consumer migrating away from the default constructor could plausibly
    // reach for an overload instead and believe it escaped the diagnostic.
    // NEGATIVE(rngcryptoserviceprovider-string-ctor-is-deprecated): is deprecated
    //     | deprecated-declarations
    System::Security::Cryptography::RNGCryptoServiceProvider rng{std::string("ignored")};
    std::vector<SharpRuntime::bytecs> buffer(4);
    rng.GetBytes(buffer);
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the type this
    // one derives from is neither sealed nor deprecated, and its static members are the
    // migration target .NET's own message names.
    std::vector<SharpRuntime::bytecs> ok(4);
    System::Security::Cryptography::RandomNumberGenerator::Fill(ok);
    auto generator = System::Security::Cryptography::RandomNumberGenerator::Create();
    generator->GetBytes(ok);
    return 0;
}
