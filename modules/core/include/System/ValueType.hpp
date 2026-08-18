// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

/**
 * @brief Provides the base class for value types.
 *
 * C++ counterpart of .NET System.ValueType, which is `public abstract class ValueType`. In .NET
 * it is the implicit base of every struct type and overrides Equals/GetHashCode to compare
 * field-by-field through reflection.
 *
 * @par The constructor is protected, since ticket #2322
 * `System::ValueType v;` used to compile. .NET's class is **abstract**, so C# rejects the
 * equivalent. A C++ class becomes abstract only by having a pure virtual, and .NET's
 * `ValueType.ToString()` has a real body — so making one pure here would be inventing surface
 * the reference does not have. The protected constructor is the faithful move instead, and it
 * gets the property that matters: the type can still be a base, and can no longer be an object.
 * The copy and move members are protected with it, so the base cannot be reached by a slice.
 *
 * @par The identity defaults are a PERMANENT DEVIATION, not a TODO
 * .NET's Equals and GetHashCode compare fields, and its ToString returns the **runtime type
 * name** (`this.GetType().ToString()`); this port returns the literal `"System.ValueType"`. All
 * three are **reflection**, which `CLAUDE.md` lists as permanently out of scope, and a C++ base
 * class cannot enumerate a derived class's fields or learn its name. The gap is therefore not
 * closable rather than merely unclosed.
 *
 * A derived type that needs value semantics must override `Equals` **and** `GetHashCode`
 * together — both are already `virtual`, so no new hook was invented for it — and `ToString` if
 * it wants its own name. Overriding only one of the first two breaks the equals/hash contract
 * silently.
 */
class ValueType {
protected:
    /**
     * @brief Protected default constructor. See the class doc-comment for why this, and not an
     *        abstract class, is the faithful counterpart of .NET's `public abstract`.
     */
    ValueType() = default;
    ValueType(const ValueType&) = default;
    ValueType(ValueType&&) = default;
    ValueType& operator=(const ValueType&) = default;
    ValueType& operator=(ValueType&&) = default;

public:
    virtual ~ValueType() = default;

    /**
     * @brief Indicates whether this instance equals another object.
     *
     * C++ counterpart of .NET ValueType.Equals(object).
     * Default: identity comparison. Override in concrete types for field equality.
     */
    virtual bool Equals(const ValueType& other) const { return this == &other; }

    /**
     * @brief Returns a hash code for this instance.
     *
     * C++ counterpart of .NET ValueType.GetHashCode().
     * Default: address-based hash. Override in concrete types.
     */
    virtual intcs GetHashCode() const {
        return static_cast<intcs>(reinterpret_cast<std::uintptr_t>(this));
    }

    /**
     * @brief Returns a string representation of this instance.
     *
     * C++ counterpart of .NET ValueType.ToString(), which returns `this.GetType().ToString()` —
     * the RUNTIME TYPE NAME. This literal is a permanent deviation: naming the runtime type is
     * reflection. A derived type that wants its own name must override this.
     */
    virtual std::string ToString() const { return "System.ValueType"; }
};

} // namespace System
