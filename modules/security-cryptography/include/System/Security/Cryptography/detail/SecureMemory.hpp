// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <vector>

// Not a public System::* API — the one primitive this component uses to erase key material from
// storage it owns, before that storage is released or reused.
//
// Why it is written this way, and not as `std::memset` / `std::fill`: a write to an object whose
// lifetime is ending, and whose value is never read again, is a dead store. The compiler is
// entitled to delete it, and at -O2 it does. Every access to a `volatile` lvalue is, by contrast,
// an observable side effect the abstract machine requires ([intro.abstract]), so a compiler that
// removed the loop below would be miscompiling the program rather than optimising it.
//
// This erases bytes in storage the process still owns. It is NOT, and must not be described as, a
// guarantee about the rest of the system: it cannot reach a copy the allocator already handed to
// another object, a page the kernel has swapped out, a core dump, a hibernation image, or a value
// the compiler chose to keep in a register or spill to the stack before the call. The measured
// scope of what it does and does not reach is recorded in
// `docs/SystemSecurityCryptographyNamespaceReviewPlan.md` §6.
namespace SharpRuntimeDetail::SecureMemory {

    /**
     * @brief Overwrites @p size bytes at @p data with zero, through volatile accesses the compiler
     * may not elide.
     */
    inline void Clear(void* data, std::size_t size) noexcept {
        if (data == nullptr || size == 0) return;
        volatile unsigned char* p = static_cast<volatile unsigned char*>(data);
        while (size-- > 0) {
            *p++ = 0;
        }
    }

    /**
     * @brief Overwrites every byte a container currently holds, then empties it.
     *
     * The container keeps its capacity: `clear()` does not release the block, so the erased bytes
     * — not the old contents — are what a later reallocation or the destructor hands back to the
     * allocator.
     */
    template <typename T>
    inline void ClearContainer(std::vector<T>& buffer) noexcept {
        Clear(buffer.data(), buffer.size() * sizeof(T));
        buffer.clear();
    }

    /** @brief Overwrites every byte a container holds, leaving its size unchanged. */
    template <typename T>
    inline void Scrub(std::vector<T>& buffer) noexcept {
        Clear(buffer.data(), buffer.size() * sizeof(T));
    }

} // namespace SharpRuntimeDetail::SecureMemory

namespace SharpRuntime::Testing {

    /**
     * @brief Test-only access seam for the private key material of a cryptographic object.
     *
     * Declared here and befriended by `KeyedHashAlgorithm`, `HMAC` and `Rfc2898DeriveBytes`;
     * **never defined in production code**, so no production translation unit and no consumer can
     * name a complete type. Erasure is not observable through any public API by construction —
     * a type that let a caller read its key-derived pads would be the defect, not the test — so a
     * regression that has to prove a buffer was erased needs to look at the buffer.
     *
     * The one definition lives in
     * `modules/security-cryptography/tests/support/KeyMaterialSeam.hpp`;
     * `scripts/check_version_seam_odr.py` enforces that ownership from the source text and
     * `test/consumer/security_cryptography_key_material_negative.cpp` enforces the consumer-side
     * half by compilation. A friend declaration changes no object layout, no signature and no
     * mangled symbol.
     *
     * @tparam TOwner The type whose private key storage the seam reaches.
     */
    template <typename TOwner>
    struct KeyMaterialAccess;

} // namespace SharpRuntime::Testing
