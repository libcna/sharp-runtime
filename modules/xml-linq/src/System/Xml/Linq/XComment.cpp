// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"
#include "System/Xml/Linq/detail/XLinqSerializationGuards.hpp"

namespace System::Xml::Linq {

    void XComment::WriteTo(System::Xml::XmlWriter& writer) const {
        writer.WriteComment(value_);
    }

    void XComment::SerializeTo(std::ostream& os, int depth, bool indent) const {
        // Ticket #2196 (SR-AUD-335). This door used to write the value verbatim, so
        // `left--right` emitted `<!--left--right-->` and `trailing-` emitted
        // `<!--trailing--->`, neither of which is a well-formed XML comment. The corruption
        // was *silent*: the tinyxml2-backed parser accepts both on read-back, so nothing
        // signalled it. The sibling WriteTo() door has always inserted the protective space.
        // Ticket #2201: an embedded NUL is emitted rather than protected, and no amount of
        // hyphen protection makes the result readable.
        detail::ThrowIfContainsNul(value_, "XComment::SerializeTo", "comment text");
        if (indent) os << std::string(static_cast<size_t>(depth) * 2, ' ');
        os << "<!--" << System::Xml::detail::SanitizeCommentText(value_) << "-->";
    }

} // namespace System::Xml::Linq
