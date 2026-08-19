// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <algorithm>
#include <atomic>

#include "System/Xml/Linq/XObject.hpp"
#include "System/Xml/Linq/XContainer.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XDocument.hpp"

namespace System::Xml::Linq {

    XElement* XObject::getParentProperty() const {
        if (parent_ != nullptr && parent_->getNodeTypeProperty() == System::Xml::XmlNodeType::Element) {
            return static_cast<XElement*>(parent_);
        }
        return nullptr;
    }

    XDocument* XObject::getDocumentProperty() const {
        const XObject* o = this;
        while (o->parent_ != nullptr) {
            o = o->parent_;
        }
        if (o->getNodeTypeProperty() == System::Xml::XmlNodeType::Document) {
            return const_cast<XDocument*>(static_cast<const XDocument*>(o));
        }
        return nullptr;
    }


    // -----------------------------------------------------------------------------------------
    // Change notification (#2199, SR-AUD-336)
    // -----------------------------------------------------------------------------------------

    namespace {
        /// Process-wide count of live Changed/Changing registrations. See anyRegistrationsExist.
        std::atomic<std::size_t>& liveRegistrationCount() noexcept {
            static std::atomic<std::size_t> count{0};
            return count;
        }
    } // namespace

    XObject::ChangeRegistrations::~ChangeRegistrations() {
        XObject::noteRegistrationsRemoved(changed.size() + changing.size());
    }

    bool XObject::anyRegistrationsExist() noexcept {
        return liveRegistrationCount().load(std::memory_order_relaxed) != 0;
    }

    void XObject::noteRegistrationAdded() noexcept {
        liveRegistrationCount().fetch_add(1, std::memory_order_relaxed);
    }

    void XObject::noteRegistrationsRemoved(std::size_t count) noexcept {
        if (count != 0) {
            liveRegistrationCount().fetch_sub(count, std::memory_order_relaxed);
        }
    }

    std::uint64_t XObject::nextRegistrationId() noexcept {
        // Starts at 1 so that 0 can mean "names no registration" -- which is what a
        // default-constructed XObjectChangeRegistration holds.
        static std::atomic<std::uint64_t> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    bool XObject::eraseRegistration(
        std::vector<std::pair<std::uint64_t, XObjectChangeEventHandler>>& list,
        const XObjectChangeRegistration& registration) noexcept {
        if (registration.IsEmpty()) {
            return false;
        }
        const auto it = std::find_if(list.begin(), list.end(),
                                     [&](const auto& entry) { return entry.first == registration.id_; });
        if (it == list.end()) {
            return false;
        }
        list.erase(it);
        return true;
    }

    XObjectChangeRegistration XObject::add_Changed(const XObjectChangeEventHandler& handler) {
        if (changeRegistrations_ == nullptr) {
            changeRegistrations_ = std::make_unique<ChangeRegistrations>();
        }
        const std::uint64_t id = nextRegistrationId();
        changeRegistrations_->changed.emplace_back(id, handler);
        noteRegistrationAdded();
        return XObjectChangeRegistration(id);
    }

    XObjectChangeRegistration XObject::add_Changing(const XObjectChangeEventHandler& handler) {
        if (changeRegistrations_ == nullptr) {
            changeRegistrations_ = std::make_unique<ChangeRegistrations>();
        }
        const std::uint64_t id = nextRegistrationId();
        changeRegistrations_->changing.emplace_back(id, handler);
        noteRegistrationAdded();
        return XObjectChangeRegistration(id);
    }

    bool XObject::remove_Changed(const XObjectChangeRegistration& registration) noexcept {
        if (changeRegistrations_ == nullptr) {
            return false;
        }
        const bool erased = eraseRegistration(changeRegistrations_->changed, registration);
        if (erased) noteRegistrationsRemoved(1);
        return erased;
    }

    bool XObject::remove_Changing(const XObjectChangeRegistration& registration) noexcept {
        if (changeRegistrations_ == nullptr) {
            return false;
        }
        const bool erased = eraseRegistration(changeRegistrations_->changing, registration);
        if (erased) noteRegistrationsRemoved(1);
        return erased;
    }

    bool XObject::NotifyChanging(void* sender, const XObjectChangeEventArgs& e) {
        return walkAndNotify(sender, e, /*changingHalf=*/true);
    }

    bool XObject::NotifyChanged(void* sender, const XObjectChangeEventArgs& e) {
        return walkAndNotify(sender, e, /*changingHalf=*/false);
    }

    /**
     * Walks this object and every ancestor, innermost first, invoking the selected half of each
     * object's registrations. C++ counterpart of .NET XObject.NotifyChanging/NotifyChanged
     * (XObject.cs:418-460), whose two bodies are identical but for the delegate they invoke.
     *
     * The return value is TRUE IF ANY OBJECT ON THE CHAIN CARRIES REGISTRATIONS AT ALL -- not "a
     * handler ran". That is .NET's own semantic: it tests `Annotation<XObjectChangeAnnotation>()
     * != null` and only then invokes the possibly-null delegate. Every .NET call site guards
     * NotifyChanged on the value NotifyChanging returned, so reading this as "a changing handler
     * ran" would silently disable every Changed-ONLY subscription.
     */
    bool XObject::walkAndNotify(void* sender, const XObjectChangeEventArgs& e, bool changingHalf) {
        // #1896: the ancestor walk is O(depth), so a deep tree built node by node costs
        // O(depth^2) even with NOTHING subscribed. Exact short-circuit -- if no registration
        // exists anywhere in the process, no walk can find one.
        if (!anyRegistrationsExist()) {
            return false;
        }
        bool notify = false;
        for (XObject* o = this; o != nullptr; o = o->parent_) {
            if (o->changeRegistrations_ == nullptr || o->changeRegistrations_->empty()) {
                continue;
            }
            notify = true;
            // A SNAPSHOT, so a handler may register or unregister during the notification without
            // invalidating this walk; the change takes effect on the next notification. This is
            // the convention MulticastAction already uses in this repository.
            const auto handlers = changingHalf ? o->changeRegistrations_->changing
                                               : o->changeRegistrations_->changed;
            for (const auto& entry : handlers) {
                if (entry.second) {
                    entry.second(sender, e);
                }
            }
        }
        return notify;
    }

} // namespace System::Xml::Linq
