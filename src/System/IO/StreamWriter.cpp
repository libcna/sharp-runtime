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
    void StreamWriter::Write(char value)                { WriteRaw(&value, 1); }
    void StreamWriter::Write(SharpRuntime::intcs value) { Write(std::to_string(value)); }
    void StreamWriter::Write(double value)              { Write(std::to_string(value)); }
    void StreamWriter::Write(bool value)                { Write(value ? std::string("True") : std::string("False")); }

    void StreamWriter::WriteLine(const std::string& value) { Write(value); Write("\n"); }
    void StreamWriter::WriteLine(const char* value)        { Write(value); Write("\n"); }
    void StreamWriter::WriteLine()                         { Write("\n"); }

    void StreamWriter::Flush() { stream_->Flush(); }
    void StreamWriter::Close() { stream_->Close(); }

} // namespace System::IO
