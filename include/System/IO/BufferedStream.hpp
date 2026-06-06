// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/Stream.hpp"

namespace System::IO {

    /**
     * @brief Adds buffering to reads and writes on another stream.
     *
     * Partial C++ counterpart of .NET System.IO.BufferedStream.
     * This stub is a pass-through wrapper — actual buffering is not implemented.
     *
     * @note Status: Stub — no buffering; delegates all calls to inner stream.
     */
    class BufferedStream : public Stream {
        Stream* inner_;
        bool    ownsStream_;
    public:
        explicit BufferedStream(Stream* stream, bool ownsStream = false)
            : inner_(stream), ownsStream_(ownsStream) {}

        ~BufferedStream() override {
            if (ownsStream_ && inner_) { inner_->Close(); }
        }

        SharpRuntime::intcs Read(SharpRuntime::bytecs buffer[],
                                 SharpRuntime::intcs  offset,
                                 SharpRuntime::intcs  count) override {
            return inner_->Read(buffer, offset, count);
        }

        void Write(const SharpRuntime::bytecs buffer[],
                   SharpRuntime::intcs         offset,
                   SharpRuntime::intcs         count) override {
            inner_->Write(buffer, offset, count);
        }

        void WriteByte(SharpRuntime::bytecs value) override { inner_->WriteByte(value); }
        void Flush()  override { inner_->Flush(); }
        void Close()  override { inner_->Close(); }

        [[nodiscard]] SharpRuntime::intcs getLengthProperty()   const override { return inner_->getLengthProperty(); }
        [[nodiscard]] bool getCanReadProperty()  const override { return inner_->getCanReadProperty(); }
        [[nodiscard]] bool getCanWriteProperty() const override { return inner_->getCanWriteProperty(); }
    };

} // namespace System::IO
