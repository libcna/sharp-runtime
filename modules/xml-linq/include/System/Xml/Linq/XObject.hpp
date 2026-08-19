// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
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
     * @brief An opaque handle to one `Changed`/`Changing` registration.
     *
     * **This type has no .NET counterpart and its existence is the one deliberate divergence in
     * ticket #2199.** .NET removes a registration by passing the *delegate* back
     * (`obj.Changed -= handler`), which works because a C# delegate is equality-comparable.
     * `XObjectChangeEventHandler` is a `std::function`, and `std::function` has **no `operator==`
     * against another `std::function`** — proved at compile time, not assumed — so a
     * handler-taking `remove_*` cannot identify which registration a caller means.
     *
     * Two alternatives were offered and declined on 2026-08-19
     * (`docs/StandingApprovals.md` SA-13): keeping .NET's signature and removing **all**
     * registrations, which silently drops a third party's subscription; and keeping it and
     * **throwing** from `remove_*`, which leaves a subscriber unable ever to unsubscribe. The
     * token was chosen so the divergence is visible **in the type** rather than hidden in the
     * behaviour.
     *
     * Ids are drawn from a process-wide counter, so a token issued by one `XObject` can never
     * accidentally match a registration on another; passing a foreign or default-constructed token
     * to `remove_*` simply removes nothing and returns `false`.
     */
    class XObjectChangeRegistration {
        friend class XObject;
        std::uint64_t id_ = 0;
        explicit XObjectChangeRegistration(std::uint64_t id) noexcept : id_(id) {}

    public:
        /** @brief Constructs a token that names no registration. */
        XObjectChangeRegistration() = default;

        /** @return true if this token names no registration. */
        [[nodiscard]] bool IsEmpty() const noexcept { return id_ == 0; }

        [[nodiscard]] bool operator==(const XObjectChangeRegistration&) const noexcept = default;
    };

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
     *
     * @note **Changed/Changing events ARE implemented** as of ticket **#2199** (SR-AUD-336,
     * 2026-08-19). They were inert -- the four accessors took a handler and discarded it, and no
     * mutation raised anything. Both of #2199's gates were opened on the same day: the layout
     * growth `sizeof(XObject)` 16 -> 24 (and every derived node type with it), and the removal
     * design, which is a **registration token** rather than .NET's handler-taking `remove_*`. See
     * `XObjectChangeRegistration` and `docs/Migration-XObjectChangeNotification.md`.
     */
    class XObject {
        friend class XContainer;

        /**
         * Per-object `Changed`/`Changing` registrations. Held behind a `unique_ptr` and allocated
         * only on first registration, so an unobserved tree pays one null pointer and no
         * allocation -- the closest analogue of .NET's annotation slot, which is likewise absent
         * until something is annotated.
         */
        struct ChangeRegistrations {
            std::vector<std::pair<std::uint64_t, XObjectChangeEventHandler>> changed;
            std::vector<std::pair<std::uint64_t, XObjectChangeEventHandler>> changing;

            [[nodiscard]] bool empty() const noexcept {
                return changed.empty() && changing.empty();
            }

            /** Keeps the process-wide live count honest when an observed object is destroyed. */
            ~ChangeRegistrations();
        };
        std::unique_ptr<ChangeRegistrations> changeRegistrations_;

        /** Process-wide, so a token from one object can never match a registration on another. */
        static std::uint64_t nextRegistrationId() noexcept;

        /**
         * @return true if ANY `XObject` in this process currently carries a registration.
         *
         * #1896's second half. The ancestor walk is O(depth) per mutation, so building a deep tree
         * is O(depth^2) even when NOTHING is subscribed -- and .NET has exactly the same shape
         * (`XObject.cs:424-427` skips annotation-less ancestors cheaply but still visits every
         * one). Measured here: 100,000 levels cost 89.3s with the walk and 0.03s without.
         *
         * A single process-wide count makes the unobserved case O(1) and is EXACT rather than
         * approximate: if no registration exists anywhere, no walk can find one, so `notify` is
         * false and no handler is skipped. When registrations do exist the walk runs in full, so
         * behaviour is identical to .NET's; only the nobody-is-listening case is faster.
         */
        static bool anyRegistrationsExist() noexcept;

        static void noteRegistrationAdded() noexcept;
        static void noteRegistrationsRemoved(std::size_t count) noexcept;

        static bool eraseRegistration(
            std::vector<std::pair<std::uint64_t, XObjectChangeEventHandler>>& list,
            const XObjectChangeRegistration& registration) noexcept;

        /** Shared body of NotifyChanging/NotifyChanged -- .NET's two are identical but for the
         *  delegate they invoke (XObject.cs:418-460). */
        bool walkAndNotify(void* sender, const XObjectChangeEventArgs& e, bool changingHalf);

    protected:
        /** Nearest containing XContainer (XElement or XDocument), or nullptr if this object is detached/root. Non-owning. */
        XContainer* parent_ = nullptr;

        /**
         * @brief Raises `Changing` on this object and every ancestor, innermost first.
         *
         * C++ counterpart of .NET `XObject.NotifyChanging` (`XObject.cs:440-460`). @p sender is
         * the object being changed, which is not necessarily `this` -- for `Add` and `Remove`
         * .NET raises on the *parent* with the *child* as sender.
         *
         * @return true if **any** object on the chain carries registrations at all. This is .NET's
         * own return value and it is deliberately *not* "a changing handler ran": the reference
         * tests `Annotation<XObjectChangeAnnotation>() != null` and only then invokes
         * `a.changing?.Invoke(...)`, so a caller who subscribed to `Changed` **alone** still makes
         * this return true -- and therefore still receives `Changed`. Getting that wrong silently
         * disables `Changed`-only subscriptions.
         */
        bool NotifyChanging(void* sender, const XObjectChangeEventArgs& e);

        /**
         * @brief Raises `Changed` on this object and every ancestor, innermost first.
         *
         * C++ counterpart of .NET `XObject.NotifyChanged` (`XObject.cs:418-438`). Every .NET call
         * site guards this on the value `NotifyChanging` returned -- `if (notify) NotifyChanged(...)`
         * -- and this port does the same.
         */
        bool NotifyChanged(void* sender, const XObjectChangeEventArgs& e);

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
         * @brief Registers @p handler to be called **after** a change to this object or any
         * descendant.
         *
         * C++ counterpart of .NET `XObject.Changed`'s add accessor. Implemented by ticket **#2199**
         * (SR-AUD-336), landed 2026-08-19; before it, this member accepted a handler and
         * **discarded** it and no mutation anywhere in the hierarchy raised anything.
         *
         * @return A token naming this registration. **This return value is the divergence from
         * .NET** — see @c XObjectChangeRegistration for why a handler cannot name itself in C++.
         *
         * @note **Notifications bubble.** A handler registered on an ancestor sees changes to every
         * descendant, innermost object first, matching .NET's walk up the parent chain
         * (`XObject.cs:418-438`). The `sender` a handler receives is the object being **changed**,
         * which for `Add` and `Remove` is the *child*, while the object whose chain is walked is
         * the *parent* — .NET's own asymmetry (`XLinq.cs:156,177`), transcribed.
         *
         * @note An **empty** `std::function` may be registered and is simply never invoked. It
         * still counts as a registration, so it still makes this object a notification point for
         * its descendants — which is .NET's behaviour, where the annotation exists whether or not
         * either delegate is null.
         *
         * @note Handlers are invoked from a **snapshot**, so a handler may register or unregister
         * during a notification without invalidating the walk; the change takes effect on the next
         * notification.
         */
        [[nodiscard]] XObjectChangeRegistration add_Changed(const XObjectChangeEventHandler& handler);

        /**
         * @brief Removes the `Changed` registration named by @p registration.
         *
         * @return true if a registration was removed; false if the token names none — including a
         * default-constructed token and one issued by a **different** `XObject`, since ids are
         * process-wide.
         *
         * @note **This signature diverges from .NET deliberately**, which removes by passing the
         * delegate back. `XObjectChangeEventHandler` is a `std::function` and has no `operator==`,
         * so no handler-taking overload can identify a registration. The two alternatives —
         * removing *all* registrations, or throwing — were offered and declined on 2026-08-19.
         */
        bool remove_Changed(const XObjectChangeRegistration& registration) noexcept;

        /**
         * @brief Registers @p handler to be called **before** a change to this object or any
         * descendant. See @c add_Changed for the bubbling, sender and snapshot rules.
         * @return A token naming this registration.
         */
        [[nodiscard]] XObjectChangeRegistration add_Changing(const XObjectChangeEventHandler& handler);

        /**
         * @brief Removes the `Changing` registration named by @p registration.
         * @return true if a registration was removed. See @c remove_Changed.
         */
        bool remove_Changing(const XObjectChangeRegistration& registration) noexcept;
    };

} // namespace System::Xml::Linq
