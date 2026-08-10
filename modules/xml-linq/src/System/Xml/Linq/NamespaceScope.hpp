// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// Internal to the Xml.Linq component — deliberately NOT under modules/xml-linq/include, so no
// consumer can reach it and no public surface is added. (Same placement, and the same reason, as
// modules/xml/src/System/Xml/XmlNodeChangeEvents.hpp.)
//
// Ticket #2197 / SR-AUD-334. The prefix↔URI bindings that are in scope at a point in an XML
// tree, and the prefix allocation a serializer needs when a namespace is used but not declared.
//
// Nothing here is stored in any node: the scope is rebuilt from the tree on each serialization
// entry point. That is deliberate and is what keeps the repair object-layout compatible —
// caching it would grow every XObject, which is exactly the approval #1896 was refused
// (docs/SystemXmlLinqNamespaceReviewPlan.md §12.2).

#include <string>
#include <utility>
#include <vector>

#include "System/Xml/Linq/XNamespace.hpp"

namespace System::Xml::Linq::detail {

    /** The URI the reserved `xml` prefix is permanently bound to; never declared explicitly. */
    inline const char* const kXmlNamespaceUri = "http://www.w3.org/XML/1998/namespace";
    /** The URI namespace-declaration attributes live in; `xmlns:p` is `{this}p`. */
    inline const char* const kXmlnsNamespaceUri = "http://www.w3.org/2000/xmlns/";

    /**
     * @brief The prefix→URI bindings in scope at one point in a tree, innermost last.
     *
     * A prefix of `""` is the default namespace (which applies to element names only, never to
     * attribute names — the XML Namespaces rule that is easiest to get wrong, and the one
     * `System::Xml::XmlAttribute::getNamespaceURIProperty` already implements on the parse side).
     */
    class NamespaceScope {
        std::vector<std::pair<std::string, std::string>> bindings_;
        int generated_ = 0;

    public:
        /** @brief Binds @p prefix to @p uri, shadowing any outer binding of the same prefix. */
        void Declare(const std::string& prefix, const std::string& uri) {
            bindings_.emplace_back(prefix, uri);
        }

        /** @return The URI @p prefix is currently bound to, or std::nullopt-as-empty-with-false. */
        [[nodiscard]] bool TryLookupPrefix(const std::string& prefix, std::string& uri) const {
            if (prefix == "xml") {
                uri = kXmlNamespaceUri;
                return true;
            }
            for (auto it = bindings_.rbegin(); it != bindings_.rend(); ++it) {
                if (it->first == prefix) {
                    uri = it->second;
                    return true;
                }
            }
            return false;
        }

        /**
         * @return The innermost prefix currently bound to @p uri and **not shadowed** by a
         * later binding of the same prefix, or `false` if none is.
         *
         * @param allowDefault When false, the default (empty) prefix is skipped — an attribute
         * name can never take its namespace from the default declaration.
         */
        [[nodiscard]] bool TryLookupUri(const std::string& uri, bool allowDefault,
                                        std::string& prefix) const {
            if (uri == kXmlNamespaceUri) {
                prefix = "xml";
                return true;
            }
            for (auto it = bindings_.rbegin(); it != bindings_.rend(); ++it) {
                if (!allowDefault && it->first.empty()) continue;
                if (it->second != uri) continue;
                // Only the innermost binding of a prefix is live; an outer one that happens to
                // name this URI has been shadowed and using it would emit a wrong name.
                std::string live;
                if (TryLookupPrefix(it->first, live) && live == uri) {
                    prefix = it->first;
                    return true;
                }
            }
            return false;
        }

        /** @return The URI the default namespace is bound to, or "" if none is. */
        [[nodiscard]] std::string DefaultUri() const {
            std::string uri;
            return TryLookupPrefix(std::string(), uri) ? uri : std::string();
        }

        /**
         * @brief Allocates a prefix that is not currently bound, as `p1`, `p2`, … .
         *
         * Matches the shape real .NET's writer produces for a namespace with no declared
         * prefix. The counter is per-scope, and collisions with a declared prefix are skipped
         * rather than assumed away.
         */
        [[nodiscard]] std::string AllocatePrefix() {
            std::string candidate;
            std::string unused;
            do {
                candidate = "p" + std::to_string(++generated_);
            } while (TryLookupPrefix(candidate, unused));
            return candidate;
        }
    };

    /** @return true if @p name is a namespace declaration: `xmlns` or `{xmlns-uri}prefix`. */
    [[nodiscard]] inline bool IsNamespaceDeclarationName(const std::string& namespaceUri,
                                                         const std::string& localName) {
        if (namespaceUri.empty()) return localName == "xmlns";
        return namespaceUri == kXmlnsNamespaceUri;
    }

    /** @return The prefix a declaration attribute declares: `""` for `xmlns`, else its local name. */
    [[nodiscard]] inline std::string DeclaredPrefix(const std::string& namespaceUri,
                                                    const std::string& localName) {
        return namespaceUri.empty() ? std::string() : localName;
    }

    /** @return The attribute name a declaration of @p prefix is written as. */
    [[nodiscard]] inline std::string DeclarationAttributeName(const std::string& prefix) {
        return prefix.empty() ? std::string("xmlns") : ("xmlns:" + prefix);
    }

    /** @return `prefix:local`, or just `local` when @p prefix is empty. */
    [[nodiscard]] inline std::string QualifiedName(const std::string& prefix,
                                                   const std::string& localName) {
        return prefix.empty() ? localName : (prefix + ":" + localName);
    }

} // namespace System::Xml::Linq::detail
