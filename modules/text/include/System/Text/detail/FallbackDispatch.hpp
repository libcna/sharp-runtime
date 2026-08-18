// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Text/DecoderFallback.hpp"
#include "System/Text/EncoderFallback.hpp"

namespace System::Text::detail {

    /**
     * @brief Routes an undecodable byte run through the configured decoder fallback.
     *
     * Ticket #2017 (SR-AUD-292/293). Before it, only `UTF8Encoding` consulted its fallback:
     * `UnicodeEncoding`, `UTF32Encoding`, `ASCIIEncoding` and `Latin1Encoding` substituted
     * `U+FFFD` or `'?'` **directly**, so a caller who installed
     * `DecoderFallback::ExceptionFallback()` on any of them got silence where .NET throws — a
     * configured policy that was accepted, stored, and then ignored.
     *
     * Every one of them now comes through here, so there is one place where "what happens to a
     * byte we cannot decode" is decided.
     *
     * @param out    Receives the fallback's replacement text, appended.
     * @param owner  The encoding whose configured fallback governs.
     * @param bytes  First byte of the undecodable run.
     * @param count  Its length. .NET passes the whole ill-formed run, not just its first byte.
     */
    inline void AppendDecoderFallback(std::string& out, const Encoding& owner,
                                      const SharpRuntime::bytecs* bytes, std::size_t count) {
        const auto fallback = owner.getDecoderFallbackProperty();
        out += fallback->GetFallbackString(bytes, static_cast<SharpRuntime::intcs>(count));
    }

    /**
     * @brief Routes an unencodable scalar through the configured encoder fallback.
     *
     * @note Ticket **#2355** widened `GetFallbackBytes` to take a `char32_t`, so the scalar
     *       reaches the fallback intact. It used to be narrowed with
     *       `static_cast<char>(scalar & 0x7F)` — which for `U+1F600` handed the fallback the
     *       byte `0x00`, and for every non-ASCII scalar handed it something that was not the
     *       character at all. No shipped result moved, because both shipped fallbacks ignore the
     *       argument, but a custom fallback could not see what it was being asked about.
     */
    inline void AppendEncoderFallback(std::vector<SharpRuntime::bytecs>& out, const Encoding& owner,
                                      std::uint32_t unencodableScalar) {
        const auto fallback = owner.getEncoderFallbackProperty();
        const auto bytes = fallback->GetFallbackBytes(static_cast<char32_t>(unencodableScalar));
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

} // namespace System::Text::detail
