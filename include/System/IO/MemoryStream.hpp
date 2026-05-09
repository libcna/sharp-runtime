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
        std::vector<bytecs> data;
        intcs position;

    public:
        /**
         * @brief Constructs a MemoryStream from a byte buffer.
         *
         * @param buffer Pointer to the source bytes.
         * @param size   Number of bytes to copy.
         *
         * @note Status: IMPLEMENTED
         */
        MemoryStream(const bytecs* buffer, intcs size);

        ~MemoryStream() override = default;

        intcs Read(bytecs buffer[], intcs offset, intcs count) override;
        void Close() override;
        [[nodiscard]] intcs getLengthProperty() const override;
    };
}
