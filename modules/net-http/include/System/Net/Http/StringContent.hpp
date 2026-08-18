// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/detail/HttpFieldValidation.hpp"
#include "System/Text/Encoding.hpp"
#include <memory>
#include <string>
#include <vector>

namespace System::Net::Http {

/** HTTP content backed by a plain text string, mirroring .NET System.Net.Http.StringContent. */
class StringContent : public HttpContent {
    std::vector<SharpRuntime::bytecs> bytes_;
    std::string                       mediaType_;
    std::string                       charset_;
public:
    /**
     * @brief Constructs StringContent from a plain text string.
     * @param content   The text body.
     * @param encoding  The encoding to serialise @p content through; `nullptr` means UTF-8.
     * @param mediaType MIME type; defaults to `"text/plain"`.
     *
     * @throws System::FormatException if @p mediaType or the encoding's web name contains a
     * carriage return, a line feed or a NUL character.
     *
     * @note **Ticket #2070 (2026-08-18) changed this parameter from a charset STRING to an
     * `Encoding`, which is a public source break.** The old signature let the declared charset
     * and the emitted bytes contradict each other: `StringContent("\xc3\xa9", "utf-16")`
     * labelled its body `charset=utf-16` and emitted the two UTF-8 bytes `c3 a9`, so a
     * conforming server decoded them as a single UTF-16 code unit and got `U+A9C3` — a wrong
     * character, silently, with no diagnostic anywhere.
     *
     * .NET makes that state **unrepresentable** rather than validating against it. Its
     * constructor takes an `Encoding`, serialises through it
     * (`GetContentByteArray`, StringContent.cs:90-98) and then labels the header with that same
     * object's `WebName` (`:73`). One source of truth, so the two cannot disagree. `null` means
     * UTF-8, which is `encoding ??= DefaultStringEncoding` at `:53`.
     *
     * @note **Narrowing since ticket #2063** (SR-AUD-313, cause NH-B). The media type and the
     * charset are concatenated into a `Content-Type: <mediaType>; charset=<charset>` field — by
     * `HttpClientHandler::Send` on the wire and by `MultipartContent::ReadAsString` inside a
     * MIME part — so a CR/LF in either used to emit extra header fields. The **body** is not
     * validated: it is payload, not a protocol field. The charset can no longer carry one at
     * all, since it now comes from an `Encoding`'s own web name, but the check is kept: it
     * costs nothing and a future encoding is not required to have a well-formed name.
     */
    explicit StringContent(const std::string&                             content,
                           const std::shared_ptr<System::Text::Encoding>& encoding = nullptr,
                           const std::string&                             mediaType = "text/plain")
        : mediaType_(mediaType) {
        const auto& effective = encoding ? encoding : System::Text::Encoding::UTF8();
        charset_ = effective->getWebNameProperty();
        detail::ThrowIfControlCharacter(charset_, "charset");
        detail::ThrowIfControlCharacter(mediaType, "media type");
        bytes_ = effective->GetBytes(content);
    }

    /**
     * @brief Returns the encoded body as a string of raw bytes.
     *
     * Under a non-UTF-8 encoding these are **not** UTF-8 storage bytes and must not be treated
     * as text: `ReadAsString()` on a UTF-16 body returns the UTF-16 octets. That is the same
     * thing .NET's `ByteArrayContent.ReadAsStringAsync` would do without a charset to decode by,
     * and it is why `ReadAsByteArray()` is the honest accessor for a non-UTF-8 body.
     */
    [[nodiscard]] std::string ReadAsString() const override {
        return std::string(bytes_.begin(), bytes_.end());
    }

    /** Returns the content body as the raw bytes of the declared charset. */
    [[nodiscard]] std::vector<SharpRuntime::bytecs> ReadAsByteArray() const override {
        return bytes_;
    }

    /** Returns the MIME type of the content (e.g. "text/plain"). */
    [[nodiscard]] std::string getContentTypeProperty() const override { return mediaType_; }
    /** Returns the character set of the content — the encoding's own web name, always. */
    [[nodiscard]] std::string getCharSetProperty()     const override { return charset_; }
};

} // namespace System::Net::Http
