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
     * @brief Uppercases the exponent marker in a float/double lexical form.
     *
     * **A measured deviation, not a preference.** XML Schema's canonical lexical space for
     * `float`/`double` spells the exponent `E`, .NET's `XmlConvert.ToString` emits `E`, and the
     * authentic fixture `Samples/NetRumble_4_0/.../rocketTrail.xml` contains
     * `<Duration>3.40282347E+38</Duration>`. `System::Xml::XmlConvert::ToString(float)` here
     * produces a lowercase `e` (`3.4028235e+38`), because it delegates to
     * `System::Single::ToString`, whose lowercase form is pinned by an existing core test
     * (`DoubleTests.cpp:643` asserts `Double::ToString(1e100, "R") == "1e+100"`, where real
     * .NET gives `1E+100`).
     *
     * That core deviation is out of this module's blast radius -- changing it would rewrite a
     * pinned expectation in another module, which needs its own ticket and audit trail. So the
     * XML wire form is corrected here, where the contract is XML's, and the finding is recorded
     * in `docs/XmlSerializationScope.md` rather than silently absorbed.
     *
     * @note The remaining difference is digit count, and it is harmless: .NET Framework 4.0
     * wrote `3.40282347E+38` (9 significant digits, its `R` algorithm) where the shortest
     * round-trippable form is `3.4028235E+38` (8). Both parse to the identical `float`, which
     * `XnaFixtureTests` asserts rather than assumes.
     */
    [[nodiscard]] inline std::string UppercaseExponent(std::string text) {
        const std::size_t marker = text.find('e');
        if (marker != std::string::npos) text[marker] = 'E';
        return text;
    }

    /**
     * @brief Drops a leading `+` before handing numeric text to `XmlConvert`.
     *
     * XML Schema's lexical space for the numeric types allows an explicit `+`
     * (`(\+|-)?` in the grammar), and .NET's `float.Parse`/`XmlConvert` accept it.
     * `System::Xml::XmlConvert::ToSingle("+0.4")` throws `Input string was not in a correct
     * format` here, because it reaches `std::from_chars`, which rejects a leading plus.
     *
     * No authentic XNA fixture uses the form -- grepped across Spacewar's `settings.xml`,
     * ShipGame's level and ship content and NetRumble's particle effects, zero hits -- so this
     * is a conformance gap rather than a blocker. It is closed anyway, at this module's own
     * boundary, for the same reason as the exponent case: a save file edited by hand or written
     * by another XSD-conformant tool may carry it, and a reader that is strictly more tolerant
     * than the writer can only help. The underlying `XmlConvert` behaviour is left alone; see
     * `docs/XmlSerializationScope.md`.
     */
    [[nodiscard]] inline std::string WithoutLeadingPlus(const std::string& text) {
        std::size_t first = text.find_first_not_of(" \t\r\n");
        if (first != std::string::npos && text[first] == '+') {
            std::string stripped = text;
            stripped.erase(first, 1);
            return stripped;
        }
        return text;
    }

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
            return UppercaseExponent(System::Xml::XmlConvert::ToString(value));
        } else if constexpr (std::is_same_v<T, double>) {
            return UppercaseExponent(System::Xml::XmlConvert::ToString(value));
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
            return System::Xml::XmlConvert::ToSingle(WithoutLeadingPlus(text));
        } else if constexpr (std::is_same_v<T, double>) {
            return System::Xml::XmlConvert::ToDouble(WithoutLeadingPlus(text));
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            return System::Xml::XmlConvert::ToInt32(WithoutLeadingPlus(text));
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return System::Xml::XmlConvert::ToInt64(WithoutLeadingPlus(text));
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
