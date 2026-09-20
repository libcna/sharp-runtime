// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <utility>

#include "System/Runtime/Serialization/StreamingContextStates.hpp"

namespace System::Runtime::Serialization {

    /**
     * @brief Describes the source and destination of a serialized stream.
     *
     * C++ counterpart of .NET System.Runtime.Serialization.StreamingContext: a value type carrying
     * the context flags and an optional caller-supplied context object, which a serializing type
     * reads to decide what to write.
     *
     * @note Scope: this is the context half of the narrow legacy serialization contract
     *   SerializationInfo documents. It is not part of a binary formatter, and nothing in this
     *   runtime writes a stream; see SerializationInfo.hpp for what the contract is and is not.
     */
    struct StreamingContext {
        /** @brief The context flags. */
        StreamingContextStates State = StreamingContextStates::All;

        /** @brief The caller-supplied context object, or an empty object when there is none. */
        std::any Context{};

        /** @brief Creates a context with no flags set and no context object. */
        StreamingContext() = default;

        /**
         * @brief Creates a context with the specified flags.
         * @param state The context flags.
         */
        explicit StreamingContext(StreamingContextStates state) : State(state) {}

        /**
         * @brief Creates a context with the specified flags and context object.
         * @param state The context flags.
         * @param additional The caller-supplied context object.
         */
        StreamingContext(StreamingContextStates state, std::any additional)
            : State(state), Context(std::move(additional)) {}

        /**
         * @brief Gets the context flags.
         *
         * C++ counterpart of .NET StreamingContext.State.
         * @return The flags this context carries.
         */
        [[nodiscard]] StreamingContextStates getStateProperty() const { return State; }

        /**
         * @brief Gets the caller-supplied context object.
         *
         * C++ counterpart of .NET StreamingContext.Context.
         * @return The context object, or an empty object when there is none.
         */
        [[nodiscard]] const std::any& getContextProperty() const { return Context; }
    };

} // namespace System::Runtime::Serialization
