// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentNullException.hpp"
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
        /**
         * @brief Constructs a BufferedStream wrapping the given stream.
         * @throws System::ArgumentNullException if @p stream is null.
         */
        explicit BufferedStream(Stream* stream, bool ownsStream = false)
            : inner_(stream), ownsStream_(ownsStream) {
            if (!inner_) throw System::ArgumentNullException("stream");
        }

        /** Destroys the BufferedStream; closes the inner stream when ownsStream is true. */
        ~BufferedStream() override {
            if (ownsStream_ && inner_) { inner_->Close(); }
        }

        /** Delegates to the inner stream's Read. */
        SharpRuntime::intcs Read(SharpRuntime::bytecs buffer[],
                                 SharpRuntime::intcs  offset,
                                 SharpRuntime::intcs  count) override {
            return inner_->Read(buffer, offset, count);
        }

        /** Delegates to the inner stream's Write. */
        void Write(const SharpRuntime::bytecs buffer[],
                   SharpRuntime::intcs         offset,
                   SharpRuntime::intcs         count) override {
            inner_->Write(buffer, offset, count);
        }

        /** Delegates to the inner stream's WriteByte. */
        void WriteByte(SharpRuntime::bytecs value) override { inner_->WriteByte(value); }
        /** Delegates to the inner stream's Flush. */
        void Flush()  override { inner_->Flush(); }
        /** Delegates to the inner stream's Close. */
        void Close()  override { inner_->Close(); }

        /** Returns the length of the inner stream. */
        [[nodiscard]] SharpRuntime::intcs getLengthProperty()   const override { return inner_->getLengthProperty(); }
        /** Returns true if the inner stream supports reading. */
        [[nodiscard]] bool getCanReadProperty()  const override { return inner_->getCanReadProperty(); }
        /** Returns true if the inner stream supports writing. */
        [[nodiscard]] bool getCanWriteProperty() const override { return inner_->getCanWriteProperty(); }

        /** Delegates to the inner stream's Position getter. */
        [[nodiscard]] SharpRuntime::intcs getPositionProperty() const override { return inner_->getPositionProperty(); }
        /** Delegates to the inner stream's Position setter. */
        void setPositionProperty(SharpRuntime::intcs value) override { inner_->setPositionProperty(value); }

        /** Returns true if the inner stream supports seeking. */
        [[nodiscard]] bool getCanSeekProperty() const override { return inner_->getCanSeekProperty(); }
        /** Delegates to the inner stream's SetLength. */
        void SetLength(SharpRuntime::intcs value) override { inner_->SetLength(value); }
    };

} // namespace System::IO
