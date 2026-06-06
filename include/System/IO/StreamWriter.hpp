// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
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
    class StreamWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;

        void WriteRaw(const char* data, size_t len);

    public:
        explicit StreamWriter(Stream* stream, bool leaveOpen = false);
        explicit StreamWriter(const std::string& path);
        virtual ~StreamWriter();

        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        virtual void Write(const std::string& value);
        virtual void Write(const char* value);
        virtual void Write(char value);
        virtual void Write(SharpRuntime::intcs value);
        virtual void Write(double value);
        virtual void Write(bool value);

        virtual void WriteLine(const std::string& value);
        virtual void WriteLine(const char* value);
        virtual void WriteLine();

        virtual void Flush();
        virtual void Close();

    private:
        bool ownsStream_ = false;
    };

} // namespace System::IO
