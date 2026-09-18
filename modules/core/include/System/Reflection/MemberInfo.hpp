// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include "System/Reflection/MemberTypes.hpp"
#include "System/Type.hpp"

namespace System::Reflection {

/**
 * @brief Obtains information about the attributes of a member and provides access to member
 *        metadata.
 *
 * C++ counterpart of .NET System.Reflection.MemberInfo, and the root of the **explicit
 * metadata** this runtime offers in place of reflection. .NET discovers members by inspecting
 * a type at run time; C++ cannot, and this runtime does not pretend to (`System::Type` is a
 * `std::type_info` wrapper, `Activator` is template-only). What a `MemberInfo` here describes is
 * a member the *author of a type* declared explicitly -- for instance the constructor
 * `ConstructorInfo::Of<Vector3, float, float, float>()` -- and every such description can do
 * its job: a `ConstructorInfo` invokes the constructor it names. There is no `GetMembers`, no
 * lookup by name on `System::Type`, and no member kind beyond the ones a consumer has needed
 * (`ConstructorInfo` for `System.ComponentModel.Design.Serialization.InstanceDescriptor`).
 *
 * Equality is identity unless a derived class defines value semantics; `ConstructorInfo` does.
 */
class MemberInfo {
public:
    virtual ~MemberInfo() = default;

    /**
     * @brief Gets the name of the current member.
     *
     * C++ counterpart of .NET MemberInfo.Name.
     * @return The member name; ".ctor" for a constructor.
     */
    [[nodiscard]] virtual const std::string& getNameProperty() const = 0;

    /**
     * @brief Gets the class that declares this member.
     *
     * C++ counterpart of .NET MemberInfo.DeclaringType.
     * @return The declaring type.
     */
    [[nodiscard]] virtual System::Type getDeclaringTypeProperty() const = 0;

    /**
     * @brief Gets a MemberTypes value indicating the type of the member.
     *
     * C++ counterpart of .NET MemberInfo.MemberType.
     * @return The member kind.
     */
    [[nodiscard]] virtual MemberTypes getMemberTypeProperty() const = 0;

    /**
     * @brief Returns a value that indicates whether this instance is equal to a specified object.
     *
     * C++ counterpart of .NET MemberInfo.Equals(object). Identity unless overridden.
     * @param other The member to compare with.
     * @return true if the two descriptions denote the same member.
     */
    [[nodiscard]] virtual bool Equals(const MemberInfo& other) const { return this == &other; }

    /**
     * @brief Returns the hash code for this instance.
     *
     * C++ counterpart of .NET MemberInfo.GetHashCode(). Consistent with `Equals`.
     * @return A hash code.
     */
    [[nodiscard]] virtual int GetHashCode() const {
        return static_cast<int>(std::hash<const MemberInfo*>{}(this));
    }

    /**
     * @brief Returns a string that represents the member.
     *
     * C++ counterpart of .NET MemberInfo.ToString().
     * @return A description of the member; the name unless a derived class says more.
     */
    [[nodiscard]] virtual std::string ToString() const { return getNameProperty(); }

protected:
    /** @brief Initializes an empty member description for a derived explicit-metadata type. */
    MemberInfo() = default;

    /** @brief Copies a member description. @param other The description to copy. */
    MemberInfo(const MemberInfo& other) = default;

    /** @brief Moves a member description. @param other The description to move. */
    MemberInfo(MemberInfo&& other) = default;

    /** @brief Copies a member description. @param other The description to copy. @return This description. */
    MemberInfo& operator=(const MemberInfo& other) = default;

    /** @brief Moves a member description. @param other The description to move. @return This description. */
    MemberInfo& operator=(MemberInfo&& other) = default;
};

} // namespace System::Reflection
