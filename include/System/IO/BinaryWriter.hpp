// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/Stream.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO {

    using SharpRuntime::bytecs;
    using SharpRuntime::intcs;
    using SharpRuntime::shortcs;
    using SharpRuntime::ushortcs;
    using SharpRuntime::longcs;
    using SharpRuntime::Single;

    /**
     * @brief Writes primitive types to a stream in binary, little-endian format.
     *
     * Partial C++ counterpart of .NET System.IO.BinaryWriter.
     *
     * @note Status: Implemented
     */
    class BinaryWriter {
    private:
        Stream* stream_;
        bool    leaveOpen_;

        void WriteBytes(const bytecs* buf, intcs count);

    public:
        explicit BinaryWriter(Stream* stream, bool leaveOpen = false);
        virtual ~BinaryWriter();

        [[nodiscard]] Stream* getBaseStreamProperty() const { return stream_; }

        virtual void Write(bytecs value);
        virtual void Write(int8_t value);
        virtual void Write(shortcs value);
        virtual void Write(ushortcs value);
        virtual void Write(intcs value);
        virtual void Write(uint32_t value);
        virtual void Write(longcs value);
        virtual void Write(uint64_t value);
        virtual void Write(Single value);
        virtual void Write(double value);
        virtual void Write(bool value);
        virtual void Write(const std::string& value);

        virtual void Write(const bytecs* buffer, intcs offset, intcs count);

        virtual void Flush();
        virtual void Close();
    };

} // namespace System::IO
