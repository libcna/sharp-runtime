// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "System/Xml/XmlConvert.hpp"

namespace System::Xml::Serialization::detail {

    /**
     * @brief Text <-> value for the leaf (non-composite, non-collection) types the three
     * DEC-008 samples' save data actually uses: `std::string`, `bool`, `float`, `double` and the
     * fixed-width integers.
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
                                       "the type needs SHARP_XML_SERIALIZABLE instead.");
        }
    }

    template <typename T>
    [[nodiscard]] T FromXmlText(const std::string& text) {
        if constexpr (std::is_same_v<T, std::string>) {
            return text;
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

}  // namespace System::Xml::Serialization::detail
