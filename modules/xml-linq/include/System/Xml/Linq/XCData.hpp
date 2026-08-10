// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XText.hpp"

namespace System::Xml::Linq {

    /**
     * @brief Represents a text node that contains CDATA.
     *
     * C++ counterpart of .NET System.Xml.Linq.XCData. Matches .NET's hierarchy: XCData derives
     * from XText (not directly from XNode) — it is exactly like a text node except for how it
     * serializes and its NodeType.
     *
     * @note Content containing the literal sequence `]]>` is **split across adjacent CDATA
     * sections**, matching real .NET's `XmlEncodedRawTextWriter.WriteCDataSection`: the section
     * is closed immediately before the embedded terminator and reopened immediately after its
     * first character, so `left]]>right` serializes as
     * `<![CDATA[left]]]]><![CDATA[>right]]>` and the concatenated value survives a read-back.
     * Note that re-parsing such text yields **two** adjacent CDATA nodes, not one — that is what
     * .NET produces too, and it is why `getValueProperty()` on the containing element, which
     * concatenates, is the round-trip-stable reading. Both serialization doors (`ToString()`/
     * `Save()` and `WriteTo()`) share one definition of the split (ticket #2196, SR-AUD-335);
     * before that, only `WriteTo()` performed it and the direct door emitted the raw terminator.
     */
    class XCData : public XText {
    public:
        /** @brief Initializes a new CDATA text node with the given value. */
        explicit XCData(const std::string& value) : XText(value) {}

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::CDATA; }

        void WriteTo(System::Xml::XmlWriter& writer) const override;

    protected:
        void SerializeTo(std::ostream& os, int depth, bool indent) const override;
    };

} // namespace System::Xml::Linq
