// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "System/Xml/Serialization/detail/XmlMember.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/Xml/XmlException.hpp"

namespace System::Xml::Serialization::detail {

    /**
     * @brief Text <-> value for the leaf (non-composite, non-collection) types the three
     * DEC-008 samples' save data actually uses: `std::string`, `bool`, `float`, `double`, the
     * fixed-width integers, and any enum registered with `SHARP_XML_ENUM`.
     *
     * Delegates entirely to `System::Xml::XmlConvert`, which is where the float/double
     * round-trip work already lives (probed empirically in `build-probe/
     * xml_probe_float_roundtrip.cpp`: 47/47 values -- including a full `Matrix.Identity` and a
     * placed-entity transform -- round-tripped bit-for-bit through `XmlConvert::ToString`/
     * `ToSingle`/`ToDouble`). This header adds no float-formatting logic of its own; it only
     * dispatches to it by type, which is the entire reason Opus's "risk #1" turned out to be
     * pre-existing, tested infrastructure rather than new work.
     */
    template <typename T>
    [[nodiscard]] std::string ToXmlText(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else if constexpr (XmlEnum<T>) {
            // .NET writes an enum as its member NAME, not its numeric value.
            for (const auto& entry : SharpXmlEnumEntries(static_cast<const T*>(nullptr))) {
                if (entry.value == value) return entry.name;
            }
            throw System::Xml::XmlException(
                "XmlSerializer: enum value has no name registered with SHARP_XML_ENUM.");
        } else if constexpr (std::is_same_v<T, bool>) {
            return System::Xml::XmlConvert::ToString(value);
        } else if constexpr (std::is_same_v<T, float>) {
            return System::Xml::XmlConvert::ToString(value);
        } else if constexpr (std::is_same_v<T, double>) {
            return System::Xml::XmlConvert::ToString(value);
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return System::Xml::XmlConvert::ToString(value);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return System::Xml::XmlConvert::ToString(value);
        } else {
            static_assert(!sizeof(T), "ToXmlText: unsupported leaf type. Add a branch here, or "
                                       "the type needs SHARP_XML_SERIALIZABLE / SHARP_XML_ENUM.");
        }
    }

    template <typename T>
    [[nodiscard]] T FromXmlText(const std::string& text) {
        if constexpr (std::is_same_v<T, std::string>) {
            return text;
        } else if constexpr (XmlEnum<T>) {
            for (const auto& entry : SharpXmlEnumEntries(static_cast<const T*>(nullptr))) {
                if (text == entry.name) return entry.value;
            }
            throw System::Xml::XmlException("XmlSerializer: '" + text +
                                             "' is not a registered enumerator name.");
        } else if constexpr (std::is_same_v<T, bool>) {
            return System::Xml::XmlConvert::ToBoolean(text);
        } else if constexpr (std::is_same_v<T, float>) {
            return System::Xml::XmlConvert::ToSingle(text);
        } else if constexpr (std::is_same_v<T, double>) {
            return System::Xml::XmlConvert::ToDouble(text);
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return System::Xml::XmlConvert::ToInt32(text);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return System::Xml::XmlConvert::ToInt64(text);
        } else {
            static_assert(!sizeof(T), "FromXmlText: unsupported leaf type.");
        }
    }

    /**
     * @brief The element name .NET gives to each item of a `List<T>` whose `T` is a primitive:
     * the **XSD** type name, not the C# keyword.
     *
     * `List<string>` serializes its items as `<string>`, `List<int>` as `<int>`, but `List<bool>`
     * as `<boolean>` and `List<long>` as `<long>` -- the XML Schema spellings, which is why this
     * cannot be derived from the C++ type name. RolePlayingGame's `PartySaveData` has both a
     * `List<string> monsterKillNames` and a `List<int> monsterKillCounts`, so both spellings are
     * reachable from a real save route.
     */
    template <typename T>
    [[nodiscard]] constexpr const char* XmlPrimitiveElementName() {
        if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        } else if constexpr (std::is_same_v<T, bool>) {
            return "boolean";
        } else if constexpr (std::is_same_v<T, float>) {
            return "float";
        } else if constexpr (std::is_same_v<T, double>) {
            return "double";
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return "int";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return "long";
        } else {
            static_assert(!sizeof(T),
                          "XmlPrimitiveElementName: no XSD element name for this list item type. "
                          "A list of enums or of an unregistered type is not supported -- none is "
                          "reachable from the DEC-008 samples; see docs/XmlSerializationScope.md.");
        }
    }

}  // namespace System::Xml::Serialization::detail
