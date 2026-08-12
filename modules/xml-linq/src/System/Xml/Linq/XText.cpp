// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XText.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/detail/XLinqSerializationGuards.hpp"

namespace System::Xml::Linq {

    void XText::WriteTo(System::Xml::XmlWriter& writer) const {
        if (dynamic_cast<XDocument*>(parent_) != nullptr) {
            writer.WriteWhitespace(text_);
        } else {
            writer.WriteString(text_);
        }
    }

    void XText::SerializeTo(std::ostream& os, int /*depth*/, bool /*indent*/) const {
        // Ticket #2201, and the mirror image of #2085: the WriteTo() door above hands the value
        // to XmlWriter, which TRUNCATED at an embedded NUL until #2085 rejected it; this door
        // has no const char* boundary and EMITTED the NUL instead, so a 3-byte value serialised
        // to 3 bytes that this runtime's own reader then rejects with XML_ERROR_PARSING_TEXT.
        // Both doors now give the same answer, using the same single detector.
        detail::ThrowIfContainsNul(text_, "XText::SerializeTo", "text");
        os << EscapeText(text_);
    }

    bool XText::DeepEqualsCore(const XNode& other) const {
        return text_ == static_cast<const XText&>(other).text_;
    }

} // namespace System::Xml::Linq
