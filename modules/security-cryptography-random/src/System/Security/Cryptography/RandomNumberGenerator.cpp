// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/Security/Cryptography/CryptographicException.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__EMSCRIPTEN__)
// No secure random source wired up under Emscripten yet.
#else
#include <sys/random.h>
#include <cerrno>
#include <cstring>
#endif

namespace System::Security::Cryptography {

namespace {

    class OsRandomNumberGenerator : public RandomNumberGenerator {
    public:
        void GetBytes(std::vector<bytecs>& data) override {
            if (data.empty()) return;
#if defined(_WIN32)
            NTSTATUS status = BCryptGenRandom(nullptr, data.data(), static_cast<ULONG>(data.size()),
                                               BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (status < 0) {
                throw CryptographicException("BCryptGenRandom failed.");
            }
#elif defined(__EMSCRIPTEN__)
            // No secure random source wired up under Emscripten yet — throw a clear exception
            // rather than silently produce weak randomness (e.g. from an unseeded PRNG).
            (void)data;
            throw System::PlatformNotSupportedException(
                "RandomNumberGenerator is not implemented on Emscripten in this runtime.");
#elif defined(__linux__)
            size_t total = 0;
            while (total < data.size()) {
                ssize_t n = ::getrandom(data.data() + total, data.size() - total, 0);
                if (n < 0) {
                    if (errno == EINTR) continue;
                    throw CryptographicException(std::string("getrandom() failed: ") + std::strerror(errno));
                }
                total += static_cast<size_t>(n);
            }
#else
            // getrandom() is Linux-only (glibc 2.25+ wrapper around the Linux 3.17+ syscall) --
            // undeclared on Apple/BSD platforms entirely (confirmed via a real macOS CI build:
            // "no member named 'getrandom' in the global namespace"). BSD/Darwin's real
            // equivalent is getentropy() (also declared in <sys/random.h>, already included
            // above): same cryptographic-quality guarantee, but a simpler signature (no flags
            // parameter, no partial-read return value) and, critically, a hard 256-byte-per-call
            // maximum -- requesting more fails with EIO rather than silently truncating, per its
            // own man page ("the maximum buffer size permitted is 256 bytes"). Chunked here in
            // <=256-byte calls for exactly that reason.
            constexpr size_t maxChunk = 256;
            size_t total = 0;
            while (total < data.size()) {
                size_t remaining = data.size() - total;
                size_t chunk = remaining < maxChunk ? remaining : maxChunk;
                if (::getentropy(data.data() + total, chunk) != 0) {
                    throw CryptographicException(std::string("getentropy() failed: ") + std::strerror(errno));
                }
                total += chunk;
            }
#endif
        }
    };

} // namespace

std::shared_ptr<RandomNumberGenerator> RandomNumberGenerator::Create() {
    return std::make_shared<OsRandomNumberGenerator>();
}

void RandomNumberGenerator::Fill(std::vector<bytecs>& data) {
    static const std::shared_ptr<RandomNumberGenerator> singleton = Create();
    singleton->GetBytes(data);
}

} // namespace System::Security::Cryptography
