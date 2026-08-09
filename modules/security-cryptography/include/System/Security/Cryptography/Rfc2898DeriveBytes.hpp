// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "System/Security/Cryptography/DeriveBytes.hpp"
#include "System/Security/Cryptography/HMAC.hpp"
#include "System/Security/Cryptography/HashAlgorithmName.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"

namespace System::Security::Cryptography {

    /**
     * @brief Implements PBKDF2 (RFC 2898 / RFC 8018) key derivation using an HMAC pseudo-random
     * function.
     *
     * C++ counterpart of .NET System.Security.Cryptography.Rfc2898DeriveBytes.
     *
     * @note Restricted to SHA1/SHA256/SHA384/SHA512 (MD5 deliberately excluded), matching .NET's
     * own restriction in `OpenHmac()`. SHA3-256/384/512 are excluded for the same reason — .NET's
     * `OpenHmac()` does not accept them either — and **not** because this runtime lacks SHA-3: it
     * has `SHA3_256`/`SHA3_384`/`SHA3_512` and `HMACSHA3_*` in this very component, and `HKDF`
     * accepts them. (The comment that used to stand here said SHA-3 "hasn't been ported"; that was
     * true when it was written and stopped being true without the comment noticing.)
     *
     * @note **Key-material contract.** `Dispose()` erases the password, the salt/counter buffer and
     * the derived-byte buffer from the storage this object owns, and every door that would keep
     * deriving or hand out state afterwards throws `ObjectDisposedException`. Destruction erases
     * the same three buffers, so a caller who never calls `Dispose()` — the ordinary C++ path — is
     * covered too, and a **rejected constructor argument** erases the password it had already
     * consumed. What this does not cover is stated in
     * `docs/SystemSecurityCryptographyNamespaceReviewPlan.md` section 6.
     */
    class Rfc2898DeriveBytes : public DeriveBytes {
        /** @brief Test-only access seam; see `detail/SecureMemory.hpp`. Never defined in production. */
        template <typename>
        friend struct SharpRuntime::Testing::KeyMaterialAccess;

        std::vector<bytecs> password_;
        std::vector<bytecs> salt_; // salt bytes followed by a 4-byte big-endian block counter
        uint32_t iterations_;
        HashAlgorithmName hashAlgorithm_;
        intcs blockSize_ = 0;
        // Deliberately here, between `blockSize_` and `buffer_`: `buffer_` needs 8-byte alignment,
        // so the four bytes after `blockSize_` are padding no member uses. Declaring the flag in
        // that hole keeps `sizeof` at 160, `alignof` at 8, and every other member at the offset it
        // already had — measured, not assumed (plan section 4.1). Appending it instead would have
        // cost 8 bytes.
        bool disposed_ = false;
        std::vector<bytecs> buffer_;
        uint32_t block_ = 0;
        size_t startIndex_ = 0;
        size_t endIndex_ = 0;

        [[nodiscard]] std::unique_ptr<HMAC> createHmac() const;
        void func();
        void initialize();
        void throwIfDisposed() const;

        /**
         * @brief Validates the arguments and hands @p password back, erasing it first if they are
         * rejected.
         *
         * Called from the member-initialiser list so that validation happens **before** the
         * password reaches a member. Validating in the constructor body instead meant a rejected
         * call destroyed a fully populated `password_` without erasing it, which measurement found
         * in freed storage (plan section 4.2 site 5).
         */
        static std::vector<bytecs> acceptPassword(std::vector<bytecs> password, intcs iterations,
                                                   const HashAlgorithmName& hashAlgorithm);

    public:
        Rfc2898DeriveBytes(std::vector<bytecs> password, std::vector<bytecs> salt, intcs iterations,
                            HashAlgorithmName hashAlgorithm = HashAlgorithmName::SHA1);
        Rfc2898DeriveBytes(const std::string& password, std::vector<bytecs> salt, intcs iterations,
                            HashAlgorithmName hashAlgorithm = HashAlgorithmName::SHA1);

        // Explicitly defaulted so the erasing destructor below does not suppress the implicit move
        // operations. A suppressed move silently becomes a copy, which would leave the source
        // object holding a live password.
        Rfc2898DeriveBytes(const Rfc2898DeriveBytes&) = default;
        Rfc2898DeriveBytes(Rfc2898DeriveBytes&&) = default;
        Rfc2898DeriveBytes& operator=(const Rfc2898DeriveBytes&) = default;
        Rfc2898DeriveBytes& operator=(Rfc2898DeriveBytes&&) = default;

        /** @brief Erases the password, salt and derived-byte buffer from the storage this instance owns. */
        ~Rfc2898DeriveBytes() override;

        /** @return The hash algorithm used for byte derivation. */
        [[nodiscard]] const HashAlgorithmName& getHashAlgorithmProperty() const { return hashAlgorithm_; }

        /** @return The number of PBKDF2 iterations. */
        [[nodiscard]] intcs getIterationCountProperty() const { return static_cast<intcs>(iterations_); }
        /**
         * @brief Sets the number of PBKDF2 iterations and resets the derivation state.
         * @throws ObjectDisposedException if this instance has been disposed.
         */
        void setIterationCountProperty(intcs value);

        /**
         * @return A copy of the salt (without the internal block counter).
         * @throws ObjectDisposedException if this instance has been disposed.
         */
        [[nodiscard]] std::vector<bytecs> getSaltProperty() const;
        /**
         * @brief Sets the salt and resets the derivation state.
         * @throws ObjectDisposedException if this instance has been disposed.
         */
        void setSaltProperty(std::vector<bytecs> value);

        /**
         * @return @p cb newly derived bytes, continuing from where the previous call left off.
         * @throws ObjectDisposedException if this instance has been disposed.
         */
        [[nodiscard]] std::vector<bytecs> GetBytes(intcs cb) override;

        /**
         * @brief Restarts derivation from the beginning.
         * @throws ObjectDisposedException if this instance has been disposed.
         */
        void Reset() override;

        /**
         * @brief Erases the password, salt and derived-byte buffer, and invalidates this instance.
         *
         * Idempotent. Matches current .NET, whose `Rfc2898DeriveBytes.Dispose(bool)` disposes its
         * HMAC and clears the buffered and salt state.
         */
        void Dispose() override;
    };

} // namespace System::Security::Cryptography
