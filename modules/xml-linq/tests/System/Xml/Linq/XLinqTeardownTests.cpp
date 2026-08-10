// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1895 (SR-AUD-333, CCF-019): releasing an Xml.Linq container
// tree is bounded by the heap, not by the call stack.
//
// The defect this pins closed is probe case X27c. Releasing a tree used to recurse - ~XContainer
// released children_, which dropped the last shared_ptr to each child node, which ran that
// child's destructor, one call frame per level - so a 20,000-deep element nest crashed with an
// ASan stack-overflow *after* it had been built successfully.
//
// The repair is documented in docs/OwnedTreeLifetimeContractPlan.md section 41 and is the twin of
// the Text.Json one: the outermost container destructor publishes a worklist; every container
// destructor that runs while a worklist is published donates its own children and returns.
//
// Attributes are deliberately not part of it: an XAttribute owns no children, so releasing one
// has never recursed. That is asserted here rather than assumed.
//
// Not covered, because it is a different root cause with its own ticket: X27d (quadratic ancestor
// guard during *construction*, #1896, blocked).

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XComment.hpp"
#include "System/Xml/Linq/XContainer.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XNode.hpp"
#include "System/Xml/Linq/XText.hpp"

using System::Xml::Linq::XAttribute;
using System::Xml::Linq::XComment;
using System::Xml::Linq::XContainer;
using System::Xml::Linq::XDocument;
using System::Xml::Linq::XElement;
using System::Xml::Linq::XName;
using System::Xml::Linq::XNode;
using System::Xml::Linq::XText;

namespace {

    // The depth probe case X27c uses: past the stack bound (X27b at 5,000 survived), inside the
    // still-quadratic build cost that ticket #1896 owns.
    constexpr int kDeep = 20000;

    std::shared_ptr<XNode> text(const std::string& s) { return std::make_shared<XText>(s); }

    std::shared_ptr<XElement> deepElementChain(int depth) {
        auto root = std::make_shared<XElement>(XName("n0"));
        XElement* cur = root.get();
        for (int i = 1; i < depth; ++i) {
            auto next = std::make_shared<XElement>(XName("n"));
            cur->Add(std::shared_ptr<XNode>(next));
            cur = next.get();
        }
        return root;
    }

    std::vector<int>* g_order = nullptr;

    class RecordingElement : public XElement {
        int tag_;
    public:
        RecordingElement(const XName& n, int tag) : XElement(n), tag_(tag) {}
        ~RecordingElement() override {
            if (g_order != nullptr) g_order->push_back(tag_);
        }
    };

} // namespace

// --- X27c: the deep chain that used to overflow the stack on release ----------------------

TEST(XLinqTeardownTests, DeepElementChain_ReleasesWithoutExhaustingTheStack) {
    auto root = deepElementChain(kDeep);
    ASSERT_EQ(root->Nodes().size(), 1u);
    root.reset(); // used to be an ASan stack-overflow / SIGSEGV
    SUCCEED();
}

TEST(XLinqTeardownTests, DeepChainUnderADocument_ReleasesWithoutExhaustingTheStack) {
    auto doc = std::make_shared<XDocument>();
    doc->Add(std::shared_ptr<XNode>(deepElementChain(kDeep)));
    doc.reset();
    SUCCEED();
}

TEST(XLinqTeardownTests, DeepChainInAutomaticStorage_ReleasesWithoutExhaustingTheStack) {
    {
        XElement root(XName("n0"));
        XElement* cur = &root;
        for (int i = 1; i < kDeep; ++i) {
            auto next = std::make_shared<XElement>(XName("n"));
            cur->Add(std::shared_ptr<XNode>(next));
            cur = next.get();
        }
    }
    SUCCEED();
}

TEST(XLinqTeardownTests, DeepChainReleasedByRemoveNodes_DoesNotExhaustTheStack) {
    auto root = deepElementChain(kDeep);
    root->RemoveNodes(); // drops the whole subtree through the child's destructor, not the root's
    EXPECT_EQ(root->Nodes().size(), 0u);
    SUCCEED();
}

// --- Shapes besides a chain ---------------------------------------------------------------

TEST(XLinqTeardownTests, EmptyContainersDestroyCleanly) {
    { XElement e(XName("e")); }
    { XDocument d; }
    { auto e = std::make_shared<XElement>(XName("e")); }
    { auto d = std::make_shared<XDocument>(); }
    SUCCEED();
}

TEST(XLinqTeardownTests, SingletonContainerDestroysCleanly) {
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(text("only"));
    e.reset();
    SUCCEED();
}

TEST(XLinqTeardownTests, WideContainerDestroysEveryChild) {
    auto e = std::make_shared<XElement>(XName("e"));
    for (int i = 0; i < 10000; ++i) e->Add(text("v"));
    EXPECT_EQ(e->Nodes().size(), 10000u);
    e.reset();
    SUCCEED();
}

TEST(XLinqTeardownTests, WideTreeOfWideContainersDestroysCleanly) {
    auto root = std::make_shared<XElement>(XName("root"));
    for (int i = 0; i < 200; ++i) {
        auto branch = std::make_shared<XElement>(XName("b"));
        for (int j = 0; j < 200; ++j) branch->Add(text("v"));
        root->Add(std::shared_ptr<XNode>(branch));
    }
    root.reset();
    SUCCEED();
}

TEST(XLinqTeardownTests, MixedNodeKindsDestroyCleanly) {
    auto root = std::make_shared<XElement>(XName("root"));
    XElement* cur = root.get();
    for (int i = 0; i < 4096; ++i) {
        cur->Add(text("t"));
        cur->Add(std::shared_ptr<XNode>(std::make_shared<XComment>("c")));
        auto next = std::make_shared<XElement>(XName("e"));
        next->Add(std::make_shared<XAttribute>(XName("a"), "1"));
        cur->Add(std::shared_ptr<XNode>(next));
        cur = next.get();
    }
    root.reset();
    SUCCEED();
}

// An attribute owns no children, so the attribute side never recursed and is left alone.
TEST(XLinqTeardownTests, ManyAttributesDestroyCleanlyAndAreNotPartOfTheWorklist) {
    auto e = std::make_shared<XElement>(XName("e"));
    for (int i = 0; i < 5000; ++i)
        e->Add(std::make_shared<XAttribute>(XName("a" + std::to_string(i)), "v"));
    EXPECT_EQ(e->getAttributesProperty().size(), 5000u);
    e.reset();
    SUCCEED();
}

// --- The invariants #1890 established must survive the new teardown -----------------------

TEST(XLinqTeardownTests, RetainedNode_OfADeepTree_IsDetachedAndReAddable) {
    std::shared_ptr<XNode> retained;
    {
        auto root = deepElementChain(64);
        XElement* cur = root.get();
        for (int i = 1; i < 32; ++i) cur = static_cast<XElement*>(cur->Nodes()[0].get());
        retained = cur->Nodes()[0];
        ASSERT_NE(retained, nullptr);
        EXPECT_EQ(retained->getParentProperty(), cur);
    }
    EXPECT_EQ(retained->getParentProperty(), nullptr);
    EXPECT_EQ(retained->getDocumentProperty(), nullptr);

    auto newOwner = std::make_shared<XElement>(XName("new"));
    newOwner->Add(retained);
    EXPECT_EQ(retained->getParentProperty(), newOwner.get());
}

TEST(XLinqTeardownTests, RetainedSubtree_SurvivesTheTeardownIntact) {
    std::shared_ptr<XNode> retained;
    {
        auto root = std::make_shared<XElement>(XName("root"));
        auto mid = std::make_shared<XElement>(XName("mid"));
        auto leaf = std::make_shared<XElement>(XName("leaf"));
        leaf->Add(text("deep"));
        mid->Add(std::shared_ptr<XNode>(leaf));
        root->Add(std::shared_ptr<XNode>(mid));
        retained = mid;
    }
    EXPECT_EQ(retained->getParentProperty(), nullptr);
    EXPECT_EQ(retained->ToString(), "<mid>\n  <leaf>deep</leaf>\n</mid>");
}

TEST(XLinqTeardownTests, RetainedAttribute_OfADestroyedDeepTree_IsDetached) {
    std::shared_ptr<XAttribute> attr;
    {
        auto root = deepElementChain(1024);
        XElement* cur = static_cast<XElement*>(root->Nodes()[0].get());
        attr = std::make_shared<XAttribute>(XName("a"), "1");
        cur->Add(attr);
        EXPECT_EQ(attr->getParentProperty(), cur);
    }
    EXPECT_EQ(attr->getParentProperty(), nullptr);
    EXPECT_EQ(attr->getNextAttributeProperty(), nullptr);
}

TEST(XLinqTeardownTests, DeepTreeDestroyedDuringExceptionUnwinding_DoesNotTerminate) {
    bool caught = false;
    try {
        auto root = deepElementChain(4096);
        throw std::runtime_error("unwind");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(XLinqTeardownTests, DestructorsRemainNonThrowing) {
    static_assert(std::is_nothrow_destructible_v<XContainer>);
    static_assert(std::is_nothrow_destructible_v<XElement>);
    static_assert(std::is_nothrow_destructible_v<XDocument>);
    static_assert(std::is_nothrow_destructible_v<XAttribute>);
    SUCCEED();
}

// --- Destruction order is preserved -------------------------------------------------------

TEST(XLinqTeardownTests, DestructionOrderIsDepthFirstDocumentOrder) {
    std::vector<int> order;
    g_order = &order;
    {
        auto root = std::make_shared<RecordingElement>(XName("root"), 0);
        auto a = std::make_shared<RecordingElement>(XName("a"), 1);
        auto a1 = std::make_shared<RecordingElement>(XName("a1"), 2);
        auto a2 = std::make_shared<RecordingElement>(XName("a2"), 3);
        auto b = std::make_shared<RecordingElement>(XName("b"), 4);
        auto b1 = std::make_shared<RecordingElement>(XName("b1"), 5);
        a->Add(std::shared_ptr<XNode>(a1));
        a->Add(std::shared_ptr<XNode>(a2));
        b->Add(std::shared_ptr<XNode>(b1));
        root->Add(std::shared_ptr<XNode>(a));
        root->Add(std::shared_ptr<XNode>(b));
    }
    g_order = nullptr;
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4, 5}));
}

// --- Content is genuinely released --------------------------------------------------------

TEST(XLinqTeardownTests, EveryNodeOfADeepTreeIsActuallyDestroyed) {
    std::vector<std::weak_ptr<XNode>> observers;
    {
        auto root = std::make_shared<XElement>(XName("n0"));
        XElement* cur = root.get();
        for (int i = 1; i < 512; ++i) {
            auto next = std::make_shared<XElement>(XName("n"));
            observers.push_back(std::shared_ptr<XNode>(next));
            cur->Add(std::shared_ptr<XNode>(next));
            cur = next.get();
        }
        for (const auto& w : observers) EXPECT_FALSE(w.expired());
    }
    for (const auto& w : observers) EXPECT_TRUE(w.expired());
}
