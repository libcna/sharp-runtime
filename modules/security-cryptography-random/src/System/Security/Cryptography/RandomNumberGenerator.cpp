// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"
#include "System/Security/Cryptography/CryptographicException.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__ANDROID__)
#include <cstdlib>
#else
#include <cerrno>
#include <cstring>
#include <unistd.h>
#endif

namespace System::Security::Cryptography {

namespace {

    class OsRandomNumberGenerator : public RandomNumberGenerator {
    public:
        // THREE ARMS, NOT FOUR, AND THAT IS THE REPAIR (#2398 plus SAMPLE-152).
        //
        // This body used to have four: Windows, an EMSCRIPTEN arm that THREW
        // `PlatformNotSupportedException`, a Linux `getrandom()` arm, and a
        // `getentropy()` arm for everything else. The Emscripten arm sat above a
        // comment reading "No secure random source wired up under Emscripten yet",
        // and THAT PREMISE WAS MEASURED FALSE BY #2228 IN THIS SAME REPOSITORY:
        // Emscripten's libc declares `getentropy()` in `<unistd.h>` and implements
        // it as `__wasi_random_get()`
        // (`system/lib/libc/musl/src/misc/getentropy.c`), backed by the host's
        // `crypto.getRandomValues`. So `System::Guid::NewGuid()` has been getting
        // real entropy on Emscripten through exactly this call while the type whose
        // whole purpose is cryptographic randomness refused to make it -- two
        // answers to one question inside one runtime.
        //
        // .NET does not refuse either: `RandomNumberGeneratorImplementation.Browser.cs`
        // forwards to `Interop.GetCryptographicallySecureRandomBytes`, whose
        // `__EMSCRIPTEN__` arm is `SystemJS_RandomBytes`
        // (`src/native/minipal/random.c:83-93`).
        //
        // Collapsing to `Guid.cpp`'s shared platform shape does more than delete the throw.
        // `getrandom()` is Linux-only (undeclared on Apple/BSD; Emscripten declares
        // it but backs `getentropy` with `__wasi_random_get`), so the old file had
        // ONE arm per platform and the Linux gate compiled only one of them. With a
        // shared Unix arm, THE CODE EMSCRIPTEN TAKES IS THE CODE LINUX TAKES,
        // so the full gate exercises it on every run -- an unverifiable platform arm
        // becomes a verified one, which is the point rather than a side effect.
        //
        // `getentropy()`'s hard 256-byte-per-call maximum is a documented `EIO`
        // rather than a silent truncation ("the maximum buffer size permitted is 256
        // bytes"), which is why the loop chunks for it.
        //
        // WHERE THIS DELIBERATELY DIFFERS FROM `Guid.cpp`: failure THROWS here.
        // `Guid::NewGuid()` retries instead, because callers treat it as infallible
        // and an escaping exception would reach `std::terminate`. This member has no
        // such constraint and .NET throws too -- `Interop.GetRandomBytes.cs:22-26`
        // is `if (Sys.GetCryptographicallySecureRandomBytes(...) != 0) throw new
        // CryptographicException();`. Neither one ever falls back to a weaker
        // source, which is the property both exist to hold.
        void GetBytes(std::vector<bytecs>& data) override {
            if (data.empty()) return;
#if defined(_WIN32)
            NTSTATUS status = BCryptGenRandom(nullptr, data.data(), static_cast<ULONG>(data.size()),
                                               BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (status < 0) {
                throw CryptographicException("BCryptGenRandom failed.");
            }
#elif defined(__ANDROID__)
            // Bionic's CSPRNG is available at every supported Android API
            // level; getentropy() is declared only from API 28 onward.
            ::arc4random_buf(data.data(), data.size());
#else
            constexpr size_t maxChunk = 256;
            size_t total = 0;
            while (total < data.size()) {
                const size_t remaining = data.size() - total;
                const size_t chunk = remaining < maxChunk ? remaining : maxChunk;
                if (::getentropy(data.data() + total, chunk) != 0) {
                    if (errno == EINTR) continue;
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
