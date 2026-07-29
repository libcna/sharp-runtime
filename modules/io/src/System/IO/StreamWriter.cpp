// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/StreamWriter.hpp"
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
    StreamWriter::StreamWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false)
    {
        if (stream == nullptr) throw System::ArgumentNullException("stream");
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
    void StreamWriter::Write(const char* value)         { WriteRaw(value, std::strlen(value)); }

    void StreamWriter::Flush() { stream_->Flush(); }

    // Verified against StreamWriter.cs: Close() delegates to Dispose(true), which checks
    // _closable (i.e. !leaveOpen) before closing the underlying stream -- matching this type's
    // own destructor (above), which already got this right. This method previously closed the
    // stream unconditionally, defeating leaveOpen's entire purpose for any caller that called
    // Close() explicitly instead of only relying on the destructor.
    void StreamWriter::Close() { if (!leaveOpen_) stream_->Close(); }

} // namespace System::IO
