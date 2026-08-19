// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1898 (SR-AUD-333, CCF-019): the Xml.Linq borrowed-view
// contract, stated in docs/OwnedTreeLifetimeContractPlan.md section 42.2 and now measurable.
//
// docs/OwnedTreeLifetimeContractPlan.md section 31 item 5 asked Extensions::Ancestors and
// AncestorsAndSelf to "return owning handles". The user rejected that wording on 2026-07-31 as
// non-implementable, and section 42.2 records why: the topmost ancestor has no parent to own it,
// and an XElement in automatic storage has no control block at all, so not even
// std::enable_shared_from_this would rescue it. What is achievable, and what this suite pins, is
// a contract stated as preconditions, postconditions, invalidation and failure behaviour.
//
// What this suite deliberately does NOT do: use a borrowed view after its owner has been
// destroyed. That is probe cases X15 and X17, it is the undefined behaviour the contract exists
// to describe, and turning it into a test would only pin an ASan report.
//
// TICKET #1899 IS CLOSED (2026-08-19) AND THE CONTRACT STAYS AS #1898 LEFT IT. #1899 existed to
// make X15/X17 unreachable rather than merely documented. Its design was completed and it proves
// the ORIGINAL REQUIREMENT IMPOSSIBLE, for two INDEPENDENT reasons -- either alone is sufficient:
//
//   1. An owning handle to an object no shared_ptr owns cannot be manufactured. The TOPMOST
//      ancestor has no parent to own it, so there is nothing to hand out a share of.
//   2. XElement, XDocument and XContainer are routinely AUTOMATIC-STORAGE objects -- 51 such
//      declarations in this repository's own tests alone -- so they have no control block, and
//      std::enable_shared_from_this would throw std::bad_weak_ptr rather than rescue the caller.
//      It would convert a latent use-after-free into a guaranteed throw at a correct call site.
//
// The alternative offered was to change Ancestors/AncestorsAndSelf's public signature to a
// non-owning view type that cannot outlive the tree -- a public source break under SA-2/SA-10.
// That was DECLINED on 2026-08-19 (docs/StandingApprovals.md SA-13).
//
// The proof is recorded HERE, beside the contract it justifies, so #1899 is not reopened from the
// same direction by a future reader who sees only "documented, not enforced" and assumes nobody
// tried. Decl1899_* below pins the two premises, because a proof whose premises silently stop
// being true is not a proof.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "System/Xml/Linq/Extensions.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XText.hpp"

using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XText;
namespace Extensions = System::Xml::Linq::Extensions;

namespace {

    std::vector<std::string> localNames(const std::vector<XElement*>& v) {
        std::vector<std::string> out;
        out.reserve(v.size());
        for (auto* e : v) out.push_back(e->getNameProperty().getLocalNameProperty());
        return out;
    }

} // namespace

// --- Ancestors: the postcondition ---------------------------------------------------------

TEST(XLinqBorrowedViewTests, Ancestors_NamesEveryAncestorInOrder) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(std::shared_ptr<XNode>(mid));
    mid->Add(std::shared_ptr<XNode>(leaf));

    std::vector<std::shared_ptr<XNode>> src{leaf};
    EXPECT_EQ(localNames(Extensions::Ancestors(src)), (std::vector<std::string>{"mid", "root"}));
}

TEST(XLinqBorrowedViewTests, Ancestors_OfADirectChild_IsJustTheParent) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto child = std::make_shared<XText>("t");
    root->Add(std::shared_ptr<XNode>(child));

    std::vector<std::shared_ptr<XNode>> src{child};
    EXPECT_EQ(localNames(Extensions::Ancestors(src)), (std::vector<std::string>{"root"}));
}

TEST(XLinqBorrowedViewTests, Ancestors_Named_FiltersButKeepsOrder) {
    auto root = std::make_shared<XElement>(XName("keep"));
    auto mid = std::make_shared<XElement>(XName("skip"));
    auto leaf = std::make_shared<XElement>(XName("keep"));
    root->Add(std::shared_ptr<XNode>(mid));
    mid->Add(std::shared_ptr<XNode>(leaf));

    std::vector<std::shared_ptr<XNode>> src{leaf};
    EXPECT_EQ(localNames(Extensions::Ancestors(src, XName("keep"))), (std::vector<std::string>{"keep"}));
}

TEST(XLinqBorrowedViewTests, Ancestors_OfANullOrDetachedSource_ContributesNothing) {
    auto detached = std::make_shared<XElement>(XName("detached"));
    std::vector<std::shared_ptr<XNode>> src{nullptr, detached};
    EXPECT_TRUE(Extensions::Ancestors(src).empty());
    EXPECT_TRUE(Extensions::Ancestors(src, XName("x")).empty());
}

TEST(XLinqBorrowedViewTests, AncestorsAndSelf_PutsTheSourceElementFirst) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(std::shared_ptr<XNode>(leaf));

    std::vector<std::shared_ptr<XElement>> src{leaf};
    EXPECT_EQ(localNames(Extensions::AncestorsAndSelf(src)), (std::vector<std::string>{"leaf", "root"}));
    EXPECT_EQ(localNames(Extensions::AncestorsAndSelf(src, XName("leaf"))), (std::vector<std::string>{"leaf"}));
}

TEST(XLinqBorrowedViewTests, AncestorsAndSelf_OfADetachedElement_IsJustItself) {
    auto lone = std::make_shared<XElement>(XName("lone"));
    std::vector<std::shared_ptr<XElement>> src{lone};
    EXPECT_EQ(localNames(Extensions::AncestorsAndSelf(src)), (std::vector<std::string>{"lone"}));
}

// A document is not an XElement, so getParentProperty() stops at the root element and the walk
// stops with it. This pins where the ancestor chain ends.
TEST(XLinqBorrowedViewTests, Ancestors_UnderADocument_StopAtTheRootElement) {
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("root"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(std::shared_ptr<XNode>(leaf));
    doc->Add(std::shared_ptr<XNode>(root));

    std::vector<std::shared_ptr<XNode>> src{leaf};
    EXPECT_EQ(localNames(Extensions::Ancestors(src)), (std::vector<std::string>{"root"}));
    EXPECT_EQ(leaf->getDocumentProperty(), doc.get());
}

// --- Ancestors: the invalidation clause ---------------------------------------------------

// "Invalidated by the destruction of the element it names, and by nothing else." Mutating the
// tree around the ancestors must not disturb the result, because an entry names an object.
TEST(XLinqBorrowedViewTests, AncestorsResult_SurvivesTreeMutation) {
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(std::shared_ptr<XNode>(mid));
    mid->Add(std::shared_ptr<XNode>(leaf));

    std::vector<std::shared_ptr<XNode>> src{leaf};
    auto ancestors = Extensions::Ancestors(src);
    ASSERT_EQ(ancestors.size(), 2u);

    for (int i = 0; i < 64; ++i) root->Add(std::shared_ptr<XNode>(std::make_shared<XText>("t")));
    mid->Add(std::shared_ptr<XNode>(std::make_shared<XElement>(XName("sib"))));
    root->RemoveNodes(); // removes `mid` from `root`, but `mid` is still owned by our shared_ptr

    EXPECT_EQ(localNames(ancestors), (std::vector<std::string>{"mid", "root"}));
    EXPECT_EQ(ancestors[0], mid.get());
    EXPECT_EQ(ancestors[1], root.get());
}

// --- getAttributesProperty(): postcondition and the reallocation clause --------------------

TEST(XLinqBorrowedViewTests, AttributesProperty_NamesTheStoreInDocumentOrder) {
    auto el = std::make_shared<XElement>(XName("e"));
    el->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    el->Add(std::make_shared<XAttribute>(XName("b"), "2"));

    const auto& attrs = el->getAttributesProperty();
    ASSERT_EQ(attrs.size(), 2u);
    EXPECT_EQ(attrs[0]->getNameProperty().getLocalNameProperty(), "a");
    EXPECT_EQ(attrs[1]->getNameProperty().getLocalNameProperty(), "b");
}

// Probe case X16: the reference names the vector *object*, which lives inside the element, so
// reallocating its buffer does not invalidate it.
TEST(XLinqBorrowedViewTests, AttributesPropertyReference_SurvivesReallocation) {
    auto el = std::make_shared<XElement>(XName("e"));
    el->Add(std::make_shared<XAttribute>(XName("a0"), "0"));
    const std::vector<std::shared_ptr<XAttribute>>& attrs = el->getAttributesProperty();

    for (int i = 1; i < 64; ++i)
        el->Add(std::make_shared<XAttribute>(XName("a" + std::to_string(i)), "v"));

    EXPECT_EQ(attrs.size(), 64u);
    EXPECT_EQ(attrs[63]->getNameProperty().getLocalNameProperty(), "a63");
}

TEST(XLinqBorrowedViewTests, AttributesPropertyReference_SurvivesRemoval) {
    auto el = std::make_shared<XElement>(XName("e"));
    auto a = std::make_shared<XAttribute>(XName("a"), "1");
    el->Add(a);
    el->Add(std::make_shared<XAttribute>(XName("b"), "2"));
    const auto& attrs = el->getAttributesProperty();

    el->RemoveAttribute(a.get());

    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0]->getNameProperty().getLocalNameProperty(), "b");
}

// --- Attributes(): the owning alternative, which is the point of documenting the other -----

TEST(XLinqBorrowedViewTests, Attributes_ReturnsOwningHandlesThatOutliveTheElement) {
    std::vector<std::shared_ptr<XAttribute>> owned;
    {
        auto el = std::make_shared<XElement>(XName("e"));
        el->Add(std::make_shared<XAttribute>(XName("a"), "1"));
        el->Add(std::make_shared<XAttribute>(XName("b"), "2"));
        owned = el->Attributes();
        ASSERT_EQ(owned.size(), 2u);
        EXPECT_EQ(owned[0]->getParentProperty(), el.get());
    }
    // The element is gone; the attributes are alive, detached and re-addable - exactly the state
    // RemoveAttributes() leaves them in (#1890).
    ASSERT_EQ(owned.size(), 2u);
    EXPECT_EQ(owned[0]->getNameProperty().getLocalNameProperty(), "a");
    EXPECT_EQ(owned[0]->getValueProperty(), "1");
    EXPECT_EQ(owned[0]->getParentProperty(), nullptr);
    EXPECT_EQ(owned[0]->getNextAttributeProperty(), nullptr);

    auto newOwner = std::make_shared<XElement>(XName("new"));
    newOwner->Add(owned[0]);
    EXPECT_EQ(owned[0]->getParentProperty(), newOwner.get());
}

TEST(XLinqBorrowedViewTests, Attributes_IsASnapshot_NotAliasedToTheStore) {
    auto el = std::make_shared<XElement>(XName("e"));
    el->Add(std::make_shared<XAttribute>(XName("a"), "1"));
    auto snapshot = el->Attributes();
    el->Add(std::make_shared<XAttribute>(XName("b"), "2"));

    EXPECT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(el->getAttributesProperty().size(), 2u);
}

TEST(XLinqBorrowedViewTests, Attributes_AndAttributesProperty_AgreeOnContent) {
    auto el = std::make_shared<XElement>(XName("e"));
    for (int i = 0; i < 8; ++i)
        el->Add(std::make_shared<XAttribute>(XName("a" + std::to_string(i)), "v"));

    const auto& borrowed = el->getAttributesProperty();
    const auto owned = el->Attributes();
    ASSERT_EQ(borrowed.size(), owned.size());
    for (size_t i = 0; i < owned.size(); ++i) EXPECT_EQ(borrowed[i], owned[i]);
}

// ---------------------------------------------------------------------------------------------
// #1899 closed 2026-08-19: the two premises of its impossibility proof, pinned.
//
// A proof whose premises silently stop being true is not a proof. These two cases fail the day
// either premise does, which is exactly when #1899 would be worth reopening.
// ---------------------------------------------------------------------------------------------

TEST(XLinqBorrowedViewTests, Decl1899_AutomaticStorageNodesHaveNoControlBlockToShareFrom) {
    // PREMISE 2. XElement/XDocument/XContainer are routinely automatic-storage objects, so
    // enable_shared_from_this could not rescue a borrowed view: there is no control block, and
    // shared_from_this() would throw std::bad_weak_ptr at a CORRECT call site. This would trade a
    // latent use-after-free for a guaranteed throw, not for safety.
    //
    // Asserted structurally rather than by trying it: the type does NOT derive from
    // enable_shared_from_this, and an automatic-storage instance is legal and usable.
    static_assert(!std::is_base_of_v<std::enable_shared_from_this<XElement>, XElement>,
                  "#1899: adding this base would make automatic-storage XElements throw, not "
                  "make borrowed views safe");

    XElement automatic(XName("root"));            // legal, and the shape the premise is about
    automatic.Add(std::shared_ptr<XNode>(std::make_shared<XElement>(XName("child"))));
    EXPECT_EQ(automatic.getNameProperty().getLocalNameProperty(), "root");
    EXPECT_EQ(automatic.Elements().size(), 1u);
}

TEST(XLinqBorrowedViewTests, Decl1899_TheTopmostAncestorHasNoParentToOwnIt) {
    // PREMISE 1. Ancestors() cannot return owning handles because the LAST element it yields has
    // no parent -- there is no object holding a shared_ptr to it that a share could be taken from.
    // The root here is deliberately held by a shared_ptr, so this is not passing merely because
    // the whole tree is automatic: even with an owned root, the OWNER is the caller's local
    // variable, not anything inside the tree.
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(std::shared_ptr<XNode>(mid));
    mid->Add(std::shared_ptr<XNode>(leaf));

    std::vector<std::shared_ptr<XNode>> src{leaf};
    const auto chain = Extensions::Ancestors(src);
    ASSERT_FALSE(chain.empty());
    EXPECT_EQ(chain.back()->getNameProperty().getLocalNameProperty(), "root");
    EXPECT_EQ(chain.back()->getParentProperty(), nullptr)
        << "#1899 premise 1: the topmost ancestor has no parent, so nothing inside the tree owns "
           "it and no owning handle to it can be manufactured";

    // And the views really are raw pointers -- the shape the contract describes.
    static_assert(std::is_same_v<std::remove_const_t<decltype(chain)>, std::vector<XElement*>>);
}
