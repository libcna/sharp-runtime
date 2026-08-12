// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /**
     * @brief Identifies the runtime category of a type token.
     *
     * @warning <b>This is not a counterpart of any .NET type, and it is not used by
     * anything.</b> Two claims that stood here until ticket #2333 were wrong and are
     * withdrawn: .NET's `System.RuntimeType` is an <i>internal sealed class</i> deriving
     * from `TypeInfo` and carrying type handles, metadata and assembly information -- it
     * is not an enumeration -- and the six values below are not "documented internal
     * constants used in CoreCLR", they are this port's own invention. `None`,
     * `Primitive`, `ValueType`, `ReferenceType`, `Array` and `GenericParameter` name a
     * category scheme that has no .NET original, so nothing here can be checked against
     * one.
     *
     * @note What it is instead: a port-local classifier with <b>no production consumer</b>.
     * The only file in this repository that includes this header is
     * `modules/core/tests/System/RuntimeTypeTests.cpp`, which asserts the six integer
     * values and nothing else. Whether the type is kept, renamed out of the way of the
     * .NET name, or removed is an open decision; note that CLAUDE.md lists reflection
     * (`System::Type`, `System::Activator`, ...) as a permanent deviation that is out of
     * scope, so the .NET class whose name this occupies is never going to be ported here
     * and will never need the name back.
     */
    enum class RuntimeType {
        /** @brief No runtime type or unknown. */
        None = 0,
        /** @brief The type is a primitive value type (int, bool, etc.). */
        Primitive = 1,
        /** @brief The type is a value type (struct). */
        ValueType = 2,
        /** @brief The type is a reference type (class). */
        ReferenceType = 3,
        /** @brief The type is an array. */
        Array = 4,
        /** @brief The type is a generic type parameter. */
        GenericParameter = 5,
    };

} // namespace System
