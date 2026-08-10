// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #2198 (SR-AUD-336): the XObject Changed/Changing contract.
//
// THIS SUITE PINS AN INERT SURFACE, AND THAT NEEDS SAYING PLAINLY. `add_Changed`,
// `remove_Changed`, `add_Changing` and `remove_Changing` accept a handler and discard it; no
// mutation anywhere in the hierarchy raises anything. That is a confirmed defect, not a design
// this suite endorses. What the suite does is make the defect *measurable* at every door, so
// that a partial implementation cannot land silently and so that the eventual repair has a
// failing test to turn green.
//
// The distinction matters because the coverage this replaces did the opposite. The single
// pre-existing test asserted only that registration does not throw
// (`XObjectTests.ChangedChangingEventAccessors_DoNotThrow`, kept below as it was), which the
// audit correctly called out as *preserving* the inert behaviour rather than describing it: it
// would keep passing if half the doors started notifying and half did not.
//
// WHY THE REPAIR IS NOT HERE. Two blockers, both measured, both needing a decision this ticket
// is not allowed to take (docs/SystemXmlLinqNamespaceReviewPlan.md §12.3, ticket #2199):
//
//   1. There is nowhere to store a handler. .NET keeps these registrations in XObject's
//      annotation slot; annotations are documented out of scope in this port. A handler field
//      grows sizeof(XObject) 16 -> 24 -- there is no padding -- and every derived node type
//      with it. That exact growth was declined on 2026-07-31 for ticket #1896.
//   2. A registration cannot be named for removal. XObjectChangeEventHandler is a bare
//      std::function alias, and std::function is not equality-comparable, so
//      remove_Changed(handler) has no way to identify which registration to drop. This one is
//      not a cost question: it is not implementable as declared, at any layout cost.
//
// Blocker 2 is pinned by a static_assert below rather than by a runtime observation, because it
// is a compile-time property and fabricating a runtime measurement for it would be dishonest.

#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XCData.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XObject.hpp"
#include "System/Xml/Linq/XObjectChange.hpp"
#include "System/Xml/Linq/XObjectChangeEventArgs.hpp"
#include "System/Xml/Linq/XText.hpp"

using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XCData;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XObjectChange;
using System::Xml::Linq::XObjectChangeEventArgs;
using System::Xml::Linq::XObjectChangeEventHandler;
using System::Xml::Linq::XText;

namespace {

    /// Counts every notification a subject and its ancestor would receive, if any were raised.
    struct Recorder {
        int changed = 0;
        int changing = 0;
        std::vector<XObjectChange> kinds;

        [[nodiscard]] XObjectChangeEventHandler onChanged() {
            return [this](void*, const XObjectChangeEventArgs& e) {
                ++changed;
                kinds.push_back(e.getObjectChangeProperty());
            };
        }
        [[nodiscard]] XObjectChangeEventHandler onChanging() {
            return [this](void*, const XObjectChangeEventArgs&) { ++changing; };
        }
        [[nodiscard]] int total() const { return changed + changing; }
    };

    /// Registers on every object a notification could plausibly be delivered to.
    template <class T>
    void listen(Recorder& r, T& subject) {
        subject.add_Changed(r.onChanged());
        subject.add_Changing(r.onChanging());
    }

    template <class T, class = void>
    struct HasEquality : std::false_type {};
    template <class T>
    struct HasEquality<T, std::void_t<decltype(std::declval<const T&>() == std::declval<const T&>())>>
        : std::true_type {};

} // namespace

// --- Blocker 2, proved structurally ----------------------------------------------------------

static_assert(std::is_same_v<XObjectChangeEventHandler,
                             std::function<void(void*, const XObjectChangeEventArgs&)>>,
              "XObjectChangeEventHandler is no longer a bare std::function alias. SR-AUD-336's "
              "removal blocker was derived from that fact -- re-derive #2199 before proceeding.");

static_assert(!HasEquality<XObjectChangeEventHandler>::value,
              "XObjectChangeEventHandler became equality-comparable. remove_Changed could then "
              "identify a registration, so SR-AUD-336's second blocker no longer holds and "
              "#2199's approval XL-2 must be re-derived.");

TEST(XLinqChangeNotificationTests, HandlerTypeCannotIdentifyARegistration) {
    // The runtime face of the static_asserts above, so the reason is visible in test output and
    // not only in a compile error nobody reads.
    EXPECT_FALSE(HasEquality<XObjectChangeEventHandler>::value);
}

// --- Every mutation door on an element -------------------------------------------------------

TEST(XLinqChangeNotificationTests, SetValue_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    listen(r, *e);
    e->setValueProperty("v");
    EXPECT_EQ(e->getValueProperty(), "v");
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, SetName_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    listen(r, *e);
    e->setNameProperty(XName("renamed"));
    EXPECT_EQ(e->getNameProperty(), XName("renamed"));
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, AddNode_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    listen(r, *e);
    e->Add(std::make_shared<XElement>(XName("child")));
    EXPECT_EQ(e->Nodes().size(), 1u);
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, AddFirst_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XElement>(XName("a")));
    listen(r, *e);
    e->AddFirst(std::make_shared<XElement>(XName("b")));
    EXPECT_EQ(e->Nodes().size(), 2u);
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, AddText_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    listen(r, *e);
    e->Add(std::string("t"));
    EXPECT_EQ(e->getValueProperty(), "t");
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, AddAttribute_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    listen(r, *e);
    e->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    EXPECT_TRUE(e->getHasAttributesProperty());
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, RemoveNodes_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XElement>(XName("child")));
    listen(r, *e);
    e->RemoveNodes();
    EXPECT_TRUE(e->Nodes().empty());
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, RemoveAttributes_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    listen(r, *e);
    e->RemoveAttributes();
    EXPECT_FALSE(e->getHasAttributesProperty());
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, RemoveAll_RaisesNothing) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    e->Add(std::make_shared<XElement>(XName("child")));
    listen(r, *e);
    e->RemoveAll();
    EXPECT_TRUE(e->Nodes().empty());
    EXPECT_FALSE(e->getHasAttributesProperty());
    EXPECT_EQ(r.total(), 0);
}

// --- Doors on the node being mutated, not on its container ------------------------------------

TEST(XLinqChangeNotificationTests, NodeRemove_RaisesNothingOnTheNodeOrItsParent) {
    Recorder onNode;
    Recorder onParent;
    auto parent = std::make_shared<XElement>(XName("p"));
    auto child = std::make_shared<XElement>(XName("c"));
    parent->Add(child);
    listen(onNode, *child);
    listen(onParent, *parent);
    child->Remove();
    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(onNode.total(), 0);
    EXPECT_EQ(onParent.total(), 0);
}

TEST(XLinqChangeNotificationTests, NodeReplaceWith_RaisesNothingOnAnyParticipant) {
    Recorder onOld;
    Recorder onNew;
    Recorder onParent;
    auto parent = std::make_shared<XElement>(XName("p"));
    auto oldChild = std::make_shared<XElement>(XName("old"));
    auto newChild = std::make_shared<XElement>(XName("new"));
    parent->Add(oldChild);
    listen(onOld, *oldChild);
    listen(onNew, *newChild);
    listen(onParent, *parent);
    oldChild->ReplaceWith(newChild);
    EXPECT_EQ(parent->Nodes().size(), 1u);
    EXPECT_EQ(onOld.total() + onNew.total() + onParent.total(), 0);
}

TEST(XLinqChangeNotificationTests, AttributeSetValue_RaisesNothing) {
    Recorder onAttr;
    Recorder onOwner;
    auto e = std::make_shared<XElement>(XName("e"));
    auto a = std::make_shared<XAttribute>(XName("a"), "1");
    e->Add(a);
    listen(onAttr, *a);
    listen(onOwner, *e);
    a->setValueProperty("2");
    EXPECT_EQ(a->getValueProperty(), "2");
    EXPECT_EQ(onAttr.total() + onOwner.total(), 0);
}

TEST(XLinqChangeNotificationTests, AttributeRemove_RaisesNothing) {
    Recorder onAttr;
    Recorder onOwner;
    auto e = std::make_shared<XElement>(XName("e"));
    auto a = std::make_shared<XAttribute>(XName("a"), "1");
    e->Add(a);
    listen(onAttr, *a);
    listen(onOwner, *e);
    a->Remove();
    EXPECT_FALSE(e->getHasAttributesProperty());
    EXPECT_EQ(onAttr.total() + onOwner.total(), 0);
}

TEST(XLinqChangeNotificationTests, TextSetValue_RaisesNothing) {
    Recorder r;
    auto t = std::make_shared<XText>("a");
    listen(r, *t);
    t->setValueProperty("b");
    EXPECT_EQ(t->getValueProperty(), "b");
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, CommentSetValue_RaisesNothing) {
    Recorder r;
    auto c = std::make_shared<XComment>("a");
    listen(r, *c);
    c->setValueProperty("b");
    EXPECT_EQ(r.total(), 0);
}

// --- Bubbling: an ancestor sees nothing either -------------------------------------------------

TEST(XLinqChangeNotificationTests, AnAncestorObservesNoDescendantChange) {
    // In .NET a handler on an ancestor receives every change beneath it. Nothing here does.
    Recorder r;
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(mid);
    mid->Add(leaf);
    listen(r, *root);
    leaf->setValueProperty("x");
    leaf->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, ADocumentObservesNoChangeInItsTree) {
    Recorder r;
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("root"));
    doc->Add(root);
    listen(r, *doc);
    root->setValueProperty("x");
    EXPECT_EQ(r.total(), 0);
}

// --- Registration and removal are themselves inert but total ------------------------------------

TEST(XLinqChangeNotificationTests, ManyRegistrationsAreAllDiscarded) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    for (int i = 0; i < 8; ++i) listen(r, *e);
    e->setValueProperty("v");
    EXPECT_EQ(r.total(), 0);
}

TEST(XLinqChangeNotificationTests, RemovingAHandlerThatWasNeverRegistered_DoesNotThrow) {
    auto e = std::make_shared<XElement>(XName("e"));
    EXPECT_NO_THROW(e->remove_Changed([](void*, const XObjectChangeEventArgs&) {}));
    EXPECT_NO_THROW(e->remove_Changing([](void*, const XObjectChangeEventArgs&) {}));
}

TEST(XLinqChangeNotificationTests, AnEmptyHandlerIsAcceptedRatherThanRejected) {
    // A std::function that holds nothing would be an invocation hazard if anything invoked it.
    // Nothing does, and accepting it is the current contract; recorded so the repair has to
    // decide deliberately whether to keep it.
    auto e = std::make_shared<XElement>(XName("e"));
    EXPECT_NO_THROW(e->add_Changed(XObjectChangeEventHandler{}));
    EXPECT_NO_THROW(e->setValueProperty("v"));
}

// --- The data holders are correct in isolation, and stay that way --------------------------------

TEST(XLinqChangeNotificationTests, TheEventArgumentStaticsStillCarryTheirKinds) {
    // XObjectChange/XObjectChangeEventArgs are the half of SR-AUD-336 that is NOT broken -- the
    // audit says so explicitly. Pinned so the repair does not disturb them.
    EXPECT_EQ(XObjectChangeEventArgs::Add.getObjectChangeProperty(), XObjectChange::Add);
    EXPECT_EQ(XObjectChangeEventArgs::Remove.getObjectChangeProperty(), XObjectChange::Remove);
    EXPECT_EQ(XObjectChangeEventArgs::Name.getObjectChangeProperty(), XObjectChange::Name);
    EXPECT_EQ(XObjectChangeEventArgs::Value.getObjectChangeProperty(), XObjectChange::Value);
    EXPECT_EQ(XObjectChangeEventArgs(XObjectChange::Value).getObjectChangeProperty(),
              XObjectChange::Value);
}

// --- Blocker 1, measured -------------------------------------------------------------------------

TEST(XLinqChangeNotificationTests, ThereIsNoSpareStorageInXObjectForAHandler) {
    // sizeof(XObject) is exactly a vptr plus one pointer: no padding, nowhere to put a handler
    // without growing every node type. This is approval XL-1 on #2199, and it is the same
    // growth declined on 2026-07-31 for #1896. If any of these change, the approval sentence in
    // docs/SystemXmlLinqNamespaceReviewPlan.md §12.3 is stale and must be re-derived.
    EXPECT_EQ(sizeof(System::Xml::Linq::XObject), 2 * sizeof(void*));
    EXPECT_EQ(sizeof(XNode), 2 * sizeof(void*));
    EXPECT_EQ(alignof(System::Xml::Linq::XObject), alignof(void*));
}
