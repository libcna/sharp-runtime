// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/ASCIIEncoding.hpp"
#include "System/Text/detail/FallbackDispatch.hpp"
#include "System/Text/detail/Utf8Scalar.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"

namespace System::Text {

// #2014 moved this file's UTF-8 decode into System/Text/detail/Utf8Scalar.hpp. It had been
// duplicated five ways across modules/text, and Latin1Encoding needed a sixth; one shared body
// is what stops the copies drifting apart, which is the defect this repository keeps repairing.
using System::Text::detail::DecodeUtf8Scalar;


    /**
     * Encodes @p str (this runtime's UTF-8 representation) to ASCII bytes, one output byte
     * per Unicode UTF-16 code unit the string would have in real .NET -- not one per raw
     * UTF-8 byte. The previous implementation iterated the UTF-8-encoded input byte-wise, so
     * a single multi-byte non-ASCII character (e.g. 'é', 2 UTF-8 bytes) produced 2-4 '?'
     * replacement bytes instead of .NET's 1 (real ASCIIEncoding operates on `char[]`/UTF-16
     * code units, verified against ASCIIEncoding.cs). A BMP code point (including ASCII)
     * maps to exactly one output byte; a supplementary-plane code point (>= U+10000) maps to
     * two, matching the two UTF-16 surrogate halves .NET would encode it as (neither half is
     * ASCII-representable, so both are always '?').
     */
    std::vector<SharpRuntime::bytecs> ASCIIEncoding::GetBytes(const std::string& str) const {
        std::vector<SharpRuntime::bytecs> result;
        result.reserve(str.size());
        size_t i = 0;
        while (i < str.size()) {
            uint32_t cp;
            size_t len;
            DecodeUtf8Scalar(str, i, cp, len);
            i += len;
            if (cp <= 127) {
                result.push_back(static_cast<SharpRuntime::bytecs>(cp));
            } else if (cp < 0x10000) {
                // #2017: the CONFIGURED encoder fallback, not a hard-coded '?'. The default is
                // still the replacement fallback with "?", so nothing moves unless a caller
                // asked it to -- which is the whole complaint: a configured policy was accepted,
                // stored, and then ignored.
                detail::AppendEncoderFallback(result, *this, cp);
            } else {
                // #2355. This used to call the fallback TWICE for a supplementary scalar,
                // because #2017 was mimicking .NET's delivery of one through a surrogate PAIR --
                // the only shape a `char` parameter could express. Measured against the
                // reference, .NET calls the fallback ONCE for a pair
                // (`EncoderReplacementFallback.Fallback(high, low, index)` sets
                // `_fallbackCount = _strDefault.Length`, EncoderReplacementFallback.cs:117-138),
                // so `Encoding.ASCII.GetBytes("\U0001F600")` is ONE '?' and this port produced
                // two. The doubling existed only to work around the narrow parameter, and the
                // parameter is no longer narrow.
                detail::AppendEncoderFallback(result, *this, cp);
            }
        }
        return result;
    }

    std::string ASCIIEncoding::GetString(const SharpRuntime::bytecs* data,
                                         SharpRuntime::intcs index,
                                         SharpRuntime::intcs count) const {
        // Ticket #2007 (SR-AUD-286): the finding's own reproduction. `data[index + i]` with
        // index -1 read the byte before the caller's buffer -- an ASan-confirmed
        // stack-buffer-underflow that returned '?' for whatever it found there.
        const auto range = detail::checkedRawDecodeRange(data, index, count);
        if (!range.any()) return {};
        std::string result;
        result.reserve(range.end - range.begin);
        for (size_t i = range.begin; i < range.end; ++i) {
            const auto b = data[i];
            if (b <= 127) {
                result.push_back(static_cast<char>(b));
            } else {
                detail::AppendDecoderFallback(result, *this, data + i, 1);
            }
        }
        return result;
    }

} // namespace System::Text
