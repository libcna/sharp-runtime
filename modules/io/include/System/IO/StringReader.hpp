// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IO/TextReader.hpp"
#include "System/ObjectDisposedException.hpp"

namespace System::IO {

    /**
     * @brief Implements a TextReader that reads from a string.
     *
     * Partial C++ counterpart of .NET System.IO.StringReader.
     *
     * @note Status: Implemented
     */
    class StringReader : public TextReader {
        std::string s_;
        intcs pos_ = 0;
        /**
         * Records the closed state, so Close() is a real disposal contract rather than the
         * inherited no-op of SR-AUD-343. .NET spells the same state as `_s = null`
         * (StringReader.cs:33), which a std::string cannot express; a private bool is this
         * port's equivalent and lands in the type's existing tail padding, so
         * `sizeof(StringReader)` is unchanged at 48 (ticket #2098, Approval IO-1 under
         * `docs/StandingApprovals.md` SA-3).
         */
        bool closed_ = false;

        /**
         * .NET throws `ObjectDisposedException(null, SR.ObjectDisposed_ReaderClosed)` from
         * `StringReader.ThrowObjectDisposedException_ReaderClosed()` (StringReader.cs:325-328) --
         * objectName is deliberately **null**, so the message stands alone with no
         * "Object name:" suffix. An empty objectName is this port's spelling of that null.
         */
        void ThrowIfClosed() const {
            if (closed_) throw System::ObjectDisposedException(std::string(),
                                                              "Cannot read from a closed TextReader.");
        }

    public:
        /** Constructs a StringReader over the given string. */
        explicit StringReader(const std::string& s) : s_(s) {}

        /**
         * @brief Returns the next character without advancing the position, or -1 at end.
         * @throws System::ObjectDisposedException if the reader has been closed.
         */
        [[nodiscard]] intcs Peek() override {
            ThrowIfClosed();
            if (pos_ >= static_cast<intcs>(s_.size())) return -1;
            return static_cast<unsigned char>(s_[pos_]);
        }

        /**
         * @brief Reads and returns the next character, or -1 at end.
         * @throws System::ObjectDisposedException if the reader has been closed.
         */
        intcs Read() override {
            ThrowIfClosed();
            if (pos_ >= static_cast<intcs>(s_.size())) return -1;
            return static_cast<unsigned char>(s_[pos_++]);
        }

        /**
         * @brief Reads the next line, stripping the line terminator.
         * @note Verified against StringReader.cs's ReadLine(): real .NET treats '\r' and '\n'
         * as interchangeable line terminators -- a lone '\r' (classic Mac line ending) ends
         * the line on its own, not just as part of "\r\n". If '\r' is immediately followed by
         * '\n', both are consumed as one terminator (CRLF); a '\r' not followed by '\n' still
         * terminates the line by itself. This previously only stopped scanning at '\n', so a
         * lone '\r' was treated as ordinary line content -- silently merging what should be
         * two separate lines into one, with the '\r' left embedded in the middle of the
         * result.
         *
         * @throws System::ObjectDisposedException if the reader has been closed.
         */
        [[nodiscard]] std::string ReadLine() override {
            ThrowIfClosed();
            if (pos_ >= static_cast<intcs>(s_.size())) return "";
            auto start = pos_;
            while (pos_ < static_cast<intcs>(s_.size()) && s_[pos_] != '\n' && s_[pos_] != '\r') ++pos_;
            std::string line = s_.substr(start, pos_ - start);
            if (pos_ < static_cast<intcs>(s_.size())) {
                bool isCrLf = s_[pos_] == '\r' && pos_ + 1 < static_cast<intcs>(s_.size()) && s_[pos_ + 1] == '\n';
                pos_ += isCrLf ? 2 : 1;
            }
            return line;
        }

        /**
         * @brief Reads all remaining text from the current position.
         * @throws System::ObjectDisposedException if the reader has been closed.
         */
        [[nodiscard]] std::string ReadToEnd() override {
            ThrowIfClosed();
            if (pos_ >= static_cast<intcs>(s_.size())) return "";
            std::string rest = s_.substr(pos_);
            pos_ = static_cast<intcs>(s_.size());
            return rest;
        }

        /**
         * @brief Closes the reader, after which every read member throws.
         *
         * Verified against StringReader.cs:31-36, whose `Dispose(bool)` sets `_s = null`
         * unconditionally; every read member then reaches
         * `ThrowObjectDisposedException_ReaderClosed()`. Closing twice is safe, exactly as
         * repeating `_s = null` is.
         *
         * Ticket #2098 / SR-AUD-343. Before it, this type inherited `TextReader::Close()`'s
         * no-op and kept reading from wherever it had got to.
         */
        void Close() override { closed_ = true; }
    };

} // namespace System::IO
