// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/StreamReader.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileAccess.hpp"

namespace System::IO
{
    // Verified against StreamReader.cs, whose every Stream-taking constructor opens with
    // ArgumentNullException.ThrowIfNull(stream): a null base stream is rejected at
    // construction, not carried. This port stored it unvalidated and then defended against
    // it at each use, so StreamReader did not crash the way its StreamWriter counterpart
    // did -- it reported end-of-stream instead. That is worse than a crash in one specific
    // way: Read() and Peek() returned -1, ReadLine() and ReadToEnd() returned "", and a
    // caller could not distinguish "there was no stream" from "the document was empty",
    // so a programming error was silently laundered into ordinary, plausible data
    // (build-probe/1806_prefix_defects.log cases 1-4, ticket #1806 / SR-AUD-338). The
    // sibling BinaryReader in this same module already threw ArgumentNullException("stream")
    // for the identical input; the parameter name here matches it and .NET's own
    // nameof(stream).
    //
    // With this check in place stream_ is non-null for the whole lifetime of every
    // StreamReader -- the only other constructor assigns a freshly allocated FileStream,
    // and nothing else ever writes the member -- so the null tests that used to guard
    // Peek(), Read(), Close() and the destructor are gone rather than left behind as
    // unreachable code implying a state that can no longer exist.
    // The second check is ticket #1808, and it is the SAME laundering defect one level
    // further out. Verified against StreamReader.cs:145-148, whose constructor follows its
    // null check with "if (!stream.CanRead) throw new ArgumentException(
    // SR.Argument_StreamNotReadable);" -- message only, no paramName, which is why this one
    // has none either. A stream that declares itself unreadable can only ever answer -1 and
    // "", so before this check a StreamReader over, say, FileStream(path, FileMode::Append)
    // reported an empty document rather than an unusable stream (ticket #1823's measurement,
    // build-probe/1823_prefix_defects.log cases 6 and 7). The sibling BinaryReader in this
    // same module already threw exactly this exception with exactly this message for exactly
    // this input; the two now agree.
    //
    // Order matters and is .NET's: null first. Testing getCanReadProperty() before the null
    // check would dereference the pointer that the null check exists to reject.
    //
    // Only the READER half is here. The matching StreamWriter guard is ticket #1824 and is
    // BLOCKED on explicit approval, because System::IO::Stream::getCanWriteProperty()
    // defaults to false where .NET's Stream.CanWrite is abstract: a custom stream that
    // implements Write() without overriding the property reports CanWrite=false and yet
    // works today (case 8 writes "hello" successfully), so the writer guard would reject
    // streams that are in fact usable. getCanReadProperty() defaults to TRUE, so this check
    // rejects only streams that positively declare themselves unreadable, which is why it
    // needed no approval. The full analysis is docs/TextWrapperInputContractPlan.md §5.
    StreamReader::StreamReader(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false)
    {
        if (stream == nullptr) throw System::ArgumentNullException("stream");
        if (!stream->getCanReadProperty())
            throw System::ArgumentException("Stream was not readable.");
    }

    StreamReader::StreamReader(const std::string& path)
        : stream_(new FileStream(path, FileMode::Open, FileAccess::Read)),
          leaveOpen_(false), ownsStream_(true)
    {
    }

    // The `!closed_` term is .NET's `if (_disposed) return;` early exit (StreamReader.cs:245-248)
    // expressed on the destructor side: once Close() has run, the stream has already been closed
    // exactly once and must not be closed a second time here. Without it, an explicit Close()
    // followed by ordinary destruction closed the same stream twice.
    StreamReader::~StreamReader()
    {
        if (!leaveOpen_ && !closed_) stream_->Close();
        if (ownsStream_) delete stream_;
    }

    void StreamReader::ThrowIfClosed() const
    {
        if (closed_)
            throw System::ObjectDisposedException("StreamReader", "Cannot read from a closed TextReader.");
    }

    intcs StreamReader::Peek()
    {
        ThrowIfClosed();
        if (hasPeeked_) return static_cast<intcs>(peeked_);

        bytecs b;
        const intcs n = stream_->Read(&b, 0, 1);
        if (n == 0) return -1;

        peeked_ = b;
        hasPeeked_ = true;
        return static_cast<intcs>(b);
    }

    intcs StreamReader::Read()
    {
        ThrowIfClosed();
        if (hasPeeked_)
        {
            hasPeeked_ = false;
            return static_cast<intcs>(peeked_);
        }

        bytecs b;
        const intcs n = stream_->Read(&b, 0, 1);
        return n == 0 ? -1 : static_cast<intcs>(b);
    }

    // Verified against StreamReader.cs's ReadLine(): real .NET treats '\r' and '\n' as
    // interchangeable line terminators -- a lone '\r' (classic Mac line ending) ends the line
    // on its own, not just as part of "\r\n". If '\r' is immediately followed by '\n', that
    // '\n' is consumed as part of the same terminator (CRLF); a '\r' not followed by '\n'
    // still terminates the line by itself. This previously only stopped scanning at '\n', so a
    // lone '\r' was treated as ordinary line content -- silently merging what should be two
    // separate lines into one, with the '\r' left embedded in the middle of the result.
    std::string StreamReader::ReadLine()
    {
        // Guarded at entry, not left to the Read() below: .NET checks in every member
        // (StreamReader.cs:1281, 1299 and their siblings), and a guard that fired only part-way
        // through would already have mutated this reader's state.
        ThrowIfClosed();
        intcs c = Read();
        if (c == -1) return "";

        std::string line;
        while (c != -1 && c != '\n' && c != '\r')
        {
            line.push_back(static_cast<char>(c));
            c = Read();
        }
        if (c == '\r' && Peek() == '\n') Read();
        return line;
    }

    std::string StreamReader::ReadToEnd()
    {
        ThrowIfClosed();
        std::string result;
        if (hasPeeked_)
        {
            result.push_back(static_cast<char>(peeked_));
            hasPeeked_ = false;
        }

        intcs c;
        while ((c = Read()) != -1) result.push_back(static_cast<char>(c));
        return result;
    }

    void StreamReader::Close()
    {
        // Verified against StreamReader.cs: Close() delegates to Dispose(true), which checks
        // _closable (i.e. !leaveOpen) before closing the underlying stream -- matching this
        // type's own destructor (above), which already got this right. This method previously
        // closed the stream unconditionally, defeating leaveOpen's entire purpose for any
        // caller that called Close() explicitly instead of only relying on the destructor.
        //
        // Ticket #2098 / SR-AUD-337, reader half. `closed_` is set FIRST and UNCONDITIONALLY,
        // because that is where StreamReader.cs puts it: `Dispose(bool)` opens with
        //
        //     if (_disposed) { return; }
        //     _disposed = true;
        //     if (_closable) { ... _stream.Close(); ... }
        //
        // (StreamReader.cs:243-268). `leaveOpen` therefore decides the STREAM's fate only; the
        // reader is disposed either way. Note that the sibling StreamWriter does NOT share this
        // shape -- see StreamWriter::Close() -- and the difference is .NET's, not this port's.
        if (closed_) return;
        closed_ = true;
        if (!leaveOpen_) stream_->Close();
    }
}
