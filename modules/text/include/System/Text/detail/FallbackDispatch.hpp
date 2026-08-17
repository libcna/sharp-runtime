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
     * @note **The scalar is narrowed to a `char`**, because that is what
     *       `EncoderFallback::GetFallbackBytes` accepts. For the replacement fallbacks — the
     *       default everywhere, and the only kind this component ships that inspects nothing —
     *       the argument is unused, so the narrowing changes no result. A custom fallback that
     *       wanted to see *which* scalar failed cannot, and widening that signature is a change
     *       to a public virtual, which is recorded as ticket **#2355** rather than smuggled in
     *       here.
     */
    inline void AppendEncoderFallback(std::vector<SharpRuntime::bytecs>& out, const Encoding& owner,
                                      std::uint32_t unencodableScalar) {
        const auto fallback = owner.getEncoderFallbackProperty();
        const auto bytes = fallback->GetFallbackBytes(static_cast<char>(unencodableScalar & 0x7F));
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

} // namespace System::Text::detail
