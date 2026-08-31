// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <tuple>
#include <type_traits>
#include <vector>

namespace System::Xml::Serialization::detail {

    /**
     * @brief One field/property binding: an explicit XML element name paired with a
     * pointer-to-member, in the order it was registered.
     *
     * SAMPLES-DEC-008's compile-time customization-point design (CLAUDE.md: reflection is a
     * permanent deviation). A real .NET `XmlSerializer` discovers members and their declaration
     * order via reflection; here the order and the name are both spelled out once, at the call
     * site that registers the type, by `SHARP_XML_M` below. That is the same shape
     * `JsonSerializer`'s ADL customization points already use for `nlohmann`'s `to_json`/
     * `from_json` -- a per-type opt-in the compiler checks, not a stub.
     */
    template <typename Class, typename Member>
    struct XmlMember {
        const char* name;
        Member Class::*ptr;
    };

    template <typename Class, typename Member>
    [[nodiscard]] constexpr XmlMember<Class, Member> MakeMember(const char* name, Member Class::*ptr) {
        return XmlMember<Class, Member>{name, ptr};
    }

    /**
     * @brief True for `T = std::vector<U>`, false otherwise.
     *
     * A field of this shape is XNA's un-adorned `List<U>`: no `[XmlArray]`/`[XmlArrayItem]`
     * override in any of the three DEC-008 samples (grep confirmed zero `XmlInclude`/
     * `XmlArray` attributes across ShipGame and RolePlayingGame's Session/save types), so the
     * default naming rule below is the only one this module implements.
     */
    template <typename T>
    struct IsXmlList : std::false_type {};

    template <typename U, typename A>
    struct IsXmlList<std::vector<U, A>> : std::true_type {};

    template <typename T>
    inline constexpr bool IsXmlListV = IsXmlList<T>::value;

    /**
     * @brief Detects a type that opted in via `SHARP_XML_SERIALIZABLE` -- i.e. one for which
     * `SharpXmlRootName`/`SharpXmlMembers` are found by argument-dependent lookup on a
     * `const T*`.
     *
     * Deliberately excludes any inheritance-based dispatch: XmlSerializer resolves this
     * exclusively on the *declared* type, which is exactly the ADL lookup below and exactly
     * what XNA's own `XmlSerializer` does when no `[XmlInclude]` is present (see
     * `docs/XmlSerializationScope.md`'s "no xsi:type" note).
     */
    template <typename T>
    concept XmlComposite = requires(const T* value) {
        { SharpXmlRootName(value) };
        { SharpXmlMembers(value) };
    };

}  // namespace System::Xml::Serialization::detail

/**
 * @def SHARP_XML_M
 * @brief Registers one member of a `SHARP_XML_SERIALIZABLE` type: element name equals the C#
 * field/property name it mirrors (XNA's own default -- `XmlSerializer` uses the member's
 * declared name verbatim when no `[XmlElement(ElementName=...)]` is present).
 */
#define SHARP_XML_M(TypeName, member) \
    ::System::Xml::Serialization::detail::MakeMember(#member, &TypeName::member)

/**
 * @def SHARP_XML_SERIALIZABLE
 * @brief Opts @p TypeName into `System::Xml::Serialization::XmlSerializer`: declares the root
 * element name it uses when serialized as a top-level type, and the ordered list of members
 * (build each with `SHARP_XML_M`) walked for both serialize and deserialize.
 *
 * Place inside the class body. Must be public (or the class must `friend` this expansion,
 * which it does implicitly by being a member) so the two hook functions are visible to ADL
 * from `System::Xml::Serialization`.
 */
#define SHARP_XML_SERIALIZABLE(TypeName, rootElementName, ...)                 \
    friend constexpr const char* SharpXmlRootName(const TypeName*) {           \
        return rootElementName;                                                \
    }                                                                          \
    friend constexpr auto SharpXmlMembers(const TypeName*) {                   \
        return std::make_tuple(__VA_ARGS__);                                   \
    }
