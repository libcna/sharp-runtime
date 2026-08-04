// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// Internal to the Xml component — deliberately NOT under modules/xml/include, so no consumer
// can reach it and no public surface is added. (Same placement as XPath/XPathAstInternal.hpp.)
//
// Ticket #2079 / SR-AUD-352, cause X-E: XmlDocument's six node-change handlers are public
// std::function DATA MEMBERS that nothing ever invoked. Wiring them therefore needs no type,
// signature or layout change — the dispatcher below simply reads those already-public members
// from the owning document.

#include <string>

#include "System/Xml/XmlNodeChangedAction.hpp"

namespace System::Xml {

    class XmlDocument;
    class XmlNode;

    namespace detail {

        /**
         * @brief Invokes the document's @c Node*ing handler for @p action, before the mutation.
         *
         * A no-op when @p doc is null or the matching handler is unset, which is what keeps a
         * document with no handlers installed exactly as fast and as silent as before. An
         * exception thrown by the handler propagates out of the mutator **before** anything is
         * mutated, so the tree is left untouched.
         */
        void RaiseNodeChanging(XmlDocument* doc, XmlNode* node, XmlNode* oldParent,
                               XmlNode* newParent, const std::string& oldValue,
                               const std::string& newValue, XmlNodeChangedAction action);

        /**
         * @brief Invokes the document's @c Node*ed handler for @p action, after the mutation.
         *
         * Same no-op rules. @p node must still be alive: this is never raised from a mutator
         * that destroys the node (see @c XmlNode::RemoveAllChildren), because handing caller
         * code a wrapper for a destroyed node would be exactly the borrowed-pointer defect
         * CCF-019 tracks.
         */
        void RaiseNodeChanged(XmlDocument* doc, XmlNode* node, XmlNode* oldParent,
                              XmlNode* newParent, const std::string& oldValue,
                              const std::string& newValue, XmlNodeChangedAction action);

    } // namespace detail

} // namespace System::Xml
