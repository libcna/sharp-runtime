// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/StreamWriter.hpp"
#include "System/IO/FileStream.hpp"

#include <cstring>

namespace System::IO {

    StreamWriter::StreamWriter(Stream* stream, bool leaveOpen)
        : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false) {}

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
    void StreamWriter::Close() { stream_->Close(); }

} // namespace System::IO
