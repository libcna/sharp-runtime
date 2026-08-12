// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XDocumentType.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"

namespace System::Xml::Linq {

    void XDocumentType::WriteTo(System::Xml::XmlWriter& writer) const {
        writer.WriteDocType(name_, publicId_, systemId_, internalSubset_);
    }

    void XDocumentType::SerializeTo(std::ostream& os, int depth, bool indent) const {
        // Ticket #2200. This node kind has TWO serialization doors and they were never twins:
        // WriteTo() above DELEGATES to XmlWriter::WriteDocType, so it inherited #2084's repair
        // with no edit here, while this door DUPLICATED the old three-literal concatenation and
        // did not. Measured (build-probe/2200_probe1_before.log), the two doors disagreed on
        // eleven of eighteen probed documents -- this one emitted `<!DOCTYPE root PUBLIC
        // "pub"lic" "sys"tem" []>]>` for input the writer door rejected outright.
        //
        // The repair is one-path REUSE, not a second implementation: the delimiter decision
        // #2084 settled lives once, in System/Xml/detail/XmlLexicalSanitizer.hpp, and the two
        // identifier validators are the ones this runtime already ships. A DOCTYPE
        // SystemLiteral/PubidLiteral has no escape mechanism at all -- this runtime's own
        // DOCTYPE reader ends a literal at its opening quote and never un-escapes -- so
        // attribute-style escaping would store six characters where a quote was meant. See
        // SelectExternalIdDelimiter's doc-comment for the full argument.
        //
        // Validation runs BEFORE the first byte reaches @p os, so a rejected declaration leaves
        // the stream exactly as it found it.
        //
        // The NAME is validated here for the same reason ticket #2196 validates the processing
        // instruction TARGET at this door rather than at construction: the class doc-comment
        // records non-validation at construction as a deliberate scope decision, and the
        // sibling WriteTo() door has routed the name through XmlConvert::VerifyName since
        // #2076. Serialization is where the two doors are made to agree.
        //
        // NOT repaired here: the internal subset. Its `>` problem is this runtime's
        // `>`-terminated DOCTYPE representation, not a delimiter choice -- an ordinary
        // `<!ENTITY a "b">` needs that `>` -- and it is tracked separately.
        (void)System::Xml::XmlConvert::VerifyName(name_);
        (void)System::Xml::XmlConvert::VerifyPublicId(publicId_);
        (void)System::Xml::XmlConvert::VerifyXmlChars(systemId_);
        if (System::Xml::detail::ExternalIdLiteralTerminatesDeclaration(systemId_))
            throw System::Xml::XmlException(
                "XDocumentType::SerializeTo: the system identifier contains '>', which would "
                "terminate the DOCTYPE declaration: '" + systemId_ + "'.");
        const char systemQuote = System::Xml::detail::SelectExternalIdDelimiter(systemId_);
        if (systemQuote == '\0')
            throw System::Xml::XmlException(
                "XDocumentType::SerializeTo: the system identifier contains both a double quote "
                "and an apostrophe and cannot be represented in a DOCTYPE system literal: '" +
                systemId_ + "'.");

        if (indent) os << std::string(static_cast<size_t>(depth) * 2, ' ');
        os << "<!DOCTYPE " << name_;
        if (!publicId_.empty()) {
            os << " PUBLIC \"" << publicId_ << "\" " << systemQuote << systemId_ << systemQuote;
        } else if (!systemId_.empty()) {
            os << " SYSTEM " << systemQuote << systemId_ << systemQuote;
        }
        if (!internalSubset_.empty()) os << " [" << internalSubset_ << "]";
        os << ">";
    }

} // namespace System::Xml::Linq
