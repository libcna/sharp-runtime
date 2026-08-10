// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XCData.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"

namespace System::Xml::Linq {

    void XCData::WriteTo(System::Xml::XmlWriter& writer) const {
        writer.WriteCData(getValueProperty());
    }

    void XCData::SerializeTo(std::ostream& os, int /*depth*/, bool /*indent*/) const {
        // Ticket #2196 (SR-AUD-335). This door used to write the value verbatim, so a value
        // containing "]]>" closed its own section: `left]]>right` emitted
        // `<![CDATA[left]]>right]]>`, which re-read as a CDATA node `left` plus a TEXT node
        // `right]]>` -- one node became two and the concatenated value became `leftright]]>`.
        // The sibling WriteTo() door above was never affected, because XmlWriter::WriteCData
        // has always split the section. Both doors now share one definition of the split.
        os << "<![CDATA[" << System::Xml::detail::SanitizeCDataText(getValueProperty()) << "]]>";
    }

} // namespace System::Xml::Linq
