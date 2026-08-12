// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XProcessingInstruction.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"
#include "System/Xml/Linq/detail/XLinqSerializationGuards.hpp"

namespace System::Xml::Linq {

    void XProcessingInstruction::WriteTo(System::Xml::XmlWriter& writer) const {
        writer.WriteProcessingInstruction(target_, data_);
    }

    void XProcessingInstruction::SerializeTo(std::ostream& os, int depth, bool indent) const {
        // Ticket #2196 (SR-AUD-335), and this node kind has TWO doors, which is why it is the
        // one the audit's three-type summary understated.
        //
        // The DATA used to be written verbatim, so `left?>right` emitted `<?p left?>right?>`,
        // whose embedded `?>` closed the instruction early. Unlike the comment case this one is
        // not silent: re-reading that text throws XML_ERROR_PARSING_DECLARATION, so a
        // ToString()/Save() round trip failed outright.
        //
        // The TARGET is an XML name and had no validator here at all, while the sibling
        // WriteTo() door has always routed it through XmlConvert::VerifyName -- measured, the
        // two doors disagreed completely for `XProcessingInstruction("a?>b", "d")`: this one
        // emitted `<?a?>b d?>` and the writer threw. Validation is applied here rather than in
        // the constructor/setter on purpose: the class doc-comment records non-validation at
        // construction as a deliberate scope decision, and narrowing THAT door is a wider
        // accepted-input change than this finding calls for. Serialization is where the writer
        // already rejects, so this is where the two doors are made to agree.
        (void)System::Xml::XmlConvert::VerifyName(target_);
        // Ticket #2201. The TARGET needs no separate guard -- VerifyName above already rejects a
        // NUL, since it is not an NCName character -- but the DATA had none.
        detail::ThrowIfContainsNul(data_, "XProcessingInstruction::SerializeTo",
                                   "processing-instruction data");
        if (indent) os << std::string(static_cast<size_t>(depth) * 2, ' ');
        os << "<?" << target_;
        if (!data_.empty()) os << " " << System::Xml::detail::SanitizeProcessingInstructionText(data_);
        os << "?>";
    }

} // namespace System::Xml::Linq
