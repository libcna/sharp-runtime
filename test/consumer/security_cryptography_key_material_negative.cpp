// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for tickets #2159 (SR-AUD-332) and #2160 (SR-AUD-331): proves that the
// test-only access seam SharpRuntime::Testing::KeyMaterialAccess<T>, and the private key material
// it reaches, give an ordinary consumer NOTHING.
//
// System/Security/Cryptography/detail/SecureMemory.hpp declares that seam and KeyedHashAlgorithm,
// HMAC and Rfc2898DeriveBytes befriend it, so that the erasure regressions can read a live
// object's key, inner/outer pads, buffered message, password, salt and derived-byte buffer -- none
// of which any public API exposes, because a type that let a caller read its own key-derived pads
// would be the defect those regressions exist to prevent. Production defines nothing: the one
// definition lives in modules/security-cryptography/tests/support/KeyMaterialSeam.hpp, which no
// consumer include path reaches. scripts/check_version_seam_odr.py (ticket #1800) enforces that
// single-definition ownership from the source text; THIS file enforces the consumer-side half of
// it, by compilation, which is the half that catches a mutation the checker cannot see -- making a
// private key buffer public.
//
// It is the third such fixture, after collections_mutation_version_negative.cpp
// (CollectionVersionAccess) and collections_sorted_set_version_negative.cpp
// (SortedSetVersionAccess). CLAUDE.md requires both checks for every seam, because they catch
// different mutations; docs/NegativeConsumerFixtureValidation.md section 18.4 records why.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler; with
// no site selected the file compiles cleanly. scripts/check_negative_consumer_fixtures.py compiles
// each site on its own and asserts the rejection.
//
// Migration for a caller that hits one of these: there is none, and that is the point. A caller
// that needs the key it supplied already has it -- it passed it in -- and getKeyProperty() returns
// a copy for the same reason. The pads, the buffered message and the PBKDF2 state are derived
// implementation detail with no public accessor by design, and erasing them on Dispose is exactly
// the guarantee SR-AUD-331 and SR-AUD-332 were opened about.
//
// One restriction this fixture deliberately does NOT claim, because it is not true: a consumer
// that writes its own explicit specialisation of the seam in namespace SharpRuntime::Testing does
// obtain the access the friend declaration grants. That is well-formed ISO C++, it is equally true
// of the two collections seams, and no C++ mechanism prevents a third party from defining a class
// a header befriends by name. It is recorded in docs/NegativeConsumerFixtureValidation.md section
// 18.5 rather than papered over here.
//
// NEGATIVE-FIXTURE: component=Security.Cryptography

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
// The single defining translation unit sits at modules/security-cryptography/tests/support/, not
// under modules/*/include, and only modules/*/include is on a component's consumer include
// surface, so the shadow path resolves to nothing.
// NEGATIVE(seam-definition-not-on-include-path): No such file or directory
//     | file not found
#include "support/KeyMaterialSeam.hpp"
#endif

#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Security/Cryptography/HMACSHA256.hpp"
#include "System/Security/Cryptography/Rfc2898DeriveBytes.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"

using System::Security::Cryptography::HashAlgorithmName;
using System::Security::Cryptography::HMACSHA256;
using System::Security::Cryptography::Rfc2898DeriveBytes;

int main() {
    std::vector<SharpRuntime::bytecs> key(32, SharpRuntime::bytecs{0xA7});
    HMACSHA256 hmac(key);
    Rfc2898DeriveBytes pbkdf(key, std::vector<SharpRuntime::bytecs>(8, SharpRuntime::bytecs{0x11}), 2,
                             HashAlgorithmName::SHA256);

    // The whole public surface a consumer is entitled to: compute, read back a COPY of the key,
    // derive bytes, and dispose. None of it discloses key-derived state.
    const std::vector<SharpRuntime::bytecs> digest = hmac.ComputeHash(key);
    const std::vector<SharpRuntime::bytecs> copy = hmac.getKeyProperty();
    const std::vector<SharpRuntime::bytecs> derived = pbkdf.GetBytes(8);
    hmac.Dispose();
    pbkdf.Dispose();
    (void)digest;
    (void)copy;
    (void)derived;

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The seam's primary template is declared and never defined in production.
    // NEGATIVE(seam-key-incomplete): incomplete type
    //     | used in nested name specifier
    (void)SharpRuntime::Testing::KeyMaterialAccess<HMACSHA256>::key(hmac);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(seam-inner-pad-incomplete): incomplete type
    //     | used in nested name specifier
    (void)SharpRuntime::Testing::KeyMaterialAccess<HMACSHA256>::innerPad(hmac);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(seam-pbkdf-password-incomplete): incomplete type
    //     | used in nested name specifier
    (void)SharpRuntime::Testing::KeyMaterialAccess<Rfc2898DeriveBytes>::password(pbkdf);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // The pads are the finding: either one recovers the padded key with a fixed XOR, so they must
    // stay private. This is the mutation the seam ODR checker cannot see.
    // NEGATIVE(inner-pad-is-private): is private within this context
    //     | private member
    (void)hmac.innerPad_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(outer-pad-is-private): is private within this context
    //     | private member
    (void)hmac.outerPad_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(pending-message-is-private): is private within this context
    //     | private member
    (void)hmac.pendingMessage_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // keyValue_ is protected, not private: KeyedHashAlgorithm's derived classes need it. A
    // consumer still gets nothing.
    // NEGATIVE(key-value-is-protected): is protected within this context
    //     | protected member
    (void)hmac.keyValue_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(pbkdf-password-is-private): is private within this context
    //     | private member
    (void)pbkdf.password_;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // The disposed flag lives in an existing padding hole and is nobody's business but the type's.
    // NEGATIVE(pbkdf-disposed-flag-is-private): is private within this context
    //     | private member
    (void)pbkdf.disposed_;
#endif

    return 0;
}
