// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a reader that can read a sequential series of characters.
     *
     * Partial C++ counterpart of .NET System.IO.TextReader.
     *
     * @note Status: Partial
     */
    class TextReader {
    public:
        /** Destroys the TextReader. */
        virtual ~TextReader() = default;

        /** @brief Reads the next character without changing the state of the reader. Returns -1 at end. */
        [[nodiscard]] virtual intcs Peek() { return -1; }

        /** @brief Reads the next character. Returns -1 at end. */
        virtual intcs Read() { return -1; }

        /** @brief Reads a line of characters. Returns empty string at end-of-stream. */
        [[nodiscard]] virtual std::string ReadLine() { return ""; }

        /** @brief Reads all characters from the current position to the end. */
        [[nodiscard]] virtual std::string ReadToEnd() { return ""; }

        /**
         * @brief Closes the reader.
         *
         * <b>This base implementation does nothing, and no derived type in this port is made
         * unusable by it.</b> A closed TextReader keeps reading: StringReader does not override
         * this at all, so after Close() its Peek/Read/ReadToEnd continue from wherever the reader
         * had got to. That is SR-AUD-343, and it is still open. Enforcing the closed state needs
         * somewhere to record it, and every option is an object-layout change in a public type
         * -- a flag in each leaf, or a flag here in the base, which relayouts every derived type
         * at once. The decision is ticket #2098, BLOCKED on Approval IO-1
         * (docs/SystemIONamespaceReviewPlan.md section 21). Until it is taken, do not rely on
         * Close() to make a reader stop working; the layouts this base and its subclasses have
         * today are pinned by test so the approved option's cost stays the costed one.
         */
        virtual void Close() {}
    };

} // namespace System::IO
