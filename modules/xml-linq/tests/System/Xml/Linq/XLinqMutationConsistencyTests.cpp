// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1891 (SR-AUD-333, CCF-019 section 31 item 2): a rejected
// XNode::ReplaceWith leaves the node it was replacing exactly where it was, and
// XContainer::InsertNodeAt adopts a node only once the container actually holds it.
//
// The defect this pins closed is probe case X20: ReplaceWith removed `this` and only then
// asked the container to accept the replacements, so a container that refused one - because it
// is the container itself or one of its own ancestors, or because the node kind may not live
// there - left the tree short of the node it was asked to replace. Measured pre-fix:
// `<a>victim</a>` came out as `<a/>`, with an exception as the only signal. That was the last
// remaining way CCF-019 could lose data.
//
// The contract is documented in docs/OwnedTreeLifetimeContractPlan.md sections 14.3 (X-2), 25
// (#1891) and 36. Probe identifiers (X20 for the defect, X21 for the documented move-on-Add
// deviation that must not change) are quoted so the two records stay linked.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XCData.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XText.hpp"

using System::ArgumentException;
using System::InvalidOperationException;
using System::Xml::Linq::XCData;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XText;

namespace {

    std::shared_ptr<XNode> text(const std::string& s) { return std::make_shared<XText>(s); }
    std::shared_ptr<XNode> element(const std::string& n) { return std::make_shared<XElement>(XName(n)); }

    std::string names(const XElement& parent) {
        std::string out;
        for (const auto& n : parent.Nodes()) {
            if (!out.empty()) out += ",";
            out += (n->getNodeTypeProperty() == System::Xml::XmlNodeType::Element)
                       ? std::static_pointer_cast<XElement>(n)->getNameProperty().getLocalNameProperty()
                       : std::static_pointer_cast<XText>(n)->getValueProperty();
        }
        return out;
    }

} // namespace

// --- X20: the rejected replacement must not cost the tree the node it replaced ------------

// The probe's exact shape: `root` is an ancestor of `a`, so InsertNodeAt refuses it.
TEST(XLinqMutationConsistencyTests, ReplaceWithAnAncestor_IsRejectedAndKeepsTheReplacedNode) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    auto victim = text("victim");
    a->Add(victim);

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(root)), InvalidOperationException);

    ASSERT_EQ(a->Nodes().size(), 1u);
    EXPECT_EQ(a->Nodes()[0], victim);
    EXPECT_EQ(victim->getParentProperty(), a.get());
    EXPECT_EQ(a->ToString(), "<a>victim</a>");
    EXPECT_EQ(root->getParentProperty(), nullptr);
}

// The same through the vector overload, which is the one that actually carries the loop.
TEST(XLinqMutationConsistencyTests, ReplaceWithVectorContainingAnAncestor_IsRejectedAndKeepsTheReplacedNode) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    auto victim = text("victim");
    a->Add(victim);

    EXPECT_THROW(victim->ReplaceWith(std::vector<std::shared_ptr<XNode>>{std::shared_ptr<XNode>(root)}),
                 InvalidOperationException);

    EXPECT_EQ(a->ToString(), "<a>victim</a>");
    EXPECT_EQ(victim->getParentProperty(), a.get());
}

// Replacing a node with its own container is the self case of the same guard.
TEST(XLinqMutationConsistencyTests, ReplaceWithItsOwnParent_IsRejectedAndKeepsTheReplacedNode) {
    auto a = std::make_shared<XElement>(XName("a"));
    auto victim = text("victim");
    a->Add(victim);

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(a)), InvalidOperationException);

    EXPECT_EQ(a->ToString(), "<a>victim</a>");
    EXPECT_EQ(victim->getParentProperty(), a.get());
}

// The other door a container refuses through: ValidateNode on the node's *kind*.
TEST(XLinqMutationConsistencyTests, ReplaceWithADocumentInsideAnElement_IsRejectedAndKeepsTheReplacedNode) {
    auto a = std::make_shared<XElement>(XName("a"));
    auto victim = text("victim");
    a->Add(victim);
    auto doc = std::make_shared<XDocument>();

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(doc)), ArgumentException);

    EXPECT_EQ(a->ToString(), "<a>victim</a>");
    EXPECT_EQ(victim->getParentProperty(), a.get());
    EXPECT_EQ(doc->getParentProperty(), nullptr);
}

// The same rejection with an XDocument as the container: CDATA may not live at document level.
TEST(XLinqMutationConsistencyTests, ReplaceWithCDataInsideADocument_IsRejectedAndKeepsTheRoot) {
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("root"));
    doc->Add(std::shared_ptr<XNode>(root));

    EXPECT_THROW(root->ReplaceWith(std::shared_ptr<XNode>(std::make_shared<XCData>("x"))), ArgumentException);

    ASSERT_NE(doc->getRootProperty(), nullptr);
    EXPECT_EQ(doc->getRootProperty(), root);
    EXPECT_EQ(root->getDocumentProperty(), doc.get());
    EXPECT_EQ(doc->Nodes().size(), 1u);
}

// Position matters: the restored node must go back where it was, not to the end.
TEST(XLinqMutationConsistencyTests, RejectedReplaceWith_RestoresTheNodeAtItsOriginalIndex) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    a->Add(text("first"));
    auto victim = text("middle");
    a->Add(victim);
    a->Add(text("last"));
    EXPECT_EQ(names(*a), "first,middle,last");

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(root)), InvalidOperationException);

    EXPECT_EQ(names(*a), "first,middle,last");
    ASSERT_EQ(a->Nodes().size(), 3u);
    EXPECT_EQ(a->Nodes()[1], victim);
    EXPECT_EQ(victim->getParentProperty(), a.get());
}

// The batch case: the first replacement is accepted, the second is refused. Everything the
// call had already inserted is undone and the replaced node comes back.
TEST(XLinqMutationConsistencyTests, RejectedLaterReplacement_UndoesTheEarlierInsertionsAndRestoresTheNode) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    a->Add(text("keep"));
    auto victim = text("victim");
    a->Add(victim);
    auto good = text("good");

    EXPECT_THROW(victim->ReplaceWith(std::vector<std::shared_ptr<XNode>>{good, std::shared_ptr<XNode>(root)}),
                 InvalidOperationException);

    EXPECT_EQ(names(*a), "keep,victim");
    ASSERT_EQ(a->Nodes().size(), 2u);
    EXPECT_EQ(a->Nodes()[1], victim);
    EXPECT_EQ(victim->getParentProperty(), a.get());
    // `good` was never attached anywhere else, so it simply ends up unattached.
    EXPECT_EQ(good->getParentProperty(), nullptr);
}

// The one bounded residue this repair deliberately does not undo, asserted rather than left
// to be discovered: a replacement that had been moved out of another container stays detached.
TEST(XLinqMutationConsistencyTests, RejectedLaterReplacement_LeavesAMovedReplacementDetached) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    auto victim = text("victim");
    a->Add(victim);

    auto donor = std::make_shared<XElement>(XName("donor"));
    auto moved = text("moved");
    donor->Add(moved);

    EXPECT_THROW(victim->ReplaceWith(std::vector<std::shared_ptr<XNode>>{moved, std::shared_ptr<XNode>(root)}),
                 InvalidOperationException);

    EXPECT_EQ(a->ToString(), "<a>victim</a>");
    EXPECT_EQ(victim->getParentProperty(), a.get());
    EXPECT_EQ(moved->getParentProperty(), nullptr); // documented: Add's move has no inverse
    EXPECT_EQ(donor->Nodes().size(), 0u);
}

// The replaced node must still be usable afterwards, exactly as if the call had never run.
TEST(XLinqMutationConsistencyTests, RejectedReplaceWith_LeavesTheReplacedNodeFullyFunctional) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    a->Add(text("before"));
    auto victim = text("victim");
    a->Add(victim);

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(root)), InvalidOperationException);

    ASSERT_NE(victim->getPreviousNodeProperty(), nullptr);
    EXPECT_EQ(victim->getNextNodeProperty(), nullptr);
    EXPECT_EQ(victim->getDocumentProperty(), nullptr);
    victim->Remove(); // must not throw "The parent is missing."
    EXPECT_EQ(a->ToString(), "<a>before</a>");
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

// A second, successful ReplaceWith after a rejected one must behave normally.
TEST(XLinqMutationConsistencyTests, RejectedReplaceWith_ThenASuccessfulOne_Works) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    auto victim = text("victim");
    a->Add(victim);

    EXPECT_THROW(victim->ReplaceWith(std::shared_ptr<XNode>(root)), InvalidOperationException);
    victim->ReplaceWith(text("fresh"));

    EXPECT_EQ(a->ToString(), "<a>fresh</a>");
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

// --- The success paths the rollback must not disturb --------------------------------------

TEST(XLinqMutationConsistencyTests, ReplaceWith_SingleNode_SwapsInPlace) {
    auto a = std::make_shared<XElement>(XName("a"));
    a->Add(text("one"));
    auto victim = text("two");
    a->Add(victim);
    a->Add(text("three"));

    victim->ReplaceWith(text("TWO"));

    EXPECT_EQ(names(*a), "one,TWO,three");
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

TEST(XLinqMutationConsistencyTests, ReplaceWith_SeveralNodes_InsertsThemInOrderAtThePosition) {
    auto a = std::make_shared<XElement>(XName("a"));
    a->Add(text("one"));
    auto victim = text("two");
    a->Add(victim);
    a->Add(text("four"));

    victim->ReplaceWith(std::vector<std::shared_ptr<XNode>>{text("2a"), text("2b"), text("2c")});

    EXPECT_EQ(names(*a), "one,2a,2b,2c,four");
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

TEST(XLinqMutationConsistencyTests, ReplaceWith_Null_JustRemoves) {
    auto a = std::make_shared<XElement>(XName("a"));
    a->Add(text("one"));
    auto victim = text("two");
    a->Add(victim);

    victim->ReplaceWith(std::shared_ptr<XNode>(nullptr));

    EXPECT_EQ(names(*a), "one");
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

TEST(XLinqMutationConsistencyTests, ReplaceWith_EmptyVector_JustRemoves) {
    auto a = std::make_shared<XElement>(XName("a"));
    auto victim = text("two");
    a->Add(victim);

    victim->ReplaceWith(std::vector<std::shared_ptr<XNode>>{});

    EXPECT_EQ(a->Nodes().size(), 0u);
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

TEST(XLinqMutationConsistencyTests, ReplaceWith_OnAnOrphan_Throws) {
    auto orphan = text("orphan");
    EXPECT_THROW(orphan->ReplaceWith(text("r")), InvalidOperationException);
    EXPECT_THROW(orphan->ReplaceWith(std::vector<std::shared_ptr<XNode>>{text("r")}),
                 InvalidOperationException);
}

// A replacement that is already attached elsewhere is *moved*, which is this port's documented
// deviation from .NET's clone-on-attach (probe case X21). The rollback must not have changed it.
TEST(XLinqMutationConsistencyTests, ReplaceWith_AnAttachedNode_StillMovesIt) {
    auto donor = std::make_shared<XElement>(XName("donor"));
    auto moved = text("moved");
    donor->Add(moved);

    auto a = std::make_shared<XElement>(XName("a"));
    auto victim = text("victim");
    a->Add(victim);

    victim->ReplaceWith(moved);

    EXPECT_EQ(a->ToString(), "<a>moved</a>");
    EXPECT_EQ(donor->Nodes().size(), 0u);
    EXPECT_EQ(moved->getParentProperty(), a.get());
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}

// Replacing the root element of a document must keep working. This is the case that rules out
// the naive "validate every replacement before removing this node" reading of the design: the
// document still has a root at that moment, so pre-validation would refuse the new one.
TEST(XLinqMutationConsistencyTests, ReplaceWith_ADocumentRoot_ReplacesIt) {
    auto doc = std::make_shared<XDocument>();
    auto oldRoot = std::make_shared<XElement>(XName("old"));
    doc->Add(std::shared_ptr<XNode>(oldRoot));

    auto newRoot = std::make_shared<XElement>(XName("new"));
    oldRoot->ReplaceWith(std::shared_ptr<XNode>(newRoot));

    ASSERT_NE(doc->getRootProperty(), nullptr);
    EXPECT_EQ(doc->getRootProperty(), newRoot);
    EXPECT_EQ(oldRoot->getParentProperty(), nullptr);
    EXPECT_EQ(doc->Nodes().size(), 1u);
}

// Two root elements in one batch: the first is accepted while the document has no root, the
// second is refused, and the original root comes back.
TEST(XLinqMutationConsistencyTests, ReplaceWith_TwoRootElements_IsRejectedAndRestoresTheOriginalRoot) {
    auto doc = std::make_shared<XDocument>();
    auto oldRoot = std::make_shared<XElement>(XName("old"));
    doc->Add(std::shared_ptr<XNode>(oldRoot));

    auto r1 = element("r1");
    auto r2 = element("r2");
    EXPECT_THROW(oldRoot->ReplaceWith(std::vector<std::shared_ptr<XNode>>{r1, r2}),
                 InvalidOperationException);

    ASSERT_NE(doc->getRootProperty(), nullptr);
    EXPECT_EQ(doc->getRootProperty(), oldRoot);
    EXPECT_EQ(doc->Nodes().size(), 1u);
    EXPECT_EQ(r1->getParentProperty(), nullptr);
    EXPECT_EQ(r2->getParentProperty(), nullptr);
}

// The container's children_ vector can hold the node's last owning reference. ReplaceWith has
// to keep it alive across its own removal, or the rollback would have nothing to restore.
TEST(XLinqMutationConsistencyTests, ReplaceWith_WhenTheContainerHoldsTheLastReference_SurvivesTheCall) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    a->Add(text("solely-owned"));
    ASSERT_EQ(a->Nodes().size(), 1u);

    XNode* raw = a->Nodes()[0].get(); // `a->children_` is the only owner
    EXPECT_THROW(raw->ReplaceWith(std::shared_ptr<XNode>(root)), InvalidOperationException);

    EXPECT_EQ(a->ToString(), "<a>solely-owned</a>");
    ASSERT_EQ(a->Nodes().size(), 1u);
    EXPECT_EQ(a->Nodes()[0].get(), raw);
    EXPECT_EQ(raw->getParentProperty(), a.get());
}

// --- InsertNodeAt: adoption happens once the container actually holds the node -------------

TEST(XLinqMutationConsistencyTests, Add_AdoptsAndStoresTogether) {
    auto a = std::make_shared<XElement>(XName("a"));
    auto child = text("c");
    a->Add(child);
    EXPECT_EQ(a->Nodes().size(), 1u);
    EXPECT_EQ(a->Nodes()[0], child);
    EXPECT_EQ(child->getParentProperty(), a.get());
}

TEST(XLinqMutationConsistencyTests, AddFirst_AdoptsAndStoresTogether) {
    auto a = std::make_shared<XElement>(XName("a"));
    a->Add(text("second"));
    auto first = text("first");
    a->AddFirst(first);
    EXPECT_EQ(names(*a), "first,second");
    EXPECT_EQ(first->getParentProperty(), a.get());
}

TEST(XLinqMutationConsistencyTests, RejectedAdd_LeavesBothTreesUntouched) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto a = std::make_shared<XElement>(XName("a"));
    root->Add(std::shared_ptr<XNode>(a));
    a->Add(text("kept"));

    EXPECT_THROW(a->Add(std::shared_ptr<XNode>(root)), InvalidOperationException);

    EXPECT_EQ(a->ToString(), "<a>kept</a>");
    EXPECT_EQ(root->getParentProperty(), nullptr);
    ASSERT_EQ(root->Nodes().size(), 1u);
    EXPECT_EQ(root->Nodes()[0], a);
}

TEST(XLinqMutationConsistencyTests, RejectedAdd_OfAWrongNodeKind_LeavesTheDonorAttached) {
    auto donor = std::make_shared<XElement>(XName("donor"));
    auto doc = std::make_shared<XDocument>();
    auto a = std::make_shared<XElement>(XName("a"));

    EXPECT_THROW(a->Add(std::shared_ptr<XNode>(doc)), ArgumentException);

    EXPECT_EQ(a->Nodes().size(), 0u);
    EXPECT_EQ(doc->getParentProperty(), nullptr);
    EXPECT_EQ(donor->Nodes().size(), 0u);
}

// Moving a node between containers must leave exactly one owner, both before and after the
// InsertNodeAt reorder.
TEST(XLinqMutationConsistencyTests, MovingANodeLeavesExactlyOneOwner) {
    auto src = std::make_shared<XElement>(XName("src"));
    auto kid = element("kid");
    src->Add(kid);
    auto dst = std::make_shared<XElement>(XName("dst"));

    dst->Add(kid);

    EXPECT_EQ(src->Nodes().size(), 0u);
    EXPECT_EQ(dst->Nodes().size(), 1u);
    EXPECT_EQ(kid->getParentProperty(), dst.get());
}

TEST(XLinqMutationConsistencyTests, MovingANodeWithinTheSameContainerReordersIt) {
    auto a = std::make_shared<XElement>(XName("a"));
    a->Add(text("one"));
    auto two = text("two");
    a->Add(two);
    EXPECT_EQ(names(*a), "one,two");

    a->AddFirst(two);

    EXPECT_EQ(names(*a), "two,one");
    EXPECT_EQ(a->Nodes().size(), 2u);
    EXPECT_EQ(two->getParentProperty(), a.get());
}

TEST(XLinqMutationConsistencyTests, ReplaceWith_ASibling_MovesItAndDropsTheReplacedNode) {
    auto a = std::make_shared<XElement>(XName("a"));
    auto sibling = text("sibling");
    a->Add(sibling);
    auto victim = text("victim");
    a->Add(victim);

    victim->ReplaceWith(sibling);

    EXPECT_EQ(names(*a), "sibling");
    EXPECT_EQ(sibling->getParentProperty(), a.get());
    EXPECT_EQ(victim->getParentProperty(), nullptr);
}
