// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlDocument.hpp"
#include <sstream>
#include <fstream>

#include <cctype>
#include <vector>
#include <string>
#include <tinyxml2/tinyxml2.h>

#include "System/ArgumentException.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"

namespace System::Xml {

    namespace {

        // ===================================================================================
        // Ticket #2082 -- an undeclared entity reference.
        //
        // Measured by #2073's public-input sweep: LoadXml("<r>&nope;</r>") was ACCEPTED and
        // round-tripped as "<r>&amp;nope;</r>", so the DOCUMENT'S OWN TEXT CHANGED. That is
        // worse than mere acceptance: a caller who loads and saves a document silently rewrites
        // it. .NET throws XmlException("Reference to undeclared entity '{0}'.")
        // (XmlTextReaderImpl.cs:3829, Strings.resx Xml_UndeclaredEntity).
        //
        // THE CHECK MUST RUN ON THE RAW TEXT, and that is not a preference. tinyxml2 DECODES
        // the five predefined entities during parsing, so by the time the tree exists,
        // "&amp;nope;" (legal -- the text `&nope;`) and "&nope;" (undeclared) are the same five
        // characters in the same text node. A post-parse walk cannot tell them apart.
        // ===================================================================================

        /** @brief XML Names 1.0 NameStartChar, as far as an entity name needs it. */
        bool IsEntityNameStart(unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':' ||
                   c >= 0x80;   // any non-ASCII byte: this scan does not decode UTF-8
        }

        /** @brief XML Names 1.0 NameChar. */
        bool IsEntityNameChar(unsigned char c) {
            return IsEntityNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
        }

        /**
         * @brief Collects the general-entity names declared in a DOCTYPE internal subset.
         *
         * Ticket #2082. The check below must reject an **undeclared** entity, not merely a
         * non-predefined one, and those are different sets: a document may declare its own with
         * `<!ENTITY name "...">`. Getting this wrong was caught immediately -- the first cut
         * rejected the repository's own billion-laughs pin, which declares two entities and
         * then references one.
         *
         * Parameter entities (`<!ENTITY % name ...>`) are skipped: they are referenced with `%`
         * inside the DTD, never with `&` in content, so they cannot declare a content entity.
         */
        std::vector<std::string> DeclaredEntityNames(const std::string& xml) {
            std::vector<std::string> names;
            const std::size_t doctype = xml.find("<!DOCTYPE");
            if (doctype == std::string::npos) return names;
            const std::size_t open = xml.find('[', doctype);
            if (open == std::string::npos) return names;
            const std::size_t close = xml.find(']', open);
            if (close == std::string::npos) return names;

            const std::string subset = xml.substr(open + 1, close - open - 1);
            for (std::size_t i = subset.find("<!ENTITY"); i != std::string::npos;
                 i = subset.find("<!ENTITY", i + 1)) {
                std::size_t j = i + 8;
                while (j < subset.size() && std::isspace(static_cast<unsigned char>(subset[j]))) ++j;
                if (j < subset.size() && subset[j] == '%') continue;   // a parameter entity
                const std::size_t nameStart = j;
                while (j < subset.size() && IsEntityNameChar(static_cast<unsigned char>(subset[j]))) ++j;
                if (j > nameStart) names.push_back(subset.substr(nameStart, j - nameStart));
            }
            return names;
        }

        /**
         * @brief Rejects any entity reference that is neither predefined, nor a character
         *        reference, nor declared by the document's own DOCTYPE internal subset.
         *
         * Regions where `&` is ordinary data are skipped: comments, CDATA sections, processing
         * instructions, and the DOCTYPE internal subset itself, whose ENTITY declarations
         * legitimately contain references.
         *
         * A bare `&` that begins no complete reference is left alone. That is a different
         * well-formedness rule and a different finding; widening this check to cover it would
         * reject input on a premise this ticket did not establish.
         *
         * @note A **declared** entity is accepted and, as before, is **not expanded** -- it
         *       stays inert literal text. That parity gap is pre-existing, is deliberately not
         *       touched here, and is what keeps this port free of the billion-laughs exposure
         *       (`XmlContractPinTests.InternalEntitiesAreNeverExpanded_NoBillionLaughsExposure`).
         *       This ticket is about the **undeclared** case, where .NET throws and this port
         *       silently rewrote the document's own text.
         */
        /**
         * @brief Reads @p filename whole, in binary, into @p contents.
         *
         * Ticket #2361. `XmlDocument::Load` needs the raw bytes so that
         * ThrowIfUndeclaredEntityReference can run at that door too, and binary mode matters:
         * a text-mode read on Windows would collapse CRLF and change offsets the scanner walks.
         *
         * @return @c false if the file cannot be opened or read; the caller then lets tinyxml2
         *         categorise the failure so the exception text is unchanged.
         */
        bool ReadWholeFile(const std::string& filename, std::string& contents) {
            std::ifstream in(filename, std::ios::binary);
            if (!in) return false;
            std::ostringstream buffer;
            buffer << in.rdbuf();
            if (in.bad()) return false;
            contents = buffer.str();
            return true;
        }

        void ThrowIfUndeclaredEntityReference(const std::string& xml) {
            static const char* const predefined[] = {"amp", "lt", "gt", "quot", "apos"};
            const std::vector<std::string> declaredNames = DeclaredEntityNames(xml);

            for (std::size_t i = 0; i < xml.size();) {
                if (xml.compare(i, 4, "<!--") == 0) {
                    const std::size_t end = xml.find("-->", i + 4);
                    i = (end == std::string::npos) ? xml.size() : end + 3;
                    continue;
                }
                if (xml.compare(i, 9, "<![CDATA[") == 0) {
                    const std::size_t end = xml.find("]]>", i + 9);
                    i = (end == std::string::npos) ? xml.size() : end + 3;
                    continue;
                }
                if (xml.compare(i, 2, "<?") == 0) {
                    const std::size_t end = xml.find("?>", i + 2);
                    i = (end == std::string::npos) ? xml.size() : end + 2;
                    continue;
                }
                if (xml.compare(i, 9, "<!DOCTYPE") == 0) {
                    const std::size_t open  = xml.find('[', i);
                    const std::size_t close = xml.find('>', i);
                    if (open != std::string::npos && (close == std::string::npos || open < close)) {
                        const std::size_t end = xml.find(']', open);
                        i = (end == std::string::npos) ? xml.size() : end + 1;
                    } else {
                        i = (close == std::string::npos) ? xml.size() : close + 1;
                    }
                    continue;
                }

                if (xml[i] != '&') { ++i; continue; }

                std::size_t j = i + 1;
                if (j < xml.size() && xml[j] == '#') { ++i; continue; }   // character reference
                if (j >= xml.size() || !IsEntityNameStart(static_cast<unsigned char>(xml[j]))) {
                    ++i;   // a bare '&' -- see the doc-comment
                    continue;
                }
                while (j < xml.size() && IsEntityNameChar(static_cast<unsigned char>(xml[j]))) ++j;
                if (j >= xml.size() || xml[j] != ';') { ++i; continue; }   // incomplete

                const std::string name = xml.substr(i + 1, j - i - 1);
                bool known = false;
                for (const char* candidate : predefined) {
                    if (name == candidate) { known = true; break; }
                }
                for (const std::string& candidate : declaredNames) {
                    if (known) break;
                    if (name == candidate) known = true;
                }
                if (!known) throw XmlException("Reference to undeclared entity '" + name + "'.");
                i = j + 1;
            }
        }

        // ===================================================================================
        // Ticket #2083 -- an undeclared namespace prefix.
        //
        // Measured by #2073's public-input sweep: LoadXml("<p:r/>") was ACCEPTED with no
        // namespace resolution and round-tripped unchanged, so a document naming a namespace it
        // never declares looked well-formed. .NET throws
        // XmlException("'{0}' is an undeclared prefix.") (XmlTextReaderImpl.cs:7787,
        // Strings.resx Xml_UnknownNs).
        //
        // Unlike #2082 this CAN be checked on the parsed tree, because a prefix survives parsing
        // intact.
        // ===================================================================================

        /** @brief The prefix of a qualified name, or "" when it has none. */
        std::string PrefixOf(const char* qualifiedName) {
            if (!qualifiedName) return "";
            const std::string name(qualifiedName);
            const std::size_t colon = name.find(':');
            return colon == std::string::npos ? std::string() : name.substr(0, colon);
        }

        void ThrowIfUndeclaredPrefix(const tinyxml2::XMLElement* element,
                                     std::vector<std::string> inScope) {
            if (!element) return;

            // Declarations on THIS element are in scope for it, including for its own name --
            // XML Names 1.0 3: the scope of a declaration includes the start-tag it appears on.
            for (const tinyxml2::XMLAttribute* a = element->FirstAttribute(); a; a = a->Next()) {
                const std::string attributeName(a->Name() ? a->Name() : "");
                if (attributeName.rfind("xmlns:", 0) == 0 && attributeName.size() > 6) {
                    inScope.push_back(attributeName.substr(6));
                }
            }

            const auto declared = [&inScope](const std::string& prefix) {
                // "xml" is bound by the specification itself and is never declared; "xmlns" is
                // reserved and never appears as a name's prefix outside a declaration.
                if (prefix.empty() || prefix == "xml" || prefix == "xmlns") return true;
                for (const std::string& candidate : inScope) {
                    if (candidate == prefix) return true;
                }
                return false;
            };

            const std::string elementPrefix = PrefixOf(element->Name());
            if (!declared(elementPrefix)) {
                throw XmlException("'" + elementPrefix + "' is an undeclared prefix.");
            }
            for (const tinyxml2::XMLAttribute* a = element->FirstAttribute(); a; a = a->Next()) {
                const std::string attributeName(a->Name() ? a->Name() : "");
                if (attributeName.rfind("xmlns:", 0) == 0 || attributeName == "xmlns") continue;
                const std::string attributePrefix = PrefixOf(a->Name());
                if (!declared(attributePrefix)) {
                    throw XmlException("'" + attributePrefix + "' is an undeclared prefix.");
                }
            }

            for (const tinyxml2::XMLElement* child = element->FirstChildElement(); child;
                 child = child->NextSiblingElement()) {
                ThrowIfUndeclaredPrefix(child, inScope);
            }
        }

        // VersionNum ::= '1.' [0-9]+  (XmlDeclaration.cs IsValidXmlVersion)
        bool IsValidXmlVersion(const std::string& ver) {
            if (ver.size() < 3 || ver[0] != '1' || ver[1] != '.') return false;
            for (size_t i = 2; i < ver.size(); ++i)
                if (!std::isdigit(static_cast<unsigned char>(ver[i]))) return false;
            return true;
        }

        // Matches XmlCharType.IsOnlyWhitespace / the XML S production: exactly tab, LF, CR,
        // space -- used by XmlWhitespace.cs/XmlSignificantWhiteSpace.cs's constructors
        // (CheckOnData) to reject non-whitespace content. An empty string is vacuously valid.
        bool IsOnlyXmlWhitespace(const std::string& s) {
            for (char c : s)
                if (c != '\t' && c != '\n' && c != '\r' && c != ' ') return false;
            return true;
        }

        bool StartsWithDoctype(const char* v) {
            if (!v) return false;
            std::string s(v);
            return s.rfind("DOCTYPE", 0) == 0;
        }

        // Best-effort DOCTYPE tokenizer: "DOCTYPE name [PUBLIC "pub" "sys" | SYSTEM "sys"]".
        // See XmlDocumentType's doc-comment for why this can't be fully correct for
        // internal-subset content (tinyxml2 itself can't parse past an internal ">" there).
        void ParseDoctype(const std::string& raw, std::string& name, std::string& publicId, std::string& systemId) {
            size_t pos = raw.find("DOCTYPE");
            pos += 7;
            auto skipWs = [&](size_t p) { while (p < raw.size() && std::isspace(static_cast<unsigned char>(raw[p]))) ++p; return p; };
            pos = skipWs(pos);
            size_t nameEnd = raw.find_first_of(" \t\r\n", pos);
            name = raw.substr(pos, nameEnd == std::string::npos ? std::string::npos : nameEnd - pos);
            pos = nameEnd == std::string::npos ? raw.size() : skipWs(nameEnd);

            auto readQuoted = [&](size_t& p) -> std::string {
                if (p >= raw.size() || (raw[p] != '"' && raw[p] != '\'')) return {};
                char q = raw[p];
                size_t start = p + 1;
                size_t end = raw.find(q, start);
                if (end == std::string::npos) { p = raw.size(); return {}; }
                p = end + 1;
                return raw.substr(start, end - start);
            };

            if (raw.compare(pos, 6, "PUBLIC") == 0) {
                pos = skipWs(pos + 6);
                publicId = readQuoted(pos);
                pos = skipWs(pos);
                systemId = readQuoted(pos);
            } else if (raw.compare(pos, 6, "SYSTEM") == 0) {
                pos = skipWs(pos + 6);
                systemId = readQuoted(pos);
            }
        }
    }

    XmlDocument::XmlDocument()
        : nameTable_(std::make_shared<NameTable>()) {
        implementation_ = std::make_unique<XmlImplementation>(nameTable_);
        native_ = &doc_;
        ownerDocument_ = nullptr;
        detachedHolder_ = doc_.NewElement("#detached-holder");
    }

    XmlDocument::XmlDocument(std::shared_ptr<XmlNameTable> nt)
        : nameTable_(std::move(nt)) {
        implementation_ = std::make_unique<XmlImplementation>(nameTable_);
        native_ = &doc_;
        ownerDocument_ = nullptr;
        detachedHolder_ = doc_.NewElement("#detached-holder");
    }

    XmlDocument::~XmlDocument() = default;

    void XmlDocument::PurgeCache(tinyxml2::XMLNode* native) {
        if (!native) return;
        for (tinyxml2::XMLNode* child = native->FirstChild(); child; child = child->NextSibling())
            PurgeCache(child);
        nodeCache_.erase(native);
    }

    void XmlDocument::DetachNode(tinyxml2::XMLNode* native) {
        if (!native || !detachedHolder_) return;
        detachedHolder_->InsertEndChild(native);
    }

    bool XmlDocument::IsDetached(const tinyxml2::XMLNode* native) const {
        return native == static_cast<const tinyxml2::XMLNode*>(detachedHolder_) ||
               (native && native->Parent() == static_cast<const tinyxml2::XMLNode*>(detachedHolder_));
    }

    void XmlDocument::ReleaseUnattachedNode(XmlNode* node) {
        for (auto it = unattachedNodes_.begin(); it != unattachedNodes_.end(); ++it) {
            if (it->get() == node) {
                it->release();
                unattachedNodes_.erase(it);
                return;
            }
        }
    }

    XmlNode* XmlDocument::WrapNode(tinyxml2::XMLNode* native) {
        if (!native) return nullptr;
        if (native == static_cast<tinyxml2::XMLNode*>(&doc_)) return this;

        auto it = nodeCache_.find(native);
        if (it != nodeCache_.end()) return it->second.get();

        std::unique_ptr<XmlNode> wrapper;
        if (native->ToElement()) {
            wrapper = std::make_unique<XmlElement>(native, this);
        } else if (auto* txt = native->ToText()) {
            wrapper = txt->CData() ? std::unique_ptr<XmlNode>(std::make_unique<XmlCDataSection>(native, this))
                                   : std::unique_ptr<XmlNode>(std::make_unique<XmlText>(native, this));
        } else if (native->ToComment()) {
            wrapper = std::make_unique<XmlComment>(native, this);
        } else if (auto* decl = native->ToDeclaration()) {
            const char* v = decl->Value();
            bool isXmlDecl = v && (std::string(v).rfind("xml", 0) == 0) &&
                             (v[3] == '\0' || std::isspace(static_cast<unsigned char>(v[3])));
            wrapper = isXmlDecl ? std::unique_ptr<XmlNode>(std::make_unique<XmlDeclaration>(native, this))
                                : std::unique_ptr<XmlNode>(std::make_unique<XmlProcessingInstruction>(native, this));
        } else if (auto* unk = native->ToUnknown()) {
            std::string name, publicId, systemId;
            if (StartsWithDoctype(unk->Value())) {
                ParseDoctype(unk->Value(), name, publicId, systemId);
            }
            wrapper = std::make_unique<XmlDocumentType>(native, this, name, publicId, systemId);
        } else {
            return nullptr;
        }

        auto* raw = wrapper.get();
        nodeCache_.emplace(native, std::move(wrapper));
        return raw;
    }

    XmlElement* XmlDocument::getDocumentElementProperty() const {
        auto* root = const_cast<tinyxml2::XMLDocument&>(doc_).RootElement();
        return root ? static_cast<XmlElement*>(const_cast<XmlDocument*>(this)->WrapNode(root)) : nullptr;
    }

    XmlDocumentType* XmlDocument::getDocumentTypeProperty() const {
        for (const tinyxml2::XMLNode* n = doc_.FirstChild(); n; n = n->NextSibling())
            if (auto* unk = n->ToUnknown())
                if (StartsWithDoctype(unk->Value()))
                    return static_cast<XmlDocumentType*>(const_cast<XmlDocument*>(this)->WrapNode(const_cast<tinyxml2::XMLNode*>(n)));
        return nullptr;
    }

    XmlAttribute* XmlDocument::CreateAttribute(const std::string& name) {
        // Verified against XmlDocument.cs's CreateElement/CreateAttribute -> XmlDocument.CheckName
        // (called on the split prefix and local name): both throw XmlException for a
        // malformed XML name (e.g. empty, or starting with a digit) instead of silently
        // accepting it and producing unparseable markup on write-out.
        (void)XmlConvert::VerifyName(name);
        // Owned by this document (unattachedNodes_) until XmlElement::SetAttributeNode claims it
        // via ReleaseUnattachedNode -- see unattachedNodes_'s own comment for why (XmlAttribute
        // has no native tinyxml2 node to key nodeCache_ by, unlike every other Create* method).
        auto attr = std::make_unique<XmlAttribute>(this, name);
        auto* raw = attr.get();
        unattachedNodes_.push_back(std::move(attr));
        return raw;
    }

    XmlElement* XmlDocument::CreateElement(const std::string& name) {
        (void)XmlConvert::VerifyName(name);
        auto* native = doc_.NewElement(name.c_str());
        return static_cast<XmlElement*>(WrapNode(native));
    }

    XmlText* XmlDocument::CreateTextNode(const std::string& text) {
        auto* native = doc_.NewText(text.c_str());
        return static_cast<XmlText*>(WrapNode(native));
    }

    XmlComment* XmlDocument::CreateComment(const std::string& data) {
        auto* native = doc_.NewComment(data.c_str());
        return static_cast<XmlComment*>(WrapNode(native));
    }

    XmlCDataSection* XmlDocument::CreateCDataSection(const std::string& data) {
        auto* native = doc_.NewText(data.c_str());
        native->SetCData(true);
        return static_cast<XmlCDataSection*>(WrapNode(native));
    }

    XmlProcessingInstruction* XmlDocument::CreateProcessingInstruction(const std::string& target, const std::string& data) {
        // Matches XmlProcessingInstruction.cs's constructor: only an empty target is rejected
        // (ArgumentException.ThrowIfNullOrEmpty) -- unlike CreateElement/CreateAttribute, the
        // DOM does not additionally require target to be a well-formed NCName at construction.
        if (target.empty())
            throw System::ArgumentException("Value cannot be null or empty.", "target");
        std::string text = data.empty() ? target : target + " " + data;
        auto* native = doc_.NewDeclaration(text.c_str());
        return static_cast<XmlProcessingInstruction*>(WrapNode(native));
    }

    XmlDeclaration* XmlDocument::CreateXmlDeclaration(const std::string& version, const std::string& encoding, const std::string& standalone) {
        if (!IsValidXmlVersion(version))
            throw System::ArgumentException("Wrong XML version information. The XML must match production \"VersionNum ::= '1.' [0-9]+\".");
        if (!standalone.empty() && standalone != "yes" && standalone != "no")
            throw System::ArgumentException("Wrong value for the XML declaration standalone attribute of '" + standalone + "'.");
        std::string text = "xml version=\"" + version + "\"";
        if (!encoding.empty()) text += " encoding=\"" + encoding + "\"";
        if (!standalone.empty()) text += " standalone=\"" + standalone + "\"";
        auto* native = doc_.NewDeclaration(text.c_str());
        return static_cast<XmlDeclaration*>(WrapNode(native));
    }

    XmlDocumentFragment* XmlDocument::CreateDocumentFragment() {
        // Scratch element that is never inserted into the visible tree; see the class doc-comment.
        auto* native = doc_.NewElement("#document-fragment");
        auto wrapper = std::make_unique<XmlDocumentFragment>(native, this);
        auto* raw = wrapper.get();
        nodeCache_.emplace(native, std::move(wrapper));
        return raw;
    }

    XmlDocumentType* XmlDocument::CreateDocumentType(const std::string& name, const std::string& publicId,
                                                     const std::string& systemId, const std::string& /*internalSubset*/) {
        // Ticket #2084: the SECOND producer of an ExternalID in this module. It builds the
        // same three-literal text by the same raw concatenation as XmlWriter::WriteDocType,
        // so it carried the same defect and takes the same repair -- the finding named only
        // the writer door. Validation runs before the node is created, so a rejected call
        // leaves the document untouched.
        (void)XmlConvert::VerifyName(name);
        (void)XmlConvert::VerifyPublicId(publicId);
        (void)XmlConvert::VerifyXmlChars(systemId);
        if (detail::ExternalIdLiteralTerminatesDeclaration(systemId))
            throw XmlException("XmlDocument::CreateDocumentType: the system identifier contains '>', "
                               "which would terminate the DOCTYPE declaration: '" + systemId + "'.");
        const char systemQuote = detail::SelectExternalIdDelimiter(systemId);
        if (systemQuote == '\0')
            throw XmlException("XmlDocument::CreateDocumentType: the system identifier contains "
                               "both a double quote and an apostrophe and cannot be represented "
                               "in a DOCTYPE system literal: '" + systemId + "'.");
        std::string text = "DOCTYPE " + name;
        if (!publicId.empty()) text += " PUBLIC \"" + publicId + "\" " + systemQuote + systemId + systemQuote;
        else if (!systemId.empty()) { text += " SYSTEM "; text += systemQuote + systemId + systemQuote; }
        auto* native = doc_.NewUnknown(text.c_str());
        auto wrapper = std::make_unique<XmlDocumentType>(native, this, name, publicId, systemId);
        auto* raw = wrapper.get();
        nodeCache_.emplace(native, std::move(wrapper));
        return raw;
    }

    XmlEntityReference* XmlDocument::CreateEntityReference(const std::string& name) {
        // Matches XmlEntityReference.cs's constructor: entity reference names are not required
        // to be well-formed NCNames (they may reference arbitrary DTD-declared entities) -- the
        // only rejected form is one starting with '#', which would collide with a character
        // reference (e.g. "&#65;").
        if (!name.empty() && name[0] == '#')
            throw System::ArgumentException("An entity reference must not start with '#'.", "name");
        // Owned by this document (unattachedNodes_) for the same reason as CreateAttribute above.
        // Unlike XmlAttribute, nothing in this port currently attaches an XmlEntityReference to a
        // tree (it has no native tinyxml2 backing and AppendChild/InsertBefore/etc. all require
        // one), so in practice this entry is never released via ReleaseUnattachedNode and simply
        // lives until the document itself is destroyed -- still correct, just always the
        // "never claimed" branch of the same ownership mechanism.
        auto ref = std::make_unique<XmlEntityReference>(this, name);
        auto* raw = ref.get();
        unattachedNodes_.push_back(std::move(ref));
        return raw;
    }

    XmlWhitespace* XmlDocument::CreateWhitespace(const std::string& text) {
        // Matches XmlWhitespace.cs's constructor: `if (!doc.IsLoading && !CheckOnData(strData))
        // throw new ArgumentException(SR.Xdom_WS_Char);` -- content must be only XML whitespace
        // characters when created programmatically (not during parsing).
        if (!IsOnlyXmlWhitespace(text))
            throw System::ArgumentException("The string for whitespace contains an invalid character.");
        auto* native = doc_.NewText(text.c_str());
        auto wrapper = std::make_unique<XmlWhitespace>(native, this);
        auto* raw = wrapper.get();
        nodeCache_.emplace(native, std::move(wrapper));
        return raw;
    }

    XmlSignificantWhitespace* XmlDocument::CreateSignificantWhitespace(const std::string& text) {
        // Matches XmlSignificantWhiteSpace.cs's constructor: same CheckOnData validation as
        // XmlWhitespace above.
        if (!IsOnlyXmlWhitespace(text))
            throw System::ArgumentException("The string for whitespace contains an invalid character.");
        auto* native = doc_.NewText(text.c_str());
        auto wrapper = std::make_unique<XmlSignificantWhitespace>(native, this);
        auto* raw = wrapper.get();
        nodeCache_.emplace(native, std::move(wrapper));
        return raw;
    }

    namespace {
        void CollectDocElementsByName(tinyxml2::XMLNode* node, const std::string& name,
                                      XmlDocument* doc, std::vector<XmlNode*>& out) {
            for (tinyxml2::XMLNode* child = node->FirstChild(); child; child = child->NextSibling()) {
                if (auto* el = child->ToElement()) {
                    if (name == "*" || (el->Name() && name == el->Name()))
                        out.push_back(doc->WrapNode(child));
                    CollectDocElementsByName(child, name, doc, out);
                }
            }
        }

        const tinyxml2::XMLElement* FindElementById(const tinyxml2::XMLElement* el, const std::string& id) {
            const char* v = el->Attribute("id");
            if (v && id == v) return el;
            for (const tinyxml2::XMLElement* child = el->FirstChildElement(); child; child = child->NextSiblingElement()) {
                if (auto* found = FindElementById(child, id)) return found;
            }
            return nullptr;
        }
    }

    XmlNodeList* XmlDocument::GetElementsByTagName(const std::string& name) const {
        std::vector<XmlNode*> results;
        CollectDocElementsByName(const_cast<tinyxml2::XMLDocument*>(&doc_), name, const_cast<XmlDocument*>(this), results);
        return new XmlNodeList(std::move(results)); // ownership: caller's responsibility, matching XmlNode::getChildNodesProperty's leak-free
                                                      // pattern is not applied here since XmlDocument has no equivalent cached member;
                                                      // documented as a minor known simplification (rare, one-shot query use).
    }

    XmlElement* XmlDocument::GetElementById(const std::string& elementId) const {
        auto* root = doc_.RootElement();
        if (!root) return nullptr;
        auto* found = FindElementById(root, elementId);
        return found ? static_cast<XmlElement*>(const_cast<XmlDocument*>(this)->WrapNode(const_cast<tinyxml2::XMLElement*>(found))) : nullptr;
    }

    void XmlDocument::Load(const std::string& filename) {
        nodeCache_.clear();

        // #2361 (2026-08-18) closed the asymmetry #2082 left here. The entity check needs the
        // RAW text -- tinyxml2 decodes the five predefined entities during parsing, after which
        // "&amp;nope;" and "&nope;" are indistinguishable -- and this door used to hand the path
        // straight to LoadFile without ever holding the bytes. So a document loaded FROM A FILE
        // accepted an undeclared entity and then silently rewrote it on save, while the identical
        // text through LoadXml was rejected. One door, two answers.
        //
        // This reads the file itself and then parses the buffer, which is what tinyxml2::LoadFile
        // does internally anyway (read whole file, call Parse). The FAILURE path deliberately
        // still goes through LoadFile: it is the thing that produces XML_ERROR_FILE_NOT_FOUND /
        // FILE_COULD_NOT_BE_OPENED / FILE_READ_ERROR and the ErrorStr() this message has always
        // carried, and reproducing those categories by hand would be inventing diagnostics rather
        // than keeping them.
        std::string contents;
        if (!ReadWholeFile(filename, contents)) {
            (void)doc_.LoadFile(filename.c_str());   // let tinyxml2 categorise its own failure
            throw XmlException("XmlDocument::Load: failed to load '" + filename + "': " +
                               (doc_.ErrorStr() ? doc_.ErrorStr() : "unknown error"));
        }
        ThrowIfUndeclaredEntityReference(contents);   // #2082, now at BOTH doors
        if (doc_.Parse(contents.c_str(), contents.size()) != tinyxml2::XML_SUCCESS)
            throw XmlException("XmlDocument::Load: failed to load '" + filename + "': " +
                               (doc_.ErrorStr() ? doc_.ErrorStr() : "unknown error"));
        ThrowIfUndeclaredPrefix(doc_.RootElement(), {});   // #2083, always ran here
        // tinyxml2::XMLDocument::LoadFile() clears and frees every previously-allocated node
        // (including detachedHolder_ from the constructor, or a prior Load/LoadXml call) before
        // parsing; recreate it now, or IsDetached()/getParentNodeProperty() etc. would compare
        // against a dangling pointer into freed (and likely reused) memory.
        detachedHolder_ = doc_.NewElement("#detached-holder");
    }

    void XmlDocument::LoadXml(const std::string& xml) {
        // #2082: BEFORE the parse, because tinyxml2 decodes the predefined entities and the
        // distinction is gone afterwards.
        ThrowIfUndeclaredEntityReference(xml);
        nodeCache_.clear();
        if (doc_.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
            throw XmlException(std::string("XmlDocument::LoadXml: parse error: ") +
                               (doc_.ErrorStr() ? doc_.ErrorStr() : "unknown error"));
        ThrowIfUndeclaredPrefix(doc_.RootElement(), {});   // #2083
        // See the comment in Load() above: Parse() also clears and frees prior nodes.
        detachedHolder_ = doc_.NewElement("#detached-holder");
    }

    void XmlDocument::Save(const std::string& filename) const {
        if (const_cast<tinyxml2::XMLDocument&>(doc_).SaveFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
            throw XmlException("XmlDocument::Save: failed to save '" + filename + "'.");
    }

    void XmlDocument::Save(XmlWriter& writer) const {
        WriteTo(writer);
    }

    std::string XmlDocument::getInnerXmlProperty() const {
        return XmlNode::getInnerXmlProperty();
    }

    void XmlDocument::setInnerXmlProperty(const std::string& xml) {
        LoadXml(xml);
    }

    void XmlDocument::WriteTo(XmlWriter& writer) const {
        WriteContentTo(writer);
    }

    void XmlDocument::WriteContentTo(XmlWriter& writer) const {
        XmlNode::WriteContentTo(writer);
    }

} // namespace System::Xml
