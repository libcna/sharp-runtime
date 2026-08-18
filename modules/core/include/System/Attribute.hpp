// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <functional>
#include <typeinfo>

namespace System {

/**
 * @brief Base class for all custom attributes.
 *
 * C++ counterpart of .NET System.Attribute.
 *
 * In .NET, Attribute is abstract with a **protected** constructor and uses reflection to
 * implement field-by-field Equals/GetHashCode.
 *
 * @par The constructor is protected too, since ticket #2339
 * It used to be public, "so that the class can be instantiated in tests", with a doc-comment
 * asking callers to *treat it as logically abstract*. A comment asking callers to behave is not a
 * contract; .NET spells the same intent in the language, and so does this port now. `System::
 * Attribute a;` no longer compiles — derive, as .NET requires.
 *
 * @par The identity Equals is a PERMANENT DEVIATION, not a TODO
 * .NET compares every instance field and derives its hash from the first non-array one. **That is
 * reflection**, which `CLAUDE.md` lists as permanently out of scope, and a C++ base class cannot
 * enumerate a derived class's fields at all — so the gap is not closable rather than merely
 * unclosed. What #2339 could do, and did, is make the incompatible default unreachable as a
 * concrete type.
 *
 * **The consequence is measured and must not be forgotten: forty-six types in this repository
 * derive from Attribute and not one overrides `Equals`, so all forty-six inherit object
 * identity.** Two independently constructed `CLSCompliantAttribute(true)` objects therefore
 * compare **unequal** here and **equal** in .NET.
 *
 * A subclass that needs value equality must override **both** `Equals` and `GetHashCode` — both
 * are already `virtual`, so no new hook was invented for this. Overriding only one breaks the
 * equals/hash contract silently.
 *
 * - TypeId returns the std::type_info of the most-derived type.
 */
class Attribute {
protected:
    /**
     * @brief Protected default constructor, matching .NET's `protected Attribute()`.
     *
     * Ticket #2339 made this protected. The copy and move members are protected with it, so a
     * caller cannot reach the base through a slice either.
     */
    Attribute() = default;
    Attribute(const Attribute&) = default;
    Attribute(Attribute&&) = default;
    Attribute& operator=(const Attribute&) = default;
    Attribute& operator=(Attribute&&) = default;

public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~Attribute() = default;

    // -------------------------------------------------------------------------

    /**
     * @brief Determines whether this instance is equal to another attribute.
     *
     * C++ counterpart of .NET Attribute.Equals(object). **The default is object identity, and
     * that is a permanent deviation** — .NET compares every instance field, which is reflection.
     * See the class doc-comment. A subclass that needs value equality must override this AND
     * GetHashCode; all forty-six subclasses in this repository currently override neither.
     * @param other The attribute to compare with.
     * @return true if the two attributes are the same object instance.
     */
    virtual bool Equals(const Attribute& other) const { return this == &other; }

    /**
     * @brief Returns a hash code for this attribute.
     *
     * C++ counterpart of .NET Attribute.GetHashCode(). Default implementation
     * hashes the object address. Subclasses that override Equals() should also
     * override GetHashCode() to maintain the equals/hashCode contract.
     * @return A hash value derived from this object's address.
     */
    virtual int GetHashCode() const {
        return static_cast<int>(std::hash<const Attribute*>{}(this));
    }

    /**
     * @brief Gets a unique identifier for this attribute.
     *
     * C++ counterpart of .NET Attribute.TypeId (returns object/Type).
     * Here the std::type_info of the most-derived type is returned, which
     * allows distinguishing different attribute types at runtime.
     * @return The std::type_info of the most-derived concrete type.
     */
    [[nodiscard]] virtual const std::type_info& getTypeIdProperty() const {
        return typeid(*this);
    }

    /**
     * @brief Returns true if this is the default instance of the attribute type.
     *
     * C++ counterpart of .NET Attribute.IsDefaultAttribute().
     * Default implementation returns false; subclasses may override.
     */
    virtual bool getIsDefaultAttributeProperty() const { return false; }

    /**
     * @brief Returns true if this attribute matches the specified attribute.
     *
     * C++ counterpart of .NET Attribute.Match(object). Default implementation
     * delegates to Equals(). Subclasses may override to provide a weaker
     * notion of equality (e.g., same type regardless of field values).
     * @param obj The attribute to match against.
     * @return The result of Equals(obj).
     */
    virtual bool Match(const Attribute& obj) const { return Equals(obj); }
};

} // namespace System
