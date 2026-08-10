// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/StreamWriter.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/IO/FileStream.hpp"

#include <cstring>

namespace System::IO {

    // Verified against StreamWriter.cs, whose every Stream-taking constructor opens with
    // ArgumentNullException.ThrowIfNull(stream): a null base stream is rejected at
    // construction, not carried. This port stored it unvalidated, which made FIVE separate
    // null dereferences reachable from public API -- Write(string), Write(const char*),
    // Flush(), Close() when leaveOpen is false, and ~StreamWriter() when leaveOpen is
    // false. The last of those is the sharpest: with the default leaveOpen=false, merely
    // constructing a StreamWriter over a null stream and letting it leave scope was fatal,
    // with no call on the object at all. All five are recorded with per-case AddressSanitizer
    // and UndefinedBehaviorSanitizer output in build-probe/1806_prefix_defects.log (ticket
    // #1806 / SR-AUD-338). The sibling BinaryWriter in this same module already threw
    // ArgumentNullException("stream") for the identical input, which is the inconsistency
    // the audit called especially hazardous; the parameter name here matches it and .NET's
    // own nameof(stream).
    // Ticket #1824 (REMED-IO-STREAMWRITER-DIRECTION, no SR-AUD-*): the CanWrite guard below
    // was blocked pending the shared Stream-capability approval, which was granted for the
    // decision recorded in docs/StreamCapabilityContractDesign.md section 6.2 -- .NET's rule
    // that a Stream which does not override getCanWriteProperty() is unwritable. StreamWriter.cs
    // opens every Stream-taking constructor with ArgumentNullException.ThrowIfNull(stream)
    // followed by `if (!stream.CanWrite) throw new ArgumentException(SR.Argument_StreamNotWritable)`
    // (StreamWriter.cs:135-146). The null check must run first: a null stream reports
    // ArgumentNullException, not the writability message. The message has NO (Parameter ...)
    // suffix because .NET's Argument_StreamNotWritable is a message-only ArgumentException with
    // no paramName -- the sibling BinaryWriter (BinaryWriter.cpp:17-19) already throws the
    // identical "Stream was not writable." for the identical input, and #1824 removes the
    // inconsistency that StreamWriter accepted what BinaryWriter rejected. Measured under #1839:
    // every in-repository StreamWriter(Stream*) site wraps a MemoryStream or FileStream, both of
    // which override the property, so this rejects no in-repository stream; a closed FileStream
    // is now correctly rejected too, since #1842 folds is_open() into its CanWrite.
    StreamWriter::StreamWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false)
    {
        if (stream == nullptr) throw System::ArgumentNullException("stream");
        if (!stream->getCanWriteProperty())
            throw System::ArgumentException("Stream was not writable.");
    }

    StreamWriter::StreamWriter(const std::string& path)
        : stream_(new FileStream(path, FileMode::Create)), leaveOpen_(false), ownsStream_(true) {}

    StreamWriter::~StreamWriter() {
        if (!leaveOpen_) stream_->Close();
        if (ownsStream_) delete stream_;
    }

    void StreamWriter::WriteRaw(const char* data, size_t len) {
        stream_->Write(reinterpret_cast<const SharpRuntime::bytecs*>(data), 0,
                       static_cast<SharpRuntime::intcs>(len));
    }

    void StreamWriter::Write(const std::string& value) { WriteRaw(value.data(), value.size()); }

    // A null value writes nothing, matching TextWriter::Write(const char*) and, through it,
    // .NET's TextWriter.cs:277-283 rule for a null string. This is the one override of that
    // overload in this repository, so it needs its own test: without it the base class guard
    // is bypassed by virtual dispatch and std::strlen is called on a null pointer, which
    // AddressSanitizer reports as a SEGV on 0x0 inside strlen (ticket #1809,
    // build-probe/1823_prefix_defects.log cases 22 and 23 -- case 23 arrives here through
    // TextWriter::WriteLine(const char*), which is why fixing only the base class would have
    // left the crash reachable).
    void StreamWriter::Write(const char* value) {
        if (value == nullptr) return;
        WriteRaw(value, std::strlen(value));
    }

    void StreamWriter::Flush() { stream_->Flush(); }

    // Verified against StreamWriter.cs: Close() delegates to Dispose(true), which checks
    // _closable (i.e. !leaveOpen) before closing the underlying stream -- matching this type's
    // own destructor (above), which already got this right. This method previously closed the
    // stream unconditionally, defeating leaveOpen's entire purpose for any caller that called
    // Close() explicitly instead of only relying on the destructor.
    void StreamWriter::Close() { if (!leaveOpen_) stream_->Close(); }

} // namespace System::IO
