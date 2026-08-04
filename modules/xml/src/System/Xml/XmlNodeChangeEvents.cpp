// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "XmlNodeChangeEvents.hpp"

#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlNodeChangedEventArgs.hpp"

namespace System::Xml::detail {

    namespace {

        /// The six handlers are public data members on XmlDocument, so selecting one needs no
        /// accessor and adds no public surface — the corrected premise that makes #2079
        /// compatible (docs/SystemXmlNamespaceReviewPlan.md §6.6).
        const XmlNodeChangedEventHandler* Select(XmlDocument* doc, XmlNodeChangedAction action,
                                                 bool before) {
            switch (action) {
                case XmlNodeChangedAction::Insert:
                    return before ? &doc->NodeInserting : &doc->NodeInserted;
                case XmlNodeChangedAction::Remove:
                    return before ? &doc->NodeRemoving : &doc->NodeRemoved;
                case XmlNodeChangedAction::Change:
                    return before ? &doc->NodeChanging : &doc->NodeChanged;
            }
            return nullptr;
        }

        void Raise(XmlDocument* doc, XmlNode* node, XmlNode* oldParent, XmlNode* newParent,
                   const std::string& oldValue, const std::string& newValue,
                   XmlNodeChangedAction action, bool before) {
            if (!doc) return;
            const XmlNodeChangedEventHandler* handler = Select(doc, action, before);
            if (!handler || !*handler) return;
            XmlNodeChangedEventArgs args(node, oldParent, newParent, oldValue, newValue, action);
            // The handler is copied before invocation so a handler that reassigns its own
            // document field mid-dispatch cannot destroy the std::function it is running from.
            XmlNodeChangedEventHandler snapshot = *handler;
            snapshot(doc, args);
        }

    } // namespace

    void RaiseNodeChanging(XmlDocument* doc, XmlNode* node, XmlNode* oldParent, XmlNode* newParent,
                           const std::string& oldValue, const std::string& newValue,
                           XmlNodeChangedAction action) {
        Raise(doc, node, oldParent, newParent, oldValue, newValue, action, /*before=*/true);
    }

    void RaiseNodeChanged(XmlDocument* doc, XmlNode* node, XmlNode* oldParent, XmlNode* newParent,
                          const std::string& oldValue, const std::string& newValue,
                          XmlNodeChangedAction action) {
        Raise(doc, node, oldParent, newParent, oldValue, newValue, action, /*before=*/false);
    }

} // namespace System::Xml::detail
