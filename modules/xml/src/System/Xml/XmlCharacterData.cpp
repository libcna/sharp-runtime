// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlCharacterData.hpp"

#include <tinyxml2/tinyxml2.h>

#include "System/ArgumentOutOfRangeException.hpp"
#include "XmlNodeChangeEvents.hpp"
#include "System/Xml/XmlWriter.hpp"

namespace System::Xml {

    namespace {
        tinyxml2::XMLText* AsText(tinyxml2::XMLNode* n) { return n ? n->ToText() : nullptr; }
        tinyxml2::XMLComment* AsComment(tinyxml2::XMLNode* n) { return n ? n->ToComment() : nullptr; }
    }

    std::string XmlCharacterData::getDataProperty() const {
        if (auto* t = AsText(native_)) return t->Value() ? t->Value() : "";
        if (auto* c = AsComment(native_)) return c->Value() ? c->Value() : "";
        return {};
    }

    // The single choke point for every character-data mutation: AppendData, InsertData,
    // DeleteData, ReplaceData, setValueProperty and setInnerTextProperty all route here, so
    // one dispatch site covers all six public doors (#2079).
    void XmlCharacterData::setDataProperty(const std::string& data) {
        auto* t = AsText(native_);
        auto* c = t ? nullptr : AsComment(native_);
        if (!t && !c) return;
        XmlDocument* doc = GetDocument();
        const std::string oldValue = getDataProperty();
        detail::RaiseNodeChanging(doc, this, getParentNodeProperty(), getParentNodeProperty(),
                                  oldValue, data, XmlNodeChangedAction::Change);
        if (t) t->SetValue(data.c_str());
        else   c->SetValue(data.c_str());
        detail::RaiseNodeChanged(doc, this, getParentNodeProperty(), getParentNodeProperty(),
                                 oldValue, data, XmlNodeChangedAction::Change);
    }

    SharpRuntime::intcs XmlCharacterData::getLengthProperty() const {
        return static_cast<SharpRuntime::intcs>(getDataProperty().size());
    }

    std::string XmlCharacterData::Substring(SharpRuntime::intcs offset, SharpRuntime::intcs count) const {
        std::string data = getDataProperty();
        if (data.empty()) return "";
        auto len = static_cast<SharpRuntime::intcs>(data.size());
        if (len < offset + count) count = len - offset;
        if (offset < 0 || static_cast<size_t>(offset) > data.size())
            throw System::ArgumentOutOfRangeException("offset");
        if (count < 0) throw System::ArgumentOutOfRangeException("count");
        return data.substr(static_cast<size_t>(offset), static_cast<size_t>(count));
    }

    void XmlCharacterData::AppendData(const std::string& strData) {
        setDataProperty(getDataProperty() + strData);
    }

    void XmlCharacterData::InsertData(SharpRuntime::intcs offset, const std::string& strData) {
        std::string data = getDataProperty();
        if (offset < 0 || static_cast<size_t>(offset) > data.size())
            throw System::ArgumentOutOfRangeException("offset");
        data.insert(static_cast<size_t>(offset), strData);
        setDataProperty(data);
    }

    void XmlCharacterData::DeleteData(SharpRuntime::intcs offset, SharpRuntime::intcs count) {
        std::string data = getDataProperty();
        if (offset < 0 || static_cast<size_t>(offset) > data.size())
            throw System::ArgumentOutOfRangeException("offset");
        data.erase(static_cast<size_t>(offset), static_cast<size_t>(count));
        setDataProperty(data);
    }

    void XmlCharacterData::ReplaceData(SharpRuntime::intcs offset, SharpRuntime::intcs count, const std::string& strData) {
        std::string data = getDataProperty();
        if (offset < 0 || static_cast<size_t>(offset) > data.size())
            throw System::ArgumentOutOfRangeException("offset");
        data.replace(static_cast<size_t>(offset), static_cast<size_t>(count), strData);
        setDataProperty(data);
    }

    void XmlCharacterData::WriteTo(XmlWriter& writer) const {
        writer.WriteString(getDataProperty());
    }

} // namespace System::Xml
