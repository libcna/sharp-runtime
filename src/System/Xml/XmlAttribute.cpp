// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlAttribute.hpp"

#include <tinyxml2/tinyxml2.h>

namespace System::Xml {

    std::string XmlAttribute::getValueProperty() const {
        if (ownerElementNative_) {
            const char* v = ownerElementNative_->Attribute(name_.c_str());
            return v ? v : "";
        }
        return localValue_;
    }

    void XmlAttribute::setValueProperty(const std::string& value) {
        if (ownerElementNative_) {
            ownerElementNative_->SetAttribute(name_.c_str(), value.c_str());
        } else {
            localValue_ = value;
        }
    }

    void XmlAttribute::AttachTo(XmlElement* elementWrapper, tinyxml2::XMLElement* elementNative) {
        ownerElementWrapper_ = elementWrapper;
        ownerElementNative_ = elementNative;
        elementNative->SetAttribute(name_.c_str(), localValue_.c_str());
    }

    void XmlAttribute::BindExisting(XmlElement* elementWrapper, tinyxml2::XMLElement* elementNative) {
        ownerElementWrapper_ = elementWrapper;
        ownerElementNative_ = elementNative;
    }

    void XmlAttribute::Detach() {
        if (ownerElementNative_) {
            const char* v = ownerElementNative_->Attribute(name_.c_str());
            localValue_ = v ? v : "";
        }
        ownerElementWrapper_ = nullptr;
        ownerElementNative_ = nullptr;
    }

} // namespace System::Xml
