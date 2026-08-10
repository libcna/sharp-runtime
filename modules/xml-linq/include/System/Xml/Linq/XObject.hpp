// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/Xml/Linq/XObjectChangeEventArgs.hpp"
#include "System/Xml/XmlNodeType.hpp"

namespace System::Xml::Linq {

    class XContainer;
    class XElement;
    class XDocument;

    /**
     * @brief Represents the method that handles the XObject::Changed/Changing events.
     *
     * C++ counterpart of .NET's `EventHandler<XObjectChangeEventArgs>` as used by XObject.
     */
    using XObjectChangeEventHandler = std::function<void(void* sender, const XObjectChangeEventArgs& e)>;

    /**
     * @brief Represents a node or an attribute in an XML tree. Abstract base of the whole
     * System.Xml.Linq node hierarchy (XNode, XContainer, XElement, XDocument, ...) and of XAttribute.
     *
     * C++ counterpart of .NET System.Xml.Linq.XObject.
     *
     * @note Deliberately out of scope for this port (documented, not silent):
     * - Annotations (AddAnnotation/Annotation/Annotations/RemoveAnnotations) — .NET's generic
     *   per-object `object?` annotation bag has no clean C++ equivalent without reflection/RTTI
     *   plumbing this runtime otherwise avoids (see CLAUDE.md's reflection policy); ported game
     *   code realistically never depends on it.
     * - BaseUri / IXmlLineInfo (HasLineInfo/LineNumber/LinePosition) — depend on the annotation
     *   system above; LoadOptions::SetBaseUri/SetLineInfo already document that they're no-ops.
     * - Changed/Changing events — the four accessors below take a handler and **discard** it,
     *   and no mutation anywhere in the hierarchy raises anything. See their doc-comments for
     *   the two measured blockers that keep it that way; the contract is pinned by
     *   `XLinqChangeNotificationTests.cpp` so a half-implementation cannot land silently
     *   (ticket #2198, SR-AUD-336). Implementation is ticket **#2199**, blocked on two
     *   approvals recorded in `docs/SystemXmlLinqNamespaceReviewPlan.md` §12.3.
     */
    class XObject {
        friend class XContainer;

    protected:
        /** Nearest containing XContainer (XElement or XDocument), or nullptr if this object is detached/root. Non-owning. */
        XContainer* parent_ = nullptr;

    public:
        XObject() = default;
        virtual ~XObject() = default;

        // Nodes/containers are always used via shared_ptr and track a raw parent_ back-pointer;
        // value-copying an XObject would silently duplicate that back-pointer without updating
        // the (former) parent's child list, so copy/move are disabled hierarchy-wide. XAttribute
        // provides its own explicit copy constructor (which starts the copy detached, not a
        // base-class copy) for the one case that needs it (see XAttribute's doc comment).
        XObject(const XObject&) = delete;
        XObject& operator=(const XObject&) = delete;
        XObject(XObject&&) = delete;
        XObject& operator=(XObject&&) = delete;

        /** @return The node type of this object. */
        [[nodiscard]] virtual System::Xml::XmlNodeType getNodeTypeProperty() const = 0;

        /**
         * @return The parent XElement of this object, or nullptr if this object has no parent
         * or its parent is an XDocument (matches .NET: `parent as XElement`).
         */
        [[nodiscard]] XElement* getParentProperty() const;

        /** @return The XDocument that owns this object (walking up to the root), or nullptr if the root is not an XDocument. */
        [[nodiscard]] XDocument* getDocumentProperty() const;

        /**
         * @brief **Inert.** Accepts @p handler and discards it; no mutation ever invokes it.
         *
         * C++ counterpart of .NET XObject.Changed's add accessor, provided so ported code
         * compiles. Measured across nine mutation doors — `setValueProperty`,
         * `setNameProperty`, `Add(node)`, `Add(attribute)`, `RemoveAll`, and a handler on an
         * ancestor observing a descendant — every one delivers **zero** notifications
         * (`build-probe/2195_probe1_surface.log`, block `E*`).
         *
         * @note **Two measured blockers keep it inert, and both need a decision this port
         * cannot take on its own** (ticket #2199; `docs/SystemXmlLinqNamespaceReviewPlan.md`
         * §12.3):
         * 1. **There is nowhere to put a handler.** .NET stores these registrations in
         *    `XObject`'s annotation slot, and annotations are documented out of scope above.
         *    Adding a handler field grows `sizeof(XObject)` from 16 to 24 — measured; there is
         *    no padding — and every derived node type with it. That exact growth was declined
         *    for a different purpose on 2026-07-31 (ticket #1896), so it has to be asked again.
         * 2. **A registration cannot be named for removal.** See @c remove_Changed.
         *
         * The inert contract is pinned by `XLinqChangeNotificationTests.cpp`, which fails the
         * moment notification is partially implemented — the previous coverage asserted only
         * that registration does not throw, which *preserved* the behaviour instead of
         * describing it.
         */
        void add_Changed(const XObjectChangeEventHandler& /*handler*/) {}
        /**
         * @brief **Inert.** Accepts @p handler and discards it.
         *
         * @note This member is the harder half of SR-AUD-336, and it is not implementable at
         * *any* layout cost as declared. `XObjectChangeEventHandler` is a bare `std::function`
         * alias, and `std::function` has no `operator==` against another `std::function`, so
         * there is no way to identify which registration a caller means. Proved at compile
         * time in `build-probe/2195_probe3_events.cpp` rather than asserted. Resolving it needs
         * either a public shape change (registration returns a token that removal consumes) or
         * a documented deviation from .NET's delegate semantics — approval XL-2 on #2199.
         */
        void remove_Changed(const XObjectChangeEventHandler& /*handler*/) {}
        /** @brief **Inert.** Accepts a handler and discards it — see @c add_Changed. */
        void add_Changing(const XObjectChangeEventHandler& /*handler*/) {}
        /** @brief **Inert.** Accepts a handler and discards it — see @c remove_Changed. */
        void remove_Changing(const XObjectChangeEventHandler& /*handler*/) {}
    };

} // namespace System::Xml::Linq
