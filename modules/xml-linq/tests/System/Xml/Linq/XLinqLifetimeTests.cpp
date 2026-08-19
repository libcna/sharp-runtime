// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1890 (SR-AUD-333, CCF-019): XContainer clears the parent
// link of every child node it still owns in its own destructor, and XElement additionally
// clears every owned attribute's parent link AND its intrusive next_ sibling link. A node or
// attribute a caller retained past its owner's destruction therefore reports no parent instead
// of dispatching a virtual call through freed storage.
//
// The contract these tests pin is documented in docs/OwnedTreeLifetimeContractPlan.md sections
// 14 and 31 item 1: after the owning container is destroyed, a retained object is left in
// exactly the state RemoveNodes()/RemoveAttributes()/Remove() already produce - no parent, no
// document, no siblings, Remove() throws "The parent is missing.", and it can be re-added.
// Every case below is one of the shapes probe build-probe/1885_ccf019_lifetime_probe.cpp
// measured against the shipped bodies; the probe's case identifiers (X01, X13, X22, ...) are
// quoted so the two records stay linked.
//
// What these tests do NOT cover, deliberately: X15 (Extensions::Ancestors' raw XElement*) and
// X17 (getAttributesProperty()'s reference), which outlive their owner for a reason the
// parent-link repair cannot reach and which belong to ticket #1892; and X20 (ReplaceWith's
// exception path losing the original node), which belongs to ticket #1891.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XContainer.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XObject.hpp"
#include "System/Xml/Linq/XText.hpp"

using System::InvalidOperationException;
using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XContainer;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XObject;
using System::Xml::Linq::XText;

namespace {

    std::shared_ptr<XNode> text(const std::string& s) {
        return std::static_pointer_cast<XNode>(std::make_shared<XText>(s));
    }

    std::shared_ptr<XNode> comment(const std::string& s) {
        return std::static_pointer_cast<XNode>(std::make_shared<XComment>(s));
    }

    std::shared_ptr<XAttribute> attr(const std::string& name, const std::string& value) {
        return std::make_shared<XAttribute>(XName(name), value);
    }

} // namespace

// --- The owner is alive: nothing changes -------------------------------------------------

TEST(XLinqLifetimeTests, LiveElementOwner_ChildAndAttributeStillReportIt) {
    XElement el(XName("root"));
    auto t = text("hello");
    auto a = attr("k", "v");
    el.Add(t);
    el.Add(comment("c"));
    el.Add(a);
    EXPECT_EQ(t->getParentProperty(), &el);
    EXPECT_EQ(a->getParentProperty(), &el);
    EXPECT_EQ(el.Nodes().size(), 2u);
    EXPECT_NE(t->getNextNodeProperty(), nullptr);
}

TEST(XLinqLifetimeTests, LiveDocumentOwner_RootStillReportsIt) {
    XDocument doc;
    auto root = std::make_shared<XElement>(XName("root"));
    doc.Add(std::static_pointer_cast<XNode>(root));
    EXPECT_EQ(root->getDocumentProperty(), &doc);
    EXPECT_EQ(root->getParentProperty(), nullptr);  // an XDocument is not an XElement
}

// --- Owner destruction detaches child nodes (X01-X09) ------------------------------------

// Probe case X01, the family's headline: XObject::getParentProperty() dispatched a virtual call
// through the freed parent, so with no sanitizer at all this aborted the process with
// "pure virtual method called".
TEST(XLinqLifetimeTests, RetainedTextNode_AfterHeapElementDestroyed_HasNoParent) {
    std::shared_ptr<XNode> t;
    {
        auto el = std::make_shared<XElement>(XName("root"));
        t = text("hello");
        el->Add(t);
        el->Add(comment("c"));
        ASSERT_EQ(t->getParentProperty(), el.get());
    }
    EXPECT_EQ(t->getParentProperty(), nullptr);
    EXPECT_EQ(t->getDocumentProperty(), nullptr);
}

TEST(XLinqLifetimeTests, RetainedTextNode_AfterAutomaticStorageElementDestroyed_HasNoParent) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("hello");
        el.Add(t);
        ASSERT_EQ(t->getParentProperty(), &el);
    }
    EXPECT_EQ(t->getParentProperty(), nullptr);
}

// The parent link is the only thing a former virtual dispatch could have gone through, so a
// null link makes that dispatch structurally impossible. Probe cases X01, X02, X10-X12.
TEST(XLinqLifetimeTests, RetainedNode_AfterOwnerDestroyed_ParentIsNotDereferenceable) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("hello");
        el.Add(t);
    }
    XElement* parent = t->getParentProperty();
    ASSERT_EQ(parent, nullptr);
    EXPECT_EQ(t->getNodeTypeProperty(), System::Xml::XmlNodeType::Text);
}

// Probe cases X03-X06: sibling navigation read the freed parent's children_ vector.
TEST(XLinqLifetimeTests, RetainedNode_AfterOwnerDestroyed_HasNoSiblings) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("a");
        el.Add(t);
        el.Add(text("b"));
        el.Add(text("c"));
        ASSERT_NE(t->getNextNodeProperty(), nullptr);
    }
    EXPECT_EQ(t->getNextNodeProperty(), nullptr);
    EXPECT_EQ(t->getPreviousNodeProperty(), nullptr);
    EXPECT_TRUE(t->NodesBeforeSelf().empty());
    EXPECT_TRUE(t->NodesAfterSelf().empty());
}

// Probe cases X07, X08, X26: the mutating paths entered a non-const member function on freed
// storage. With no parent they take the same guarded early-throw a detached node already takes.
TEST(XLinqLifetimeTests, RetainedNode_AfterOwnerDestroyed_RemoveThrowsInsteadOfTouchingFreedStorage) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("a");
        el.Add(t);
    }
    EXPECT_THROW(t->Remove(), InvalidOperationException);
}

TEST(XLinqLifetimeTests, RetainedNode_AfterOwnerDestroyed_ReplaceWithThrowsInsteadOfTouchingFreedStorage) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("a");
        el.Add(t);
    }
    EXPECT_THROW(t->ReplaceWith(text("r")), InvalidOperationException);
    EXPECT_THROW(t->ReplaceWith(std::vector<std::shared_ptr<XNode>>{text("r1"), text("r2")}),
                 InvalidOperationException);
}

// Probe case X09.
TEST(XLinqLifetimeTests, RetainedNode_AfterOwnerDestroyed_CompareDocumentOrderThrows) {
    std::shared_ptr<XNode> a;
    std::shared_ptr<XNode> b;
    {
        XElement el(XName("root"));
        a = text("a");
        b = text("b");
        el.Add(a);
        el.Add(b);
    }
    EXPECT_THROW((void)XNode::CompareDocumentOrder(a.get(), b.get()), InvalidOperationException);
}

// --- Owner destruction detaches attributes (X10-X13, X25) --------------------------------

// Probe case X10: a retained attribute's getParentProperty() aborted the process.
TEST(XLinqLifetimeTests, RetainedAttribute_AfterOwnerDestroyed_HasNoParent) {
    std::shared_ptr<XAttribute> a;
    {
        auto el = std::make_shared<XElement>(XName("root"));
        a = attr("k", "v");
        el->Add(a);
        ASSERT_EQ(a->getParentProperty(), el.get());
    }
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(a->getValueProperty(), "v");
    EXPECT_EQ(a->getNameProperty().getLocalNameProperty(), "k");
}

// Probe case X13: next_ is a SECOND borrowed link, and it dangles independently of parent_.
// ~XElement must clear both.
TEST(XLinqLifetimeTests, RetainedAttribute_AfterOwnerDestroyed_HasNoNextAttribute) {
    std::shared_ptr<XAttribute> first;
    {
        XElement el(XName("root"));
        first = attr("a", "1");
        el.Add(first);
        el.Add(attr("b", "2"));
        el.Add(attr("c", "3"));
        ASSERT_NE(first->getNextAttributeProperty(), nullptr);
    }
    EXPECT_EQ(first->getNextAttributeProperty(), nullptr);
    EXPECT_EQ(first->getPreviousAttributeProperty(), nullptr);
}

// Probe cases X11, X12, X25.
TEST(XLinqLifetimeTests, RetainedAttribute_AfterOwnerDestroyed_RemoveThrowsInsteadOfTouchingFreedStorage) {
    std::shared_ptr<XAttribute> a;
    std::shared_ptr<XAttribute> b;
    {
        XElement el(XName("root"));
        a = attr("a", "1");
        b = attr("b", "2");
        el.Add(a);
        el.Add(b);
    }
    EXPECT_EQ(a->getPreviousAttributeProperty(), nullptr);
    EXPECT_THROW(a->Remove(), InvalidOperationException);
    EXPECT_THROW(b->Remove(), InvalidOperationException);
}

TEST(XLinqLifetimeTests, EveryAttributeOfAMultiAttributeOwnerIsDetached) {
    std::vector<std::shared_ptr<XAttribute>> retained;
    {
        XElement el(XName("root"));
        for (int i = 0; i < 8; ++i) {
            auto a = attr("a" + std::to_string(i), "v");
            el.Add(a);
            retained.push_back(a);
        }
        ASSERT_EQ(el.getAttributesProperty().size(), 8u);
    }
    ASSERT_EQ(retained.size(), 8u);
    for (const auto& a : retained) {
        EXPECT_EQ(a->getParentProperty(), nullptr);
        EXPECT_EQ(a->getNextAttributeProperty(), nullptr);
    }
}

TEST(XLinqLifetimeTests, EveryChildOfAMultiChildOwnerIsDetached) {
    std::vector<std::shared_ptr<XNode>> retained;
    {
        XElement el(XName("root"));
        for (int i = 0; i < 8; ++i) {
            auto t = text("t" + std::to_string(i));
            el.Add(t);
            retained.push_back(t);
        }
    }
    ASSERT_EQ(retained.size(), 8u);
    for (const auto& n : retained) {
        EXPECT_EQ(n->getParentProperty(), nullptr);
        EXPECT_EQ(n->getNextNodeProperty(), nullptr);
    }
}

// --- Nested trees and documents (X18, X19) -----------------------------------------------

TEST(XLinqLifetimeTests, RetainedGrandchild_AfterWholeTreeDestroyed_HasNoParent) {
    std::shared_ptr<XNode> leaf;
    std::shared_ptr<XElement> mid;
    {
        auto root = std::make_shared<XElement>(XName("root"));
        mid = std::make_shared<XElement>(XName("mid"));
        leaf = text("leaf");
        mid->Add(leaf);
        root->Add(std::static_pointer_cast<XNode>(mid));
        ASSERT_EQ(leaf->getParentProperty(), mid.get());
        ASSERT_EQ(mid->getParentProperty(), root.get());
    }
    EXPECT_EQ(mid->getParentProperty(), nullptr);
    EXPECT_EQ(leaf->getParentProperty(), mid.get());  // mid is still alive and still owns it
}

TEST(XLinqLifetimeTests, RetainedLeafOnly_AfterWholeTreeDestroyed_HasNoParent) {
    std::shared_ptr<XNode> leaf;
    {
        auto root = std::make_shared<XElement>(XName("root"));
        auto mid = std::make_shared<XElement>(XName("mid"));
        leaf = text("leaf");
        mid->Add(leaf);
        root->Add(std::static_pointer_cast<XNode>(mid));
    }
    EXPECT_EQ(leaf->getParentProperty(), nullptr);
    EXPECT_EQ(leaf->getDocumentProperty(), nullptr);
}

// Probe case X18: XDocument is an XContainer, so it inherits the same destructor.
TEST(XLinqLifetimeTests, RetainedRoot_AfterDocumentDestroyed_HasNoDocument) {
    std::shared_ptr<XElement> root;
    {
        auto doc = std::make_shared<XDocument>();
        root = std::make_shared<XElement>(XName("root"));
        doc->Add(std::static_pointer_cast<XNode>(root));
        ASSERT_EQ(root->getDocumentProperty(), doc.get());
    }
    EXPECT_EQ(root->getDocumentProperty(), nullptr);
    EXPECT_EQ(root->getParentProperty(), nullptr);
}

// Probe case X19: two freed levels between the retained node and the destroyed document.
TEST(XLinqLifetimeTests, RetainedGrandchild_AfterDocumentDestroyed_HasNoDocument) {
    std::shared_ptr<XNode> leaf;
    {
        auto doc = std::make_shared<XDocument>();
        auto root = std::make_shared<XElement>(XName("root"));
        leaf = text("leaf");
        root->Add(leaf);
        doc->Add(std::static_pointer_cast<XNode>(root));
        ASSERT_EQ(leaf->getDocumentProperty(), doc.get());
    }
    EXPECT_EQ(leaf->getDocumentProperty(), nullptr);
    EXPECT_EQ(leaf->getParentProperty(), nullptr);
}

// --- Only the destroying owner's own links are cleared -----------------------------------

// XContainer::Add moves an already-attached node (the documented deviation from .NET's clone,
// XContainer.hpp's class comment, probe case X21). The former owner must not then clear a link
// that now names the new one.
TEST(XLinqLifetimeTests, NodeMovedToAnotherOwner_KeepsTheNewParentAfterTheOldOwnerDies) {
    XElement destination(XName("dst"));
    std::shared_ptr<XNode> t;
    {
        XElement source(XName("src"));
        t = text("kid");
        source.Add(t);
        destination.Add(t);  // moves it
        ASSERT_EQ(t->getParentProperty(), &destination);
        ASSERT_EQ(source.Nodes().size(), 0u);
    }
    EXPECT_EQ(t->getParentProperty(), &destination);
    EXPECT_EQ(destination.Nodes().size(), 1u);
}

TEST(XLinqLifetimeTests, AttributeMovedToAnotherOwner_KeepsTheNewParentAfterTheOldOwnerDies) {
    XElement destination(XName("dst"));
    std::shared_ptr<XAttribute> a;
    {
        XElement source(XName("src"));
        a = attr("k", "v");
        source.Add(a);
        destination.Add(a);  // moves it
        ASSERT_EQ(a->getParentProperty(), &destination);
        ASSERT_EQ(source.getAttributesProperty().size(), 0u);
    }
    EXPECT_EQ(a->getParentProperty(), &destination);
}

// --- Structural mutation before destruction ----------------------------------------------

TEST(XLinqLifetimeTests, NodeRemovedBeforeOwnerDestroyed_StaysDetached) {
    std::shared_ptr<XNode> t;
    {
        XElement el(XName("root"));
        t = text("a");
        el.Add(t);
        t->Remove();
        EXPECT_EQ(t->getParentProperty(), nullptr);
    }
    EXPECT_EQ(t->getParentProperty(), nullptr);
}

TEST(XLinqLifetimeTests, AttributeRemovedBeforeOwnerDestroyed_StaysDetachedWithNoNext) {
    std::shared_ptr<XAttribute> a;
    {
        XElement el(XName("root"));
        a = attr("a", "1");
        el.Add(a);
        el.Add(attr("b", "2"));
        a->Remove();
        EXPECT_EQ(a->getParentProperty(), nullptr);
        EXPECT_EQ(a->getNextAttributeProperty(), nullptr);
    }
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(a->getNextAttributeProperty(), nullptr);
}

TEST(XLinqLifetimeTests, ReplacedNodeBeforeOwnerDestroyed_BothEndDetached) {
    std::shared_ptr<XNode> replaced;
    std::shared_ptr<XNode> replacement;
    {
        XElement el(XName("root"));
        replaced = text("old");
        replacement = text("new");
        el.Add(replaced);
        replaced->ReplaceWith(replacement);
        EXPECT_EQ(replaced->getParentProperty(), nullptr);
        EXPECT_EQ(replacement->getParentProperty(), &el);
    }
    EXPECT_EQ(replaced->getParentProperty(), nullptr);
    EXPECT_EQ(replacement->getParentProperty(), nullptr);
}

TEST(XLinqLifetimeTests, RemoveAllBeforeOwnerDestroyed_RetainedObjectsRemainDetached) {
    std::shared_ptr<XNode> t;
    std::shared_ptr<XAttribute> a;
    {
        XElement el(XName("root"));
        t = text("x");
        a = attr("k", "v");
        el.Add(t);
        el.Add(a);
        el.RemoveAll();
        EXPECT_EQ(t->getParentProperty(), nullptr);
        EXPECT_EQ(a->getParentProperty(), nullptr);
    }
    EXPECT_EQ(t->getParentProperty(), nullptr);
    EXPECT_EQ(a->getParentProperty(), nullptr);
}

// --- Degenerate owners --------------------------------------------------------------------

TEST(XLinqLifetimeTests, EmptyOwnersDestroyCleanly) {
    { XElement el(XName("empty")); EXPECT_EQ(el.Nodes().size(), 0u); }
    { XDocument doc; EXPECT_EQ(doc.Nodes().size(), 0u); }
    { auto el = std::make_shared<XElement>(XName("empty")); EXPECT_FALSE(el->getHasAttributesProperty()); }
    { auto doc = std::make_shared<XDocument>(); EXPECT_EQ(doc->Nodes().size(), 0u); }
    SUCCEED();
}

// An element with attributes but no children, and one with children but no attributes, exercise
// each destructor loop with the other one empty.
TEST(XLinqLifetimeTests, AttributesOnlyAndChildrenOnlyOwnersBothDetach) {
    std::shared_ptr<XAttribute> a;
    std::shared_ptr<XNode> t;
    {
        XElement attrsOnly(XName("a"));
        a = attr("k", "v");
        attrsOnly.Add(a);
    }
    {
        XElement kidsOnly(XName("k"));
        t = text("x");
        kidsOnly.Add(t);
    }
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(t->getParentProperty(), nullptr);
}

// --- Exception paths ----------------------------------------------------------------------

TEST(XLinqLifetimeTests, OwnerDestroyedDuringExceptionUnwinding_DetachesWithoutTerminating) {
    std::shared_ptr<XNode> t;
    std::shared_ptr<XAttribute> a;
    bool caught = false;
    try {
        XElement el(XName("root"));
        t = text("x");
        a = attr("k", "v");
        el.Add(t);
        el.Add(a);
        throw InvalidOperationException("unwind");
    } catch (const InvalidOperationException&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->getParentProperty(), nullptr);
    EXPECT_EQ(a->getParentProperty(), nullptr);
    EXPECT_EQ(a->getNextAttributeProperty(), nullptr);
}

TEST(XLinqLifetimeTests, NestedOwnersDestroyedDuringExceptionUnwinding_DetachEveryLevel) {
    std::shared_ptr<XNode> leaf;
    try {
        XDocument doc;
        auto root = std::make_shared<XElement>(XName("root"));
        leaf = text("leaf");
        root->Add(leaf);
        doc.Add(std::static_pointer_cast<XNode>(root));
        throw InvalidOperationException("unwind");
    } catch (const InvalidOperationException&) {
    }
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->getParentProperty(), nullptr);
    EXPECT_EQ(leaf->getDocumentProperty(), nullptr);
}

// A rejected attribute Add must leave the attribute with its real owner, and the element that
// rejected it must not detach it when it dies.
TEST(XLinqLifetimeTests, RejectedDuplicateAttribute_LeavesTheOriginalOwnedAfterTheRejecterDies) {
    XElement owner(XName("owner"));
    auto a = attr("k", "1");
    owner.Add(a);
    {
        XElement rejecter(XName("rejecter"));
        rejecter.Add(attr("k", "existing"));
        EXPECT_THROW(rejecter.Add(a), InvalidOperationException);
        EXPECT_EQ(rejecter.getAttributesProperty().size(), 1u);
    }
    EXPECT_EQ(a->getParentProperty(), &owner);
}

// Adding a node into its own subtree is rejected by InsertNodeAt's ancestor guard. The guard
// walks the parent chain, which after #1890 is guaranteed to contain no freed link.
TEST(XLinqLifetimeTests, SelfInsertionIsStillRejected_AndTheAncestorWalkTouchesNoFreedLink) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    root->Add(std::static_pointer_cast<XNode>(mid));
    EXPECT_THROW(mid->Add(std::static_pointer_cast<XNode>(root)), InvalidOperationException);
}

// --- Re-attachment (X14) ------------------------------------------------------------------

// Probe case X14: re-adding an orphan to a LIVE element made InsertNodeAt call
// `n->parent_->RemoveNode(n)` on the freed former owner. With a null link it takes the
// no-former-owner path a detached node already takes.
TEST(XLinqLifetimeTests, RetainedNode_IsReAddableAfterItsOwnerIsDestroyed) {
    std::shared_ptr<XNode> t;
    {
        XElement dead(XName("dead"));
        t = text("hello");
        dead.Add(t);
    }
    XElement live(XName("live"));
    EXPECT_NO_THROW(live.Add(t));
    EXPECT_EQ(t->getParentProperty(), &live);
    EXPECT_EQ(live.getValueProperty(), "hello");
}

TEST(XLinqLifetimeTests, RetainedAttribute_IsReAddableAfterItsOwnerIsDestroyed) {
    std::shared_ptr<XAttribute> a;
    {
        XElement dead(XName("dead"));
        a = attr("k", "v");
        dead.Add(a);
    }
    XElement live(XName("live"));
    EXPECT_NO_THROW(live.Add(a));
    EXPECT_EQ(a->getParentProperty(), &live);
    EXPECT_EQ(live.getAttributeValue("k").value_or(""), "v");
}

TEST(XLinqLifetimeTests, RetainedSubtree_IsReAddableWithItsOwnChildrenIntact) {
    std::shared_ptr<XElement> subtree;
    {
        XDocument doc;
        subtree = std::make_shared<XElement>(XName("kept"));
        subtree->Add(text("inner"));
        subtree->Add(attr("k", "v"));
        doc.Add(std::static_pointer_cast<XNode>(subtree));
    }
    ASSERT_EQ(subtree->getParentProperty(), nullptr);
    EXPECT_EQ(subtree->getValueProperty(), "inner");
    EXPECT_EQ(subtree->getAttributesProperty().size(), 1u);

    XElement live(XName("live"));
    EXPECT_NO_THROW(live.Add(std::static_pointer_cast<XNode>(subtree)));
    EXPECT_EQ(subtree->getParentProperty(), &live);
    EXPECT_EQ(live.getValueProperty(), "inner");
}

// --- Allocator reuse (X22) ----------------------------------------------------------------

// Probe case X22 is the shape a sanitizer's quarantine hides: the freed element's storage is
// reused by an unrelated XElement at the same address, and the retained child then reported that
// squatter's name as its parent's. A null parent link cannot name a squatter.
TEST(XLinqLifetimeTests, RetainedNode_ReportsNoParentEvenIfTheFreedOwnersStorageIsReused) {
    std::shared_ptr<XNode> t;
    {
        auto el = std::make_shared<XElement>(XName("original"));
        t = text("hello");
        el->Add(t);
    }
    std::vector<std::shared_ptr<XElement>> squatters;
    for (int i = 0; i < 16; ++i) {
        squatters.push_back(std::make_shared<XElement>(XName("squatter")));
        squatters.back()->Add(text("s"));
    }
    EXPECT_EQ(t->getParentProperty(), nullptr);
    EXPECT_EQ(t->getDocumentProperty(), nullptr);
}

// --- Representation invariants ------------------------------------------------------------

// UPDATED BY #2199 on 2026-08-19, which grew every one of these by exactly ONE POINTER: the
// unique_ptr to XObject's Changed/Changing registration block. The growth was granted per action
// (docs/StandingApprovals.md SA-13) after being declined for a different purpose on 2026-07-31.
// #1890's invariant is unchanged in KIND -- these figures must still not move without an
// approval; only the numbers did.
static_assert(sizeof(XObject) == 24, "#1890/#2199: XObject must stay 24 bytes (was 16)");
static_assert(sizeof(XNode) == 24, "#1890/#2199: XNode must stay 24 bytes (was 16)");
static_assert(sizeof(XContainer) == 48, "#1890/#2199: XContainer must stay 48 bytes (was 40)");
static_assert(sizeof(XElement) == 136, "#1890/#2199: XElement must stay 136 bytes (was 128)");
static_assert(sizeof(XAttribute) == 128, "#1890/#2199: XAttribute must stay 128 bytes (was 120)");
static_assert(sizeof(XText) == 56, "#1890/#2199: XText must stay 56 bytes (was 48)");
static_assert(sizeof(XDocument) == 64, "#1890/#2199: XDocument must stay 64 bytes (was 56)");

// The growth is exactly one pointer on EVERY type, asserted as a relationship so a second member
// added later cannot hide behind a literal that someone updated by hand.
static_assert(sizeof(XObject) == 16 + sizeof(void*));
static_assert(sizeof(XElement) == 128 + sizeof(void*));
static_assert(sizeof(XAttribute) == 120 + sizeof(void*));
static_assert(alignof(XElement) == 8 && alignof(XAttribute) == 8, "#1890: alignment unchanged");
static_assert(std::has_virtual_destructor_v<XObject>, "#1890: destruction stays virtual");
static_assert(std::is_nothrow_destructible_v<XContainer>, "#1890: ~XContainer must be noexcept");
static_assert(std::is_nothrow_destructible_v<XElement>, "#1890: ~XElement must be noexcept");
// The value semantics XObject already deleted hierarchy-wide, which #1890 must not disturb.
static_assert(!std::is_copy_constructible_v<XElement>, "#1890: XObject copy stays deleted");
static_assert(!std::is_copy_assignable_v<XElement>, "#1890: XObject copy-assign stays deleted");
static_assert(!std::is_move_constructible_v<XElement>, "#1890: XObject move stays deleted");

TEST(XLinqLifetimeTests, PublicLayoutIsUnchangedByTheDetachContract) {
    // UPDATED BY #2199: every figure grew by exactly one pointer for XObject's Changed/Changing
    // registration block. The property this case guards is unchanged -- the DETACH CONTRACT costs
    // no layout -- so it is written relative to #2199's approved baseline rather than restated as
    // fresh literals, which keeps it about detachment rather than about notification.
    EXPECT_EQ(sizeof(XObject), 16u + sizeof(void*));
    EXPECT_EQ(sizeof(XNode), 16u + sizeof(void*));
    EXPECT_EQ(sizeof(XContainer), 40u + sizeof(void*));
    EXPECT_EQ(sizeof(XElement), 128u + sizeof(void*));
    EXPECT_EQ(sizeof(XAttribute), 120u + sizeof(void*));
    EXPECT_EQ(sizeof(XText), 48u + sizeof(void*));
    EXPECT_EQ(sizeof(XDocument), 56u + sizeof(void*));
}
