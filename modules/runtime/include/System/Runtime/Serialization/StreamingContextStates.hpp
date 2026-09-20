// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Runtime::Serialization {

    /**
     * @brief Defines a set of flags that specifies the source or destination context for a stream.
     *
     * C++ counterpart of .NET System.Runtime.Serialization.StreamingContextStates. The values are
     * .NET's own, so a caller that tests or combines them reads the same numbers.
     */
    enum class StreamingContextStates {
        /** @brief No context is specified. */
        None = 0,
        /** @brief The source or destination is a different process on the same machine. */
        CrossProcess = 1,
        /** @brief The source or destination is a different machine. */
        CrossMachine = 2,
        /** @brief The source or destination is a file. */
        File = 4,
        /** @brief The source or destination is persisted storage. */
        Persistence = 8,
        /** @brief The data is remoted to a context in an unknown location. */
        Remoting = 16,
        /** @brief The source or destination is unknown to the serialized data's consumer. */
        Other = 32,
        /** @brief The object graph is being cloned. */
        Clone = 64,
        /** @brief The source or destination is a different AppDomain. */
        CrossAppDomain = 128,
        /** @brief The serialized data can be transmitted to or received from any context. */
        All = 255
    };

    /** @brief Bitwise OR of two StreamingContextStates values, as .NET's `[Flags]` enum allows. */
    [[nodiscard]] constexpr StreamingContextStates operator|(StreamingContextStates left,
                                                             StreamingContextStates right) noexcept {
        return static_cast<StreamingContextStates>(static_cast<int>(left) | static_cast<int>(right));
    }

    /** @brief Bitwise AND of two StreamingContextStates values. */
    [[nodiscard]] constexpr StreamingContextStates operator&(StreamingContextStates left,
                                                             StreamingContextStates right) noexcept {
        return static_cast<StreamingContextStates>(static_cast<int>(left) & static_cast<int>(right));
    }

} // namespace System::Runtime::Serialization
