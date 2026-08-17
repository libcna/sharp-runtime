// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <sstream>
#include <string>
#include "System/IO/TextWriter.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::IO {

    /**
     * @brief Implements a TextWriter that writes to a string (via std::ostringstream).
     *
     * Partial C++ counterpart of .NET System.IO.StringWriter.
     *
     * @note Status: Implemented
     */
    class StringWriter : public TextWriter {
        std::ostringstream buf_;
        /**
         * Records the closed state. This is .NET's own `_isOpen` field (StringWriter.cs:16),
         * inverted to match this port's `closed_` spelling in the sibling wrappers.
         *
         * **This is the one type Approval IO-1 costs anything:** `std::ostringstream` fills all
         * 384 bytes, so the flag cannot land in tail padding and `sizeof(StringWriter)` grows
         * **384 -> 392**. Authorised under `docs/StandingApprovals.md` SA-3; every consumer
         * must be fully recompiled, per `docs/Migration-IOLifecycleAndArgumentStrictness.md`.
         * No vtable, mangled-symbol, signature or `noexcept` change is involved.
         */
        bool closed_ = false;

        /**
         * .NET throws `ObjectDisposedException(null, SR.ObjectDisposed_WriterClosed)` from every
         * `Write` overload (StringWriter.cs:73-75 and its eight siblings) -- objectName is
         * deliberately **null**, so the message stands alone. An empty objectName is this port's
         * spelling of that null.
         */
        void ThrowIfClosed() const {
            if (closed_) throw System::ObjectDisposedException(std::string(),
                                                              "Cannot write to a closed TextWriter.");
        }

    public:
        /** Constructs a StringWriter with an empty buffer. */
        StringWriter() = default;

        using TextWriter::Write;
        using TextWriter::WriteLine;

        /**
         * @brief Writes a string to the internal buffer.
         *
         * Every inherited `Write`/`WriteLine` overload funnels through this one, so closing the
         * writer disables all of them together -- which is what .NET achieves by repeating the
         * `_isOpen` check in each overload.
         *
         * @throws System::ObjectDisposedException if the writer has been closed.
         */
        void Write(const std::string& value) override { ThrowIfClosed(); buf_ << value; }

        /**
         * @brief Returns the string written so far.
         *
         * **Still works after Close(), deliberately.** StringWriter.cs:309-312 defines
         * `ToString()` as a bare `return _sb.ToString();` with no `_isOpen` check, so text
         * written before closing stays retrievable afterwards. Extending #2098's guard to this
         * member would be a divergence from .NET, not a repair.
         */
        [[nodiscard]] std::string ToString() const { return buf_.str(); }

        /**
         * @brief Returns the underlying string builder contents (alias for ToString).
         *
         * **Still works after Close()**, for the same reason: .NET's `GetStringBuilder()`
         * (StringWriter.cs:64-67) is a bare `return _sb;` with no `_isOpen` check.
         */
        [[nodiscard]] std::string GetStringBuilder() const { return buf_.str(); }

        /**
         * @brief Closes the writer, after which every write throws but the text stays readable.
         *
         * Verified against StringWriter.cs:49-55, whose `Dispose(bool)` sets `_isOpen = false`
         * and does **not** discard `_sb`. Closing twice is safe.
         *
         * Ticket #2098 / SR-AUD-343. Before it, this type inherited `TextWriter::Close()`'s
         * no-op and kept appending.
         */
        void Close() override { closed_ = true; }
    };

} // namespace System::IO
