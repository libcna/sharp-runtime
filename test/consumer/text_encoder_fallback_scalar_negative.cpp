// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2355: proves that a custom encoder fallback can no longer
// be written against the old `char` signatures.
//
// EncoderFallback::GetFallbackBytes and EncoderFallbackBuffer::Fallback took a `char`, so every
// scalar an encoding cannot represent -- which is by definition outside that encoding, and for
// ASCII and Latin-1 outside `char`'s useful range too -- reached a custom fallback narrowed.
// Measured, the narrowing was `static_cast<char>(scalar & 0x7F)`: U+1F600 arrived as the byte
// 0x00. Both shipped fallbacks ignore the argument, so no shipped result was wrong; it was a
// surface that could not express what a custom implementation needs.
//
// .NET's parameter is a `char` too, with a SECOND overload taking a surrogate PAIR, because a
// UTF-16 char cannot hold a supplementary scalar -- and it reassembles that pair into one integer
// before formatting its message (EncoderExceptionFallback.cs:53-59). This port has no pair to
// reassemble, so it carries the scalar directly and needs neither the second overload nor a
// second field.
//
// THE DANGEROUS SPELLING IS THE ONE WITHOUT `override`: it compiles, silently fails to override,
// and leaves the pure virtual unimplemented -- which the compiler then reports at the point of
// INSTANTIATION rather than at the declaration. Site 2 pins that.
//
// Every `#if SHARP_RUNTIME_NEGATIVE_SITE == N` block below must be REJECTED by the compiler;
// with no site selected the file compiles cleanly.
// scripts/check_negative_consumer_fixtures.py compiles each site on its own and asserts the
// rejection; the record is docs/NegativeConsumerFixtureValidation.md.
//
// Migration: change `char` to `char32_t` in the override, and stop narrowing at the call site.
//
// NEGATIVE-FIXTURE: component=Text
#include "System/Text/EncoderFallback.hpp"

#include <memory>
#include <vector>

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
class NarrowFallback final : public System::Text::EncoderFallback {
public:
    // NEGATIVE(encoder-fallback-char-override): marked 'override', but does not override
    //     | does not override
    //     | marked override
    [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const override {
        return {};
    }
    [[nodiscard]] std::unique_ptr<System::Text::EncoderFallbackBuffer> CreateFallbackBuffer()
        const override {
        return nullptr;
    }
    [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override { return 1; }
};
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
// The spelling most likely to survive a careless migration: no `override`, so the declaration
// itself is legal and the class is merely still abstract.
class SilentlyNotOverriding final : public System::Text::EncoderFallback {
public:
    [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char) const { return {}; }
    [[nodiscard]] std::unique_ptr<System::Text::EncoderFallbackBuffer> CreateFallbackBuffer()
        const override {
        return nullptr;
    }
    [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override { return 1; }
};
void instantiate() {
    // NEGATIVE(encoder-fallback-char-abstract): abstract type
    //     | is abstract
    //     | pure within
    SilentlyNotOverriding f;
    (void)f;
}
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
class NarrowBuffer final : public System::Text::EncoderFallbackBuffer {
public:
    // NEGATIVE(encoder-fallback-buffer-char-override): marked 'override', but does not override
    //     | does not override
    //     | marked override
    bool Fallback(char, SharpRuntime::intcs) override { return false; }
    char GetNextChar() override { return '\0'; }
    bool MovePrevious() override { return false; }
    [[nodiscard]] SharpRuntime::intcs getRemainingProperty() const override { return 0; }
};
#endif

// The migrated spellings, which must all compile -- and which the clean baseline needs, so that a
// per-site verdict can be attributed to the site rather than to this file.
class WideFallback final : public System::Text::EncoderFallback {
public:
    [[nodiscard]] std::vector<SharpRuntime::bytecs> GetFallbackBytes(char32_t) const override {
        return {};
    }
    [[nodiscard]] std::unique_ptr<System::Text::EncoderFallbackBuffer> CreateFallbackBuffer()
        const override {
        return nullptr;
    }
    [[nodiscard]] SharpRuntime::intcs getMaxByteCountProperty() const override { return 1; }
};

void useMigrated() {
    WideFallback f;
    (void)f.GetFallbackBytes(U'\U0001F600');
}
