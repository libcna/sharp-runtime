// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Xml::Linq {

    class XNamespace;

    class XName {
        std::string localName_;
        std::string namespaceName_;

    public:
        XName() = default;
        explicit XName(const std::string& localName) : localName_(localName) {}
        XName(const std::string& namespaceName, const std::string& localName)
            : localName_(localName), namespaceName_(namespaceName) {}

        [[nodiscard]] const std::string& getLocalNameProperty()     const { return localName_; }
        [[nodiscard]] const std::string& getNamespaceNameProperty() const { return namespaceName_; }

        [[nodiscard]] std::string ToString() const {
            if (namespaceName_.empty()) return localName_;
            return "{" + namespaceName_ + "}" + localName_;
        }

        bool operator==(const XName& o) const { return localName_ == o.localName_ && namespaceName_ == o.namespaceName_; }
        bool operator!=(const XName& o) const { return !(*this == o); }

        static XName Get(const std::string& expandedName) {
            if (!expandedName.empty() && expandedName[0] == '{') {
                size_t end = expandedName.find('}');
                if (end != std::string::npos)
                    return XName(expandedName.substr(1, end - 1), expandedName.substr(end + 1));
            }
            return XName(expandedName);
        }
    };

} // namespace System::Xml::Linq
