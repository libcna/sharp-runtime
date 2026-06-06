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

        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        void  Write(const bytecs buffer[], intcs offset, intcs count) override;
        void  WriteByte(bytecs value) override;
        void  Close() override;
        void  Flush() override {}

        [[nodiscard]] intcs getLengthProperty()   const override;
        [[nodiscard]] bool  getCanWriteProperty() const override { return writable_; }

        /** @brief Returns the underlying buffer as a vector. */
        [[nodiscard]] const std::vector<bytecs>& ToArray() const { return data_; }

        /** @brief Returns a copy of the buffer contents. */
        [[nodiscard]] std::vector<bytecs> GetBuffer() const { return data_; }
    };
}
