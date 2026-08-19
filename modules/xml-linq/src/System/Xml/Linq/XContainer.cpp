// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XContainer.hpp"
#include <algorithm>
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XElement.hpp"

namespace System::Xml::Linq {

    using System::Xml::XmlNodeType;

    namespace {

        /**
         * Worklist for the iterative container teardown (ticket #1895), the Xml.Linq twin of the
         * one in `JsonNode.cpp`.
         *
         * Releasing an element tree used to recurse: `~XContainer` released `children_`, which
         * dropped the last `shared_ptr` to each child node, which ran that child's destructor, one
         * call frame per level. A 20,000-deep nest therefore crashed with a stack overflow *after*
         * it had been built successfully (probe case X27c).
         *
         * The **outermost** container destructor publishes a worklist here and drains it in a
         * loop; every container destructor that runs while a worklist is published donates its own
         * children to that worklist and returns. Stack depth is constant; the bound is the heap.
         *
         * `thread_local` because the worklist belongs to one in-progress teardown on one thread.
         * It is never shared between threads, so it needs no synchronisation and introduces no
         * race.
         */
        thread_local std::vector<std::shared_ptr<XNode>>* pendingRelease = nullptr;

        /** Publishes `pending` for the duration of the outermost teardown, and unpublishes it. */
        class PendingReleaseScope {
        public:
            explicit PendingReleaseScope(std::vector<std::shared_ptr<XNode>>& pending) noexcept {
                pendingRelease = &pending;
            }
            ~PendingReleaseScope() { pendingRelease = nullptr; }
            PendingReleaseScope(const PendingReleaseScope&) = delete;
            PendingReleaseScope& operator=(const PendingReleaseScope&) = delete;
        };

        /**
         * Moves every non-null child onto the worklist, **last child first**, so that draining the
         * worklist from its back reproduces exactly the front-to-back document order the recursive
         * teardown produced.
         *
         * The only way this can fail is `std::bad_alloc` from the worklist's own growth, which is
         * caught rather than allowed to escape a destructor: whatever is still in `store` keeps
         * its (already detached) children and is released by `store`'s own destructor, i.e. the
         * recursive behaviour this repair replaces. Nothing is lost, leaked, double-detached or
         * released twice — the worklist and `store` never hold the same `shared_ptr`, because each
         * one is *moved* across.
         */
        void donateChildren(std::vector<std::shared_ptr<XNode>>& store,
                            std::vector<std::shared_ptr<XNode>>& worklist) noexcept {
            try {
                // Grow the worklist once for the whole donation instead of a child at a time, and
                // keep that growth geometric: reserving the exact requirement every time would
                // trade a stack-depth problem for a quadratic-copying one.
                const size_t needed = worklist.size() + store.size();
                if (needed > worklist.capacity())
                    worklist.reserve(needed > worklist.capacity() * 2 ? needed : worklist.capacity() * 2);
                for (auto it = store.rbegin(); it != store.rend(); ++it)
                    if (*it) worklist.push_back(std::move(*it));
            } catch (...) {
            }
        }

        /**
         * Drains the worklist. Each `shared_ptr` released here re-enters a container destructor,
         * which sees a published worklist and donates instead of recursing, so this loop is the
         * only stack frame the whole teardown needs.
         */
        void drainPendingRelease(std::vector<std::shared_ptr<XNode>>& worklist) noexcept {
            while (!worklist.empty()) {
                std::shared_ptr<XNode> node = std::move(worklist.back());
                worklist.pop_back();
                // `node` is released at the end of this iteration.
            }
        }

    } // namespace

    XContainer::~XContainer() {
        for (const auto& child : children_) {
            if (!child) continue;
            XObject& obj = *child;                 // XContainer is a friend of XObject
            if (obj.parent_ == this) obj.parent_ = nullptr;
        }

        if (children_.empty()) return;
        if (pendingRelease != nullptr) {
            donateChildren(children_, *pendingRelease);
            return;
        }
        std::vector<std::shared_ptr<XNode>> worklist;
        PendingReleaseScope scope(worklist);
        donateChildren(children_, worklist);
        drainPendingRelease(worklist);
    }

    void XContainer::RemoveNode(XNode* n) {
        auto it = std::find_if(children_.begin(), children_.end(),
                                [n](const std::shared_ptr<XNode>& c) { return c.get() == n; });
        if (it == children_.end()) return;
        // #2199: Changing before, Changed after, sender is the CHILD while the chain walked is
        // THIS container's -- .NET's own asymmetry (XContainer.cs:404,416). Nothing is raised for
        // a node that was not a child, because the early return above precedes this.
        const bool notify = NotifyChanging(n, XObjectChangeEventArgs::Remove);
        AdoptObject(**it, nullptr);
        children_.erase(it);
        if (notify) NotifyChanged(n, XObjectChangeEventArgs::Remove);
    }

    void XContainer::InsertNodeAt(size_t index, const std::shared_ptr<XNode>& n) {
        if (!n) return;
        ValidateNode(*n);
        // Verified against XContainer.cs's AddNode(): real .NET detects "n is this or an
        // ancestor of this" and clones n instead of inserting the original (Add() has
        // copy-on-attach semantics whenever the node being added is already attached
        // somewhere). This port instead reparents nodes in place (moves rather than clones) as
        // its established, simpler design for the common case, so replicating .NET's full
        // clone-based Add() would be a much larger behavioral change than this specific bug
        // warrants. This guards against the one genuinely broken outcome instead: inserting a
        // node into its own subtree, which previously created a permanent shared_ptr reference
        // cycle (the container ends up holding, transitively, a shared_ptr back to itself) and
        // a stack overflow in any recursive traversal (Nodes()/ToString()/etc.).
        // #1896: this walk used to run UNCONDITIONALLY, which is O(depth) per insert and therefore
        // O(depth^2) to build a deep tree top-down -- measured at 120.0s for 100,000 levels against
        // 4.7s for 20,000 (build-probe/1896_probe1_before.log).
        //
        // IT IS UNCHANGED IN WHAT IT REJECTS; IT IS ONLY REACHED WHEN IT CAN REJECT. The guard asks
        // "is `n` an ancestor-or-self of `this`?", and two facts answer that in O(1) whenever the
        // answer is no:
        //
        //   * a node that is not an XContainer has no children and cannot be anybody's ancestor;
        //   * a container with NO children cannot contain `this` either -- unless it IS `this`,
        //     which is checked directly.
        //
        // A CHILDLESS CONTAINER STILL NEEDS THE SELF-CHECK, and that is the trap: `n == this` is a
        // cycle even when `this` is empty, so the equality test must sit OUTSIDE the emptiness
        // guard. Dropping it is a mutation the tests catch.
        //
        // This is why the approved object-layout growth was NOT taken: no cached root, no cached
        // depth, no new member. Every sizeof is unchanged by this ticket.
        if (n.get() == this) {
            throw System::InvalidOperationException("Cannot add a node as a child of itself or of one of its own descendants.");
        }
        if (auto* candidate = dynamic_cast<XContainer*>(n.get()); candidate != nullptr && !candidate->children_.empty()) {
            for (XContainer* ancestorOrSelf = this; ancestorOrSelf; ancestorOrSelf = ancestorOrSelf->parent_) {
                if (ancestorOrSelf == n.get()) {
                    throw System::InvalidOperationException("Cannot add a node as a child of itself or of one of its own descendants.");
                }
            }
        }
        if (n->parent_ != nullptr) {
            n->parent_->RemoveNode(n.get());
        }
        index = std::min(index, children_.size());
        // #2199: Changing before the insertion, Changed after, guarded on `notify` -- .NET's
        // InsertNode does exactly this (XLinq.cs:156,177), with the added node as sender and this
        // container's chain walked.
        //
        // PLACEMENT IS DELIBERATE AND DIVERGES BY A HAIR. .NET raises Changing before its own
        // `n.parent != null` throw, so it can notify for a change that then fails. This port's
        // guards are different -- ValidateNode and the self-insertion check above, both of which
        // REFUSE the operation -- and notifying for a change that is then refused would be worse
        // than not notifying at all. So the pair sits after validation and around the mutation.
        // The detach of `n` from a previous parent above raises its own Remove pair, as it should.
        const bool notify = NotifyChanging(n.get(), XObjectChangeEventArgs::Add);
        // Adopt only once the insertion has actually happened. The reverse order left `n`
        // reporting this container as its parent while the container did not hold it, if the
        // vector growth threw -- a node claiming an owner that has never heard of it, whose
        // own Remove() would then silently do nothing (ticket #1891).
        children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), n);
        AdoptObject(*n, this);
        if (notify) NotifyChanged(n.get(), XObjectChangeEventArgs::Add);
    }

    void XContainer::Add(std::shared_ptr<XNode> node) {
        if (!node) return;
        InsertNodeAt(children_.size(), node);
    }

    void XContainer::Add(const std::vector<std::shared_ptr<XNode>>& nodes) {
        for (const auto& n : nodes) Add(n);
    }

    void XContainer::AddFirst(std::shared_ptr<XNode> node) {
        if (!node) return;
        InsertNodeAt(0, node);
    }

    void XContainer::AddFirst(const std::vector<std::shared_ptr<XNode>>& nodes) {
        size_t index = 0;
        for (const auto& n : nodes) {
            if (!n) continue;
            InsertNodeAt(index, n);
            ++index;
        }
    }

    std::vector<std::shared_ptr<XNode>> XContainer::DescendantNodes() const {
        std::vector<std::shared_ptr<XNode>> result;
        CollectDescendantNodes(result);
        return result;
    }

    void XContainer::CollectDescendantNodes(std::vector<std::shared_ptr<XNode>>& out) const {
        for (const auto& child : children_) {
            out.push_back(child);
            if (child->getNodeTypeProperty() == XmlNodeType::Element) {
                static_cast<XElement*>(child.get())->CollectDescendantNodes(out);
            }
        }
    }

    std::shared_ptr<XElement> XContainer::Element(const XName& name) const {
        for (const auto& child : children_) {
            if (child->getNodeTypeProperty() == XmlNodeType::Element) {
                auto e = std::static_pointer_cast<XElement>(child);
                if (e->getNameProperty() == name) return e;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<XElement>> XContainer::Elements() const {
        std::vector<std::shared_ptr<XElement>> result;
        for (const auto& child : children_) {
            if (child->getNodeTypeProperty() == XmlNodeType::Element) {
                result.push_back(std::static_pointer_cast<XElement>(child));
            }
        }
        return result;
    }

    std::vector<std::shared_ptr<XElement>> XContainer::Elements(const XName& name) const {
        std::vector<std::shared_ptr<XElement>> result;
        for (const auto& child : children_) {
            if (child->getNodeTypeProperty() == XmlNodeType::Element) {
                auto e = std::static_pointer_cast<XElement>(child);
                if (e->getNameProperty() == name) result.push_back(e);
            }
        }
        return result;
    }

    std::vector<std::shared_ptr<XElement>> XContainer::Descendants() const {
        std::vector<std::shared_ptr<XElement>> result;
        CollectDescendants(nullptr, result);
        return result;
    }

    std::vector<std::shared_ptr<XElement>> XContainer::Descendants(const XName& name) const {
        std::vector<std::shared_ptr<XElement>> result;
        CollectDescendants(&name, result);
        return result;
    }

    void XContainer::CollectDescendants(const XName* nameFilter, std::vector<std::shared_ptr<XElement>>& out) const {
        for (const auto& child : children_) {
            if (child->getNodeTypeProperty() == XmlNodeType::Element) {
                auto e = std::static_pointer_cast<XElement>(child);
                if (nameFilter == nullptr || e->getNameProperty() == *nameFilter) out.push_back(e);
                e->CollectDescendants(nameFilter, out);
            }
        }
    }

    void XContainer::RemoveNodes() {
        // #2199: one Remove pair PER CHILD, as .NET does -- its RemoveNodes loops and notifies for
        // each node individually (XContainer.cs:400-416), so a handler counting removals sees the
        // child count rather than one bulk event.
        //
        // The children are detached one at a time and each is detached BETWEEN its own Changing
        // and Changed, so a handler observing during the walk sees exactly the state .NET's would.
        while (!children_.empty()) {
            const std::shared_ptr<XNode> child = children_.front();
            const bool notify = NotifyChanging(child.get(), XObjectChangeEventArgs::Remove);
            AdoptObject(*child, nullptr);
            children_.erase(children_.begin());
            if (notify) NotifyChanged(child.get(), XObjectChangeEventArgs::Remove);
        }
    }

} // namespace System::Xml::Linq
