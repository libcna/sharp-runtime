// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "System/IO/Stream.hpp"

namespace System::IO
{
    /**
     * @brief A stream backed by an in-memory byte buffer.
     *
     * Used to wrap data loaded from sources such as Android APK assets.
     *
     * @note Status: IMPLEMENTED
     */
    class MemoryStream : public Stream
    {
    private:
        std::vector<bytecs> data_;
        intcs position_;
        bool  writable_;

    public:
        /** @brief Creates an empty, writable MemoryStream. */
        MemoryStream();

        /**
         * @brief Creates a read-only MemoryStream over a byte buffer.
         *
         * @param buffer Pointer to the source bytes.
         * @param size   Number of bytes to copy.
         */
        MemoryStream(const bytecs* buffer, intcs size);

        ~MemoryStream() override = default;

        /** Reads up to count bytes into buffer at offset; returns bytes actually read. */
        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        /** Writes count bytes from buffer at offset into the memory buffer. */
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        /** Writes a single byte to the memory buffer. */
        void  WriteByte(bytecs value) override;
        /** Resets the stream position (no-op for MemoryStream). */
        void  Close() override;
        /** No-op for MemoryStream; the buffer is always up to date. */
        void  Flush() override {}

        /** Returns the length of the in-memory buffer in bytes. */
        [[nodiscard]] intcs getLengthProperty()   const override;
        /** Returns true if the stream was created as writable. */
        [[nodiscard]] bool  getCanWriteProperty() const override { return writable_; }

        /** @brief Returns the underlying buffer as a vector. */
        [[nodiscard]] const std::vector<bytecs>& ToArray() const { return data_; }

        /** @brief Returns a copy of the buffer contents. */
        [[nodiscard]] std::vector<bytecs> GetBuffer() const { return data_; }
    };
}
