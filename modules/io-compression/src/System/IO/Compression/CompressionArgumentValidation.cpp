// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Compression/CompressionArgumentValidation.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"

#include <zlib.h>

namespace System::IO::Compression::Detail {

    // The enum's five members happen to carry the same numeric values as zlib's constants today.
    // These assertions make that an enforced fact rather than a coincidence a future edit could
    // break silently: if either side is renumbered the build stops here instead of compressing
    // with the wrong strategy.
    static_assert(static_cast<int>(ZLibCompressionStrategy::Default) == Z_DEFAULT_STRATEGY);
    static_assert(static_cast<int>(ZLibCompressionStrategy::Filtered) == Z_FILTERED);
    static_assert(static_cast<int>(ZLibCompressionStrategy::HuffmanOnly) == Z_HUFFMAN_ONLY);
    static_assert(static_cast<int>(ZLibCompressionStrategy::RunLengthEncoding) == Z_RLE);
    static_assert(static_cast<int>(ZLibCompressionStrategy::Fixed) == Z_FIXED);

    void ThrowNegativeLength(const char* paramName) {
        throw System::ArgumentOutOfRangeException(paramName, "Non-negative number required.");
    }

    void ThrowNullBuffer(const char* paramName) {
        throw System::ArgumentNullException(paramName);
    }

    void ThrowInvalidCompressionMode(const char* paramName) {
        throw System::ArgumentException("Enum value was out of legal range.", paramName);
    }

    void ThrowStreamClosed(const char* typeName) {
        throw System::ObjectDisposedException(typeName, "Cannot access a closed Stream.");
    }

    intcs ResolveZLibStrategy(ZLibCompressionStrategy strategy) {
        switch (strategy) {
            case ZLibCompressionStrategy::Default:           return Z_DEFAULT_STRATEGY;
            case ZLibCompressionStrategy::Filtered:          return Z_FILTERED;
            case ZLibCompressionStrategy::HuffmanOnly:       return Z_HUFFMAN_ONLY;
            case ZLibCompressionStrategy::RunLengthEncoding: return Z_RLE;
            case ZLibCompressionStrategy::Fixed:             return Z_FIXED;
        }
        throw System::ArgumentOutOfRangeException("strategy", "Value must be between Default and Fixed.");
    }

} // namespace System::IO::Compression::Detail
