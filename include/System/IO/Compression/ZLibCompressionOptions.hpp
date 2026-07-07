// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Compression/ZLibCompressionStrategy.hpp"

namespace System::IO::Compression {

    /**
     * @brief Provides compression options to be used with ZLibStream, DeflateStream, and
     * GZipStream.
     *
     * C++ counterpart of .NET System.IO.Compression.ZLibCompressionOptions.
     */
    class ZLibCompressionOptions {
    private:
        int compressionLevel_ = -1;
        ZLibCompressionStrategy strategy_ = ZLibCompressionStrategy::Default;
        int windowLog_ = -1;

    public:
        /** The minimum window log (base-2 logarithm of the window size) for zlib compression. */
        static constexpr int MinWindowLog = 8;
        /** The maximum window log (base-2 logarithm of the window size) for zlib compression. */
        static constexpr int MaxWindowLog = 15;
        /** The default window log (base-2 logarithm of the window size) for zlib compression. */
        static constexpr int DefaultWindowLog = MaxWindowLog;
        /** The minimum compression level accepted by CompressionLevel. */
        static constexpr int MinQuality = 0;
        /** The maximum compression level accepted by CompressionLevel. */
        static constexpr int MaxQuality = 9;

        ZLibCompressionOptions() = default;

        /**
         * @brief Gets or sets the compression level for a compression stream.
         *
         * Accepts any value between -1 and 9 (inclusive): 0 gives no compression, 1 gives best
         * speed, 9 gives best compression, and -1 requests the default compression level
         * (currently equivalent to 6). Default value is -1.
         * @throws System::ArgumentOutOfRangeException if @p value is not -1 and outside [0, 9].
         */
        [[nodiscard]] int getCompressionLevelProperty() const { return compressionLevel_; }
        void setCompressionLevelProperty(int value) {
            if (value != -1) {
                if (value < MinQuality)
                    throw System::ArgumentOutOfRangeException("value", "Value must be greater than or equal to " + std::to_string(MinQuality) + ".");
                if (value > MaxQuality)
                    throw System::ArgumentOutOfRangeException("value", "Value must be less than or equal to " + std::to_string(MaxQuality) + ".");
            }
            compressionLevel_ = value;
        }

        /**
         * @brief Gets or sets the compression algorithm for a compression stream.
         * @throws System::ArgumentOutOfRangeException if @p value is not a valid ZLibCompressionStrategy value.
         */
        [[nodiscard]] ZLibCompressionStrategy getCompressionStrategyProperty() const { return strategy_; }
        void setCompressionStrategyProperty(ZLibCompressionStrategy value) {
            if (static_cast<int>(value) < static_cast<int>(ZLibCompressionStrategy::Default))
                throw System::ArgumentOutOfRangeException("value", "Value must be greater than or equal to Default.");
            if (static_cast<int>(value) > static_cast<int>(ZLibCompressionStrategy::Fixed))
                throw System::ArgumentOutOfRangeException("value", "Value must be less than or equal to Fixed.");
            strategy_ = value;
        }

        /**
         * @brief Gets or sets the base-2 logarithm of the window size for a compression stream.
         *
         * Accepts -1 or any value between 8 and 15 (inclusive). -1 requests the default window
         * log (currently equivalent to 15, a 32KB window). Default value is -1.
         * @throws System::ArgumentOutOfRangeException if @p value is not -1 and outside [8, 15].
         */
        [[nodiscard]] int getWindowLogProperty() const { return windowLog_; }
        void setWindowLogProperty(int value) {
            if (value != -1) {
                if (value < MinWindowLog)
                    throw System::ArgumentOutOfRangeException("value", "Value must be greater than or equal to " + std::to_string(MinWindowLog) + ".");
                if (value > MaxWindowLog)
                    throw System::ArgumentOutOfRangeException("value", "Value must be less than or equal to " + std::to_string(MaxWindowLog) + ".");
            }
            windowLog_ = value;
        }
    };

} // namespace System::IO::Compression
