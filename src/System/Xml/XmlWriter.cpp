// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlWriter.hpp"
#include <tinyxml2/tinyxml2.h>
#include "System/Xml/XmlException.hpp"
#include <stack>

namespace System::Xml {

// ---------------------------------------------------------------------------
// Opaque state
// ---------------------------------------------------------------------------

struct XmlWriterState {
    tinyxml2::XMLDocument          doc;
    std::stack<tinyxml2::XMLNode*> nodeStack;  // top = current parent
    std::string                    filePath;
    bool                           hasDeclaration = false;
};

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

XmlWriter::XmlWriter(std::unique_ptr<XmlWriterState> s) : state_(std::move(s)) {
    // Start with the document as the root parent
    state_->nodeStack.push(&state_->doc);
}

XmlWriter::~XmlWriter() { Close(); }

// ---------------------------------------------------------------------------
// Write methods
// ---------------------------------------------------------------------------

void XmlWriter::WriteStartDocument() {
    if (!state_ || state_->hasDeclaration) return;
    state_->doc.InsertFirstChild(state_->doc.NewDeclaration());
    state_->hasDeclaration = true;
}

void XmlWriter::WriteEndDocument() {
    Flush();
}

void XmlWriter::WriteStartElement(const std::string& localName) {
    if (!state_ || state_->nodeStack.empty()) return;
    tinyxml2::XMLElement* el = state_->doc.NewElement(localName.c_str());
    state_->nodeStack.top()->InsertEndChild(el);
    state_->nodeStack.push(el);
}

void XmlWriter::WriteEndElement() {
    if (!state_ || state_->nodeStack.size() <= 1) return; // never pop the document
    state_->nodeStack.pop();
}

void XmlWriter::WriteAttributeString(const std::string& name, const std::string& value) {
    if (!state_ || state_->nodeStack.empty()) return;
    auto* el = state_->nodeStack.top()->ToElement();
    if (el) el->SetAttribute(name.c_str(), value.c_str());
}

void XmlWriter::WriteString(const std::string& text) {
    if (!state_ || state_->nodeStack.empty()) return;
    tinyxml2::XMLText* tn = state_->doc.NewText(text.c_str());
    state_->nodeStack.top()->InsertEndChild(tn);
}

void XmlWriter::WriteElementString(const std::string& name, const std::string& value) {
    WriteStartElement(name);
    WriteString(value);
    WriteEndElement();
}

void XmlWriter::WriteComment(const std::string& text) {
    if (!state_ || state_->nodeStack.empty()) return;
    tinyxml2::XMLComment* cmt = state_->doc.NewComment(text.c_str());
    state_->nodeStack.top()->InsertEndChild(cmt);
}

std::string XmlWriter::ToString() const {
    if (!state_) return {};
    tinyxml2::XMLPrinter printer;
    state_->doc.Print(&printer);
    return printer.CStr() ? printer.CStr() : "";
}

void XmlWriter::Flush() {
    if (!state_ || state_->filePath.empty()) return;
    if (state_->doc.SaveFile(state_->filePath.c_str()) != tinyxml2::XML_SUCCESS)
        throw XmlException("XmlWriter: failed to save file: " + state_->filePath);
}

void XmlWriter::Close() {
    if (!state_) return;
    Flush();
    // clear the stack
    while (!state_->nodeStack.empty()) state_->nodeStack.pop();
}

// ---------------------------------------------------------------------------
// Factory methods
// ---------------------------------------------------------------------------

XmlWriter* XmlWriter::Create(const std::string& outputFileName) {
    auto st = std::make_unique<XmlWriterState>();
    st->filePath = outputFileName;
    return new XmlWriter(std::move(st));
}

XmlWriter* XmlWriter::CreateToString() {
    auto st = std::make_unique<XmlWriterState>();
    return new XmlWriter(std::move(st));
}

} // namespace System::Xml
