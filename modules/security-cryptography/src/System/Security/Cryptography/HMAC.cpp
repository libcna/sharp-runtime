// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Security/Cryptography/HMAC.hpp"

namespace System::Security::Cryptography {

void HMAC::derivePads() {
    const size_t block = static_cast<size_t>(blockSizeValue_);

    // One allocation for the whole derivation. Reserving the final size up front is what makes the
    // erasure below sufficient: neither the copy nor the resize can reallocate, and a reallocation
    // would hand the allocator a block still holding the key before anything could erase it.
    std::vector<bytecs> keyPrime;
    keyPrime.reserve(keyValue_.size() > block ? keyValue_.size() : block);
    keyPrime.assign(keyValue_.begin(), keyValue_.end());

    if (keyPrime.size() > block) {
        auto hasher = createHash_();
        std::vector<bytecs> hashedKey = hasher->ComputeHash(keyPrime);
        SharpRuntimeDetail::SecureMemory::Scrub(keyPrime);
        keyPrime.assign(hashedKey.begin(), hashedKey.end());
        SharpRuntimeDetail::SecureMemory::ClearContainer(hashedKey);
    }
    keyPrime.resize(block, bytecs{0});

    innerPad_.assign(block, bytecs{0});
    outerPad_.assign(block, bytecs{0});
    for (size_t i = 0; i < keyPrime.size(); ++i) {
        innerPad_[i] = static_cast<bytecs>(keyPrime[i] ^ 0x36);
        outerPad_[i] = static_cast<bytecs>(keyPrime[i] ^ 0x5C);
    }

    // keyPrime is the padded key. Without this, every construction and every key replacement hands
    // a block still containing it back to the allocator (SR-AUD-332, plan section 4.2 site 1).
    SharpRuntimeDetail::SecureMemory::Scrub(keyPrime);
}

void HMAC::HashCore(const std::vector<bytecs>& array, intcs offset, intcs count) {
    pendingMessage_.insert(pendingMessage_.end(), array.begin() + offset, array.begin() + offset + count);
}

std::vector<bytecs> HMAC::HashFinal() {
    const size_t block = static_cast<size_t>(blockSizeValue_);

    // Both working buffers begin with a key-derived pad, so both are key material for as long as
    // they exist. Reserved up front for the same reason as in derivePads: a growth reallocation
    // would release a block holding the pad before anything could erase it.
    std::vector<bytecs> innerInput;
    innerInput.reserve(block + pendingMessage_.size());
    innerInput.assign(innerPad_.begin(), innerPad_.end());
    innerInput.insert(innerInput.end(), pendingMessage_.begin(), pendingMessage_.end());
    auto inner = createHash_();
    std::vector<bytecs> innerDigest = inner->ComputeHash(innerInput);
    SharpRuntimeDetail::SecureMemory::Scrub(innerInput);

    std::vector<bytecs> outerInput;
    outerInput.reserve(block + innerDigest.size());
    outerInput.assign(outerPad_.begin(), outerPad_.end());
    outerInput.insert(outerInput.end(), innerDigest.begin(), innerDigest.end());
    auto outer = createHash_();
    std::vector<bytecs> result = outer->ComputeHash(outerInput);
    SharpRuntimeDetail::SecureMemory::Scrub(outerInput);
    SharpRuntimeDetail::SecureMemory::Scrub(innerDigest);
    return result;
}

} // namespace System::Security::Cryptography
