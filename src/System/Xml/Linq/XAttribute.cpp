// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XElement.hpp"

namespace System::Xml::Linq {

    std::string XAttribute::EscapeValue(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c; break;
            }
        }
        return out;
    }

    XAttribute* XAttribute::getPreviousAttributeProperty() const {
        XElement* owner = getParentProperty();
        if (owner == nullptr) return nullptr;
        XAttribute* prev = nullptr;
        for (const auto& a : owner->getAttributesProperty()) {
            if (a.get() == this) return prev;
            prev = a.get();
        }
        return nullptr;
    }

    void XAttribute::Remove() {
        XElement* owner = getParentProperty();
        if (owner == nullptr) throw System::InvalidOperationException("The parent is missing.");
        owner->RemoveAttribute(this);
    }

} // namespace System::Xml::Linq
