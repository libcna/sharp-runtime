// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "System/IO/TextWriter.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    /**
     * @brief Implements a TextWriter for writing characters to a stream in a
     * particular encoding (default: UTF-8).
     *
     * Partial C++ counterpart of .NET System.IO.StreamWriter.
     *
     * @note Status: Implemented
     */
    class StreamWriter : public TextWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;
        bool    ownsStream_ = false;
        /**
         * Records the closed state -- .NET's own `_disposed` field (StreamWriter.cs:42).
         * Lands in this type's existing tail padding, so `sizeof(StreamWriter)` is unchanged
         * at 24 (ticket #2098, Approval IO-1 under `docs/StandingApprovals.md` SA-3).
         *
         * Unlike StreamReader's flag, this one is set **only when the writer owns the close**
         * -- see Close(). That asymmetry is .NET's.
         */
        bool    closed_ = false;

        /**
         * Throws `ObjectDisposedException("StreamWriter", "Cannot write to a closed
         * TextWriter.")`, matching StreamWriter.cs:1008-1015, whose `ThrowIfDisposed()` passes
         * `GetType().Name` as the object name -- unlike `StringWriter`, which passes null.
         */
        void ThrowIfClosed() const;

        void WriteRaw(const char* data, size_t len);

    public:
        /**
         * @brief Constructs a StreamWriter wrapping the given stream.
         *
         * A null @p stream is rejected here rather than carried. With the
         * default `leaveOpen = false` it was previously fatal merely to
         * construct such a writer and let it leave scope, because the
         * destructor closed the stream it did not have. This matches .NET's
         * own constructors and the sibling BinaryWriter in this module.
         *
         * @param stream Stream to write to. Must not be null.
         * @param leaveOpen If false (the default), the stream is closed when this StreamWriter is destroyed.
         * @throws System::ArgumentNullException if @p stream is null.
         */
        explicit StreamWriter(Stream* stream, bool leaveOpen = false);
        /** Constructs a StreamWriter that writes to a new or truncated file at path. */
        explicit StreamWriter(const std::string& path);
        /** Destroys the StreamWriter and closes the underlying stream if not leaveOpen. */
        ~StreamWriter() override;

        /** Returns the underlying stream. */
        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        using TextWriter::Write;
        using TextWriter::WriteLine;

        /**
         * @brief Writes a string to the stream.
         * @throws System::ObjectDisposedException if the writer has been closed -- which,
         *         per Close(), happens only when this writer was NOT constructed with
         *         leaveOpen.
         */
        void Write(const std::string& value) override;
        /**
         * @brief Writes a null-terminated character array to the stream.
         *
         * A null @p value writes nothing and returns normally, matching
         * TextWriter::Write(const char*) and .NET's own treatment of a null string
         * (TextWriter.cs:277-283). @see TextWriter::Write(const char*)
         *
         * @param value Null-terminated string to write, or null to write nothing.
         * @throws System::ObjectDisposedException if the writer has been closed and @p value
         *         is not null. A null @p value writes nothing and throws nothing, because
         *         .NET's own `Write(string?)` returns before reaching its disposal check
         *         (TextWriter.cs:277-283).
         */
        void Write(const char* value) override;

        /**
         * @brief Flushes any buffered data to the underlying stream.
         * @throws System::ObjectDisposedException if the writer has been closed
         *         (StreamWriter.cs:283, whose Flush opens with ThrowIfDisposed()).
         */
        void Flush() override;
        /**
         * @brief Closes the underlying stream, unless this writer was constructed with leaveOpen.
         *
         * <b>With leaveOpen the writer stays usable, and that is .NET's behaviour, not a
         * defect.</b> StreamWriter.cs sets `_disposed = true` only inside
         * `CloseStreamFromDispose`, under `if (_closable && !_disposed)`
         * (StreamWriter.cs:221-244), and `_closable` is `!leaveOpen` (StreamWriter.cs:129). So a
         * leaveOpen writer is never marked disposed and a Write after Close() legitimately
         * succeeds. The sibling StreamReader is different -- it marks itself disposed
         * unconditionally -- and the two disagreeing is upstream's choice.
         *
         * Ticket #2098 / SR-AUD-337, writer half. The finding reported the leaveOpen writer's
         * post-Close growth as a divergence; measured against the reference it is **not** one,
         * so what #2098 repairs here is the non-leaveOpen case, where a Write after Close()
         * used to reach a closed stream instead of throwing ObjectDisposedException.
         */
        void Close() override;
    };

} // namespace System::IO
