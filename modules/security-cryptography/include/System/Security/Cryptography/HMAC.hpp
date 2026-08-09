// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "System/Security/Cryptography/KeyedHashAlgorithm.hpp"
#include "System/Security/Cryptography/detail/SecureMemory.hpp"

namespace System::Security::Cryptography {

    /**
     * @brief Computes a Hash-based Message Authentication Code (HMAC, RFC 2104) using a
     * configurable underlying hash algorithm.
     *
     * C++ counterpart of .NET System.Security.Cryptography.HMAC.
     *
     * @note .NET's own `HMAC` base class is a near-empty shell whose `HashCore`/`HashFinal`/
     * `Initialize` all throw `PlatformNotSupportedException` — the real HMAC construction lives
     * in an internal `HMACCommon` type used only by the concrete `HMACMD5`/`HMACSHA1`/etc.
     * subclasses (both marked `ignore`/not applicable in this port). Since splitting that
     * internal plumbing out as a separate C++ type would add complexity with no benefit, this
     * class implements the real RFC 2104 construction directly: `HMAC(K,m) =
     * H((K' ^ opad) || H((K' ^ ipad) || m))`, parameterized by an inner-hash factory and block
     * size supplied by each concrete subclass's constructor.
     *
     * @note **Key-material contract.** `innerPad_` and `outerPad_` are `key' ^ 0x36` and
     * `key' ^ 0x5C`, so either one recovers the padded key with a single XOR — they are key
     * material, not a derived digest, and they are erased on the same terms as the key itself: on
     * Dispose(), on destruction, and on key replacement, always before the storage is released.
     * The working buffers built inside derivePads() and HashFinal() are erased before they die for
     * the same reason. What this does **not** cover — a caller's own copy of the key, the copy
     * getKeyProperty() hands out, growth reallocation inside `pendingMessage_`, register and stack
     * spills, and anything outside the process image — is stated in
     * `docs/SystemSecurityCryptographyNamespaceReviewPlan.md` section 6.
     */
    class HMAC : public KeyedHashAlgorithm {
        /** @brief Test-only access seam; see `detail/SecureMemory.hpp`. Never defined in production. */
        template <typename>
        friend struct SharpRuntime::Testing::KeyMaterialAccess;

        std::function<std::unique_ptr<HashAlgorithm>()> createHash_;
        intcs blockSizeValue_;
        std::vector<bytecs> innerPad_;
        std::vector<bytecs> outerPad_;
        std::vector<bytecs> pendingMessage_;
        std::string hashName_;

        void derivePads();

    protected:
        HMAC(std::function<std::unique_ptr<HashAlgorithm>()> createHash, intcs blockSizeValue, std::string hashName,
             std::vector<bytecs> key)
            : createHash_(std::move(createHash)), blockSizeValue_(blockSizeValue), hashName_(std::move(hashName)) {
            keyValue_ = std::move(key);
            derivePads();
        }

        void HashCore(const std::vector<bytecs>& array, intcs offset, intcs count) override;
        std::vector<bytecs> HashFinal() override;

        /** @brief Erases the key-derived pads and any buffered message. */
        void clearDerivedState() {
            SharpRuntimeDetail::SecureMemory::ClearContainer(innerPad_);
            SharpRuntimeDetail::SecureMemory::ClearContainer(outerPad_);
            SharpRuntimeDetail::SecureMemory::ClearContainer(pendingMessage_);
        }

    public:
        // See KeyedHashAlgorithm: explicitly defaulted so the erasing destructor below does not
        // suppress the implicit move operations and silently turn a move into a copy.
        HMAC(const HMAC&) = default;
        HMAC(HMAC&&) = default;
        HMAC& operator=(const HMAC&) = default;
        HMAC& operator=(HMAC&&) = default;

        /** @brief Erases the key-derived pads and buffered message from the storage this instance owns. */
        ~HMAC() override { clearDerivedState(); }

        /** @return The block size, in bytes, of the underlying hash algorithm. */
        [[nodiscard]] intcs getBlockSizeValueProperty() const { return blockSizeValue_; }

        /** @return The name of the underlying hash algorithm (e.g. "SHA256"). */
        [[nodiscard]] const std::string& getHashNameProperty() const { return hashName_; }

        [[nodiscard]] std::vector<bytecs> getKeyProperty() const override { return keyValue_; }

        /** @brief Sets a new key, erasing the previous key and pads, and re-deriving the pads. */
        void setKeyProperty(std::vector<bytecs> value) override {
            SharpRuntimeDetail::SecureMemory::Scrub(innerPad_);
            SharpRuntimeDetail::SecureMemory::Scrub(outerPad_);
            KeyedHashAlgorithm::setKeyProperty(std::move(value));
            derivePads();
        }

        void Initialize() override { SharpRuntimeDetail::SecureMemory::ClearContainer(pendingMessage_); }

        /** @brief Erases the key and the key-derived pads, and releases resources used by this instance. */
        void Dispose() override {
            clearDerivedState();
            KeyedHashAlgorithm::Dispose();
        }
    };

} // namespace System::Security::Cryptography
