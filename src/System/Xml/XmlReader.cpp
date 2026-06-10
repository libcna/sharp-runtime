// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlReader.hpp"
#include <tinyxml2/tinyxml2.h>
#include <stdexcept>
#include <stack>

namespace System::Xml {

// ---------------------------------------------------------------------------
// Internal event model
// ---------------------------------------------------------------------------

struct XmlEvent {
    XmlNodeType                              type         = XmlNodeType::None;
    std::string                              name;
    std::string                              value;
    bool                                     isEmptyElement = false;
    std::vector<std::pair<std::string,std::string>> attributes;
};

// ---------------------------------------------------------------------------
// Opaque state
// ---------------------------------------------------------------------------

struct XmlReaderState {
    tinyxml2::XMLDocument              doc;
    std::vector<XmlEvent>              events;
    int                                pos       = -1;  // before first Read()
    int                                attrIndex = -1;  // attribute cursor (-1 = on element)
    ReadState                          readState = ReadState::Initial;
};

// ---------------------------------------------------------------------------
// DOM → flat event list
// ---------------------------------------------------------------------------

static void buildEvents(tinyxml2::XMLNode* node, std::vector<XmlEvent>& out) {
    if (auto* decl = node->ToDeclaration()) {
        XmlEvent e;
        e.type  = XmlNodeType::XmlDeclaration;
        e.name  = "xml";
        e.value = decl->Value() ? decl->Value() : "";
        out.push_back(std::move(e));
        return;
    }
    if (auto* el = node->ToElement()) {
        XmlEvent e;
        e.type          = XmlNodeType::Element;
        e.name          = el->Name() ? el->Name() : "";
        e.isEmptyElement = !el->FirstChild();
        for (const tinyxml2::XMLAttribute* a = el->FirstAttribute(); a; a = a->Next())
            e.attributes.emplace_back(a->Name() ? a->Name() : "",
                                      a->Value() ? a->Value() : "");
        out.push_back(std::move(e));

        for (tinyxml2::XMLNode* child = el->FirstChild(); child; child = child->NextSibling())
            buildEvents(child, out);

        if (!e.isEmptyElement) {
            XmlEvent ee;
            ee.type = XmlNodeType::EndElement;
            ee.name = el->Name() ? el->Name() : "";
            out.push_back(std::move(ee));
        }
        return;
    }
    if (auto* txt = node->ToText()) {
        XmlEvent e;
        e.type  = XmlNodeType::Text;
        e.value = txt->Value() ? txt->Value() : "";
        out.push_back(std::move(e));
        return;
    }
    if (auto* cmt = node->ToComment()) {
        XmlEvent e;
        e.type  = XmlNodeType::Comment;
        e.value = cmt->Value() ? cmt->Value() : "";
        out.push_back(std::move(e));
        return;
    }
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

XmlReader::XmlReader(std::unique_ptr<XmlReaderState> s) : state_(std::move(s)) {}
XmlReader::~XmlReader() = default;

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

XmlNodeType XmlReader::getNodeTypeProperty() const {
    if (!state_ || state_->pos < 0 ||
        state_->pos >= static_cast<int>(state_->events.size()))
        return XmlNodeType::None;
    if (state_->attrIndex >= 0)
        return XmlNodeType::Attribute;
    return state_->events[static_cast<size_t>(state_->pos)].type;
}

std::string XmlReader::getNameProperty() const {
    if (!state_ || state_->pos < 0) return {};
    const auto& ev = state_->events[static_cast<size_t>(state_->pos)];
    if (state_->attrIndex >= 0 &&
        state_->attrIndex < static_cast<int>(ev.attributes.size()))
        return ev.attributes[static_cast<size_t>(state_->attrIndex)].first;
    return ev.name;
}

std::string XmlReader::getValueProperty() const {
    if (!state_ || state_->pos < 0) return {};
    const auto& ev = state_->events[static_cast<size_t>(state_->pos)];
    if (state_->attrIndex >= 0 &&
        state_->attrIndex < static_cast<int>(ev.attributes.size()))
        return ev.attributes[static_cast<size_t>(state_->attrIndex)].second;
    return ev.value;
}

bool XmlReader::getIsEmptyElementProperty() const {
    if (!state_ || state_->pos < 0) return false;
    return state_->events[static_cast<size_t>(state_->pos)].isEmptyElement;
}

ReadState XmlReader::getReadStateProperty() const {
    return state_ ? state_->readState : ReadState::Closed;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

bool XmlReader::Read() {
    if (!state_) return false;
    state_->attrIndex = -1;
    ++state_->pos;
    if (state_->pos >= static_cast<int>(state_->events.size())) {
        state_->readState = ReadState::EndOfFile;
        return false;
    }
    state_->readState = ReadState::Interactive;
    return true;
}

bool XmlReader::MoveToElement() {
    if (!state_ || state_->pos < 0) return false;
    state_->attrIndex = -1;
    return state_->events[static_cast<size_t>(state_->pos)].type == XmlNodeType::Element;
}

bool XmlReader::MoveToNextAttribute() {
    if (!state_ || state_->pos < 0) return false;
    const auto& ev = state_->events[static_cast<size_t>(state_->pos)];
    if (ev.type != XmlNodeType::Element) return false;
    int next = state_->attrIndex + 1;
    if (next >= static_cast<int>(ev.attributes.size())) return false;
    state_->attrIndex = next;
    return true;
}

std::string XmlReader::GetAttribute(const std::string& name) const {
    if (!state_ || state_->pos < 0) return {};
    const auto& ev = state_->events[static_cast<size_t>(state_->pos)];
    for (const auto& [k, v] : ev.attributes)
        if (k == name) return v;
    return {};
}

std::string XmlReader::ReadElementContentAsString() {
    if (!state_) return {};
    std::string result;
    // advance past the Element node we're sitting on
    Read();
    while (state_->pos >= 0 &&
           state_->pos < static_cast<int>(state_->events.size())) {
        const auto& ev = state_->events[static_cast<size_t>(state_->pos)];
        if (ev.type == XmlNodeType::EndElement) {
            Read(); // consume the end-element
            break;
        }
        if (ev.type == XmlNodeType::Text || ev.type == XmlNodeType::CDATA)
            result += ev.value;
        Read();
    }
    return result;
}

void XmlReader::ReadStartElement() {
    if (!state_ || state_->pos < 0 ||
        state_->events[static_cast<size_t>(state_->pos)].type != XmlNodeType::Element)
        throw std::runtime_error("XmlReader::ReadStartElement: not on an element node");
    Read();
}

void XmlReader::ReadEndElement() {
    if (!state_ || state_->pos < 0 ||
        state_->events[static_cast<size_t>(state_->pos)].type != XmlNodeType::EndElement)
        throw std::runtime_error("XmlReader::ReadEndElement: not on an end-element node");
    Read();
}

void XmlReader::Close() {
    if (state_) state_->readState = ReadState::Closed;
}

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------

static XmlReader* createFromDoc(std::unique_ptr<XmlReaderState> st) {
    if (st->doc.Error())
        throw std::runtime_error(std::string("XmlReader: parse error: ") +
                                 (st->doc.ErrorStr() ? st->doc.ErrorStr() : ""));
    // Walk all top-level nodes (declaration + root element)
    for (tinyxml2::XMLNode* n = st->doc.FirstChild(); n; n = n->NextSibling())
        buildEvents(n, st->events);
    return new XmlReader(std::move(st));
}

XmlReader* XmlReader::Create(const std::string& inputUri) {
    auto st = std::make_unique<XmlReaderState>();
    // Heuristic: treat as file path if it ends with .xml or contains a path separator
    bool isFile = (inputUri.find('/') != std::string::npos ||
                   inputUri.find('\\') != std::string::npos ||
                   (inputUri.size() >= 4 &&
                    inputUri.substr(inputUri.size() - 4) == ".xml"));
    if (isFile)
        st->doc.LoadFile(inputUri.c_str());
    else
        st->doc.Parse(inputUri.c_str());
    return createFromDoc(std::move(st));
}

XmlReader* XmlReader::CreateFromString(const std::string& xmlContent) {
    auto st = std::make_unique<XmlReaderState>();
    st->doc.Parse(xmlContent.c_str());
    return createFromDoc(std::move(st));
}

} // namespace System::Xml
