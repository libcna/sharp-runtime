// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Xml::Linq {

    class XNamespace;

    /** Represents a qualified XML name (local name + optional namespace URI). */
    class XName {
        std::string localName_;
        std::string namespaceName_;

    public:
        /** Default constructor — produces an empty name. */
        XName() = default;

        /** Constructs an unqualified name from @p localName. */
        explicit XName(const std::string& localName) : localName_(localName) {}

        /** Constructs a qualified name from @p namespaceName and @p localName. */
        XName(const std::string& namespaceName, const std::string& localName)
            : localName_(localName), namespaceName_(namespaceName) {}

        /** @return The local (unqualified) part of the name. */
        [[nodiscard]] const std::string& getLocalNameProperty()     const { return localName_; }

        /** @return The namespace URI, or empty string if unqualified. */
        [[nodiscard]] const std::string& getNamespaceNameProperty() const { return namespaceName_; }

        /** @return The expanded form: "{namespace}localName", or just localName when unqualified. */
        [[nodiscard]] std::string ToString() const {
            if (namespaceName_.empty()) return localName_;
            return "{" + namespaceName_ + "}" + localName_;
        }

        /** Equality — both local name and namespace URI must match. */
        bool operator==(const XName& o) const { return localName_ == o.localName_ && namespaceName_ == o.namespaceName_; }
        /** Inequality operator. */
        bool operator!=(const XName& o) const { return !(*this == o); }

        /**
         * Parses an expanded name of the form "{namespace}localName" or plain "localName".
         * @param expandedName The expanded XML name string.
         * @return Parsed XName.
         */
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
