// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/Security/Cryptography/HashAlgorithm.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"

namespace System::Security::Cryptography {

    /**
     * @brief Represents the abstract base class from which all implementations of keyed hash
     * algorithms must derive.
     *
     * C++ counterpart of .NET System.Security.Cryptography.KeyedHashAlgorithm.
     *
     * @note **Key-material contract.** The key is erased from storage this object owns before that
     * storage is released or reused: on Dispose(), on destruction, and on key replacement. A
     * derived class that caches *transformed* key material (HMAC's inner/outer pads are the case
     * this component actually has) owns the same obligation for its own buffers — this base can
     * only reach `keyValue_`. What the erasure does not cover is stated in
     * `docs/SystemSecurityCryptographyNamespaceReviewPlan.md` section 6; in particular
     * getKeyProperty() hands out a **copy**, whose lifetime is then the caller's.
     */
    class KeyedHashAlgorithm : public HashAlgorithm {
        /** @brief Test-only access seam; see `detail/SecureMemory.hpp`. Never defined in production. */
        template <typename>
        friend struct SharpRuntime::Testing::KeyMaterialAccess;

    protected:
        std::vector<bytecs> keyValue_;

    public:
        KeyedHashAlgorithm() = default;
        // Explicitly defaulted so that declaring the erasing destructor below does not suppress the
        // implicit move operations. A suppressed move silently becomes a copy, which would leave
        // the *source* object holding a live key — a regression introduced by the fix itself.
        KeyedHashAlgorithm(const KeyedHashAlgorithm&) = default;
        KeyedHashAlgorithm(KeyedHashAlgorithm&&) = default;
        KeyedHashAlgorithm& operator=(const KeyedHashAlgorithm&) = default;
        KeyedHashAlgorithm& operator=(KeyedHashAlgorithm&&) = default;

        /** @brief Erases the key from the storage this instance owns. */
        ~KeyedHashAlgorithm() override { SharpRuntimeDetail::SecureMemory::ClearContainer(keyValue_); }

        /** @return A copy of the key used by this instance. */
        [[nodiscard]] virtual std::vector<bytecs> getKeyProperty() const { return keyValue_; }

        /** @brief Sets the key used by this instance, erasing the previous one first. */
        virtual void setKeyProperty(std::vector<bytecs> value) {
            SharpRuntimeDetail::SecureMemory::Scrub(keyValue_);
            keyValue_ = std::move(value);
        }

        /** @brief Zeroes the key and releases resources used by this instance. */
        void Dispose() override {
            SharpRuntimeDetail::SecureMemory::ClearContainer(keyValue_);
            HashAlgorithm::Dispose();
        }
    };

} // namespace System::Security::Cryptography
