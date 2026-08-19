// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for tickets #2198 and #2199 (SR-AUD-336): the XObject Changed/Changing
// contract.
//
// THIS SUITE USED TO PIN AN INERT SURFACE. Until 2026-08-19 `add_Changed`, `remove_Changed`,
// `add_Changing` and `remove_Changing` accepted a handler and DISCARDED it, and no mutation
// anywhere in the hierarchy raised anything; every case below was named `*_RaisesNothing` and
// asserted a total of zero. #2198 wrote them that way deliberately, so that a partial
// implementation could not land silently and so the eventual repair would have failing tests to
// turn green. #2199 is that repair, and this file is those tests turned green.
//
// BOTH OF #2199's GATES WERE OPENED ON 2026-08-19 (docs/StandingApprovals.md SA-13):
//
//   1. LAYOUT. Handlers need storage, and there was no padding: sizeof(XObject) 16 -> 24, and
//      every derived node type with it. That exact growth had been declined on 2026-07-31 for
//      a different purpose (#1896), so it had to be asked again. It was granted, and #1896's
//      was granted with it.
//   2. REMOVAL. XObjectChangeEventHandler is a bare std::function alias and std::function is not
//      equality-comparable, so a handler-taking remove_* cannot identify a registration. This was
//      never a cost question -- it is not implementable as declared, at any layout cost. The
//      decision was a REGISTRATION TOKEN: add_* returns one, remove_* takes it. Two alternatives
//      were declined: removing ALL registrations (silently drops a third party's subscription)
//      and throwing (a subscriber could never unsubscribe).
//
// The static_asserts below are KEPT rather than deleted. They no longer describe a blocker; they
// describe the PREMISE the token design rests on. If std::function ever became equality-
// comparable, the token would stop being necessary and #2199's divergence should be revisited.

#include <gtest/gtest.h>
#include <functional>
#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "System/InvalidOperationException.hpp"
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

    /// Records every notification delivered to one registration point.
    struct Recorder {
        int changed = 0;
        int changing = 0;
        std::vector<XObjectChange> kinds;          ///< kinds seen by the Changed half, in order
        std::vector<XObjectChange> changingKinds;  ///< kinds seen by the Changing half, in order
        std::vector<void*> senders;                ///< Changed senders, in order
        std::vector<void*> changingSenders;         ///< Changing senders, in order
        std::vector<std::string> order;            ///< interleaved trace: "changing"/"changed"

        [[nodiscard]] XObjectChangeEventHandler onChanged() {
            return [this](void* sender, const XObjectChangeEventArgs& e) {
                ++changed;
                kinds.push_back(e.getObjectChangeProperty());
                senders.push_back(sender);
                order.emplace_back("changed");
            };
        }
        [[nodiscard]] XObjectChangeEventHandler onChanging() {
            return [this](void* sender, const XObjectChangeEventArgs& e) {
                ++changing;
                changingKinds.push_back(e.getObjectChangeProperty());
                // Recorded because the FIRST CUT of this suite did not: a mutation that moved the
                // sender on the Changing half ALONE went uncaught, since only the Changed half's
                // senders were asserted. Half a pair is still a wrong notification.
                changingSenders.push_back(sender);
                order.emplace_back("changing");
            };
        }
        [[nodiscard]] int total() const { return changed + changing; }

        /// The two halves must always report the SAME kinds and the SAME senders, in the same
        /// order. Asserting only the Changed half let two mutations through -- one that moved the
        /// sender and one that changed the KIND, each on the Changing call alone. Half a pair is
        /// still a wrong notification, so every case checks this.
        void ExpectBothHalvesAgree(const char* what) const {
            EXPECT_EQ(changingKinds, kinds) << "Changing/Changed kinds disagree: " << what;
            EXPECT_EQ(changingSenders, senders) << "Changing/Changed senders disagree: " << what;
        }
    };

    /// Builds a `depth`-deep XElement chain and returns how long it took.
    ///
    /// #1896 -- and the reason this returns a DURATION rather than just building: a mutation that
    /// reintroduces the quadratic walk still BUILDS the tree correctly, so an assertion on the
    /// result cannot see it. It would be caught only as "the suite got slower", which is not
    /// caught by name. The bound below is deliberately three orders of magnitude above the linear
    /// cost, so it discriminates without being a benchmark that flakes under gate load.
    [[nodiscard]] double buildChainSeconds(int depth) {
        const auto t0 = std::chrono::steady_clock::now();
        auto root = std::make_shared<XElement>(XName("r"));
        XElement* cur = root.get();
        for (int i = 0; i < depth; ++i) {
            auto next = std::make_shared<XElement>(XName("e"));
            XElement* raw = next.get();
            cur->Add(std::static_pointer_cast<XNode>(next));
            cur = raw;
        }
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }

    /// One subject's two registrations, kept so they can be handed back to remove_*.
    struct Subscription {
        System::Xml::Linq::XObjectChangeRegistration changed;
        System::Xml::Linq::XObjectChangeRegistration changing;
    };

    /// Registers both halves on `subject` and returns the tokens.
    template <class T>
    [[nodiscard]] Subscription listen(Recorder& r, T& subject) {
        return Subscription{subject.add_Changed(r.onChanged()),
                            subject.add_Changing(r.onChanging())};
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

// --- Every door #2198 measured as inert, now measured live -------------------------------------
//
// Each case below replaces one `*_RaisesNothing` case. The subject, the registration and the
// mutation are unchanged; only the expectation is inverted -- which is exactly what #2198 wrote
// them for.

TEST(XLinqChangeNotificationTests, Fix2199_SetValueComposesRemovesAndAnAddRatherThanRaisingValue) {
    // NOT a Value pair, and that is .NET's behaviour rather than an omission: XElement's Value
    // setter is `RemoveNodes(); Add(value);`, so a subscriber sees a Remove pair per existing
    // child and then an Add pair. Raising Value here would invent an event .NET does not raise.
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    e->Add(std::make_shared<XText>("old"));
    const auto sub = listen(r, *e);
    (void)sub;
    e->setValueProperty("new");

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Remove, XObjectChange::Add}));

    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.changing, r.changed);
    EXPECT_EQ(e->getValueProperty(), "new");
}

TEST(XLinqChangeNotificationTests, Fix2199_SetNameRaisesAName) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    const auto sub = listen(r, *e);
    (void)sub;
    e->setNameProperty(XName("renamed"));

    EXPECT_EQ(r.changing, 1);
    EXPECT_EQ(r.changed, 1);
    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Name}));
    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{e.get()})) << "sender is the renamed element itself";
}

TEST(XLinqChangeNotificationTests, Fix2199_AddNodeRaisesAnAddWithTheChildAsSender) {
    Recorder r;
    auto parent = std::make_shared<XElement>(XName("p"));
    const auto sub = listen(r, *parent);
    (void)sub;
    auto child = std::make_shared<XElement>(XName("c"));
    parent->Add(child);

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Add}));

    r.ExpectBothHalvesAgree("r");
    // .NET's asymmetry, transcribed: the chain walked is the PARENT's, the sender is the CHILD
    // (XLinq.cs:156,177). A port that passed the parent would satisfy a bare count.
    EXPECT_EQ(r.senders, (std::vector<void*>{child.get()}));
    EXPECT_EQ(r.changingSenders, (std::vector<void*>{child.get()}))
        << "BOTH halves name the child -- asserting only the Changed half misses a mutation "
           "that moves the sender on the Changing one";
}

TEST(XLinqChangeNotificationTests, Fix2199_AddFirstRaisesAnAdd) {
    Recorder r;
    auto parent = std::make_shared<XElement>(XName("p"));
    parent->Add(std::make_shared<XElement>(XName("existing")));
    const auto sub = listen(r, *parent);
    (void)sub;
    auto child = std::make_shared<XElement>(XName("c"));
    parent->AddFirst(child);

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Add}));

    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{child.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_AddTextRaisesAnAddOrAValueDependingOnMerging) {
    // Add(string) merges into a trailing XText rather than creating a sibling (XContainer.cs's
    // AddString). So the SAME call raises Add on an empty element and Value on one that already
    // ends in text -- a distinction a test asserting only "something fired" would miss.
    {
        Recorder r;
        auto e = std::make_shared<XElement>(XName("e"));
        const auto sub = listen(r, *e);
        (void)sub;
        e->Add(std::string("hello"));
        EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Add}))
            << "no trailing text node, so a new XText is added";
    }
    {
        Recorder r;
        auto e = std::make_shared<XElement>(XName("e"));
        e->Add(std::make_shared<XText>("hello"));
        const auto sub = listen(r, *e);
        (void)sub;
        e->Add(std::string(" world"));
        EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Value}))
            << "merged into the existing text node, so its Value changed";
        EXPECT_EQ(e->getValueProperty(), "hello world");
    }
    {
        Recorder r;
        auto e = std::make_shared<XElement>(XName("e"));
        const auto sub = listen(r, *e);
        (void)sub;
        e->Add(std::string(""));
        EXPECT_EQ(r.total(), 0) << "an empty string is a genuine no-op, so nothing is raised";
    }
}

TEST(XLinqChangeNotificationTests, Fix2199_AddAttributeRaisesAnAddWithTheAttributeAsSender) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    const auto sub = listen(r, *e);
    (void)sub;
    auto attr = std::make_shared<XAttribute>(XName("a"), "1");
    e->Add(attr);

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Add}));

    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{attr.get()}));
    EXPECT_EQ(r.changingSenders, (std::vector<void*>{attr.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_RemoveNodesRaisesOneRemovePerChild) {
    // PER CHILD, not one bulk event -- .NET's RemoveNodes loops and notifies for each node
    // (XContainer.cs:400-416). A handler counting removals must see the child count.
    Recorder r;
    auto parent = std::make_shared<XElement>(XName("p"));
    auto a = std::make_shared<XElement>(XName("a"));
    auto b = std::make_shared<XElement>(XName("b"));
    auto c = std::make_shared<XElement>(XName("c"));
    parent->Add(a); parent->Add(b); parent->Add(c);
    const auto sub = listen(r, *parent);
    (void)sub;
    parent->RemoveNodes();

    EXPECT_EQ(r.changed, 3);
    EXPECT_EQ(r.changing, 3);
    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Remove, XObjectChange::Remove,
                                                  XObjectChange::Remove}));
    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{a.get(), b.get(), c.get()}))
        << "in document order, each child named as its own removal's sender";
    EXPECT_EQ(r.changingSenders, (std::vector<void*>{a.get(), b.get(), c.get()}));
    EXPECT_TRUE(parent->Nodes().empty());
}

TEST(XLinqChangeNotificationTests, Fix2199_RemoveAttributesRaisesOneRemovePerAttribute) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    auto a1 = std::make_shared<XAttribute>(XName("a"), "1");
    auto a2 = std::make_shared<XAttribute>(XName("b"), "2");
    e->Add(a1); e->Add(a2);
    const auto sub = listen(r, *e);
    (void)sub;
    e->RemoveAttributes();

    EXPECT_EQ(r.changed, 2);
    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Remove, XObjectChange::Remove}));
    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{a1.get(), a2.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_RemoveAllRaisesAttributesThenNodes) {
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    auto attr = std::make_shared<XAttribute>(XName("a"), "1");
    auto child = std::make_shared<XElement>(XName("c"));
    e->Add(attr); e->Add(child);
    const auto sub = listen(r, *e);
    (void)sub;
    e->RemoveAll();

    // The ORDER is the observable that a count cannot express: RemoveAll is
    // RemoveAttributes() then RemoveNodes(), so the attribute goes first.
    EXPECT_EQ(r.senders, (std::vector<void*>{attr.get(), child.get()}));
    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Remove, XObjectChange::Remove}));
    r.ExpectBothHalvesAgree("r");
}

// --- Doors on the node being mutated, not on its container ------------------------------------

TEST(XLinqChangeNotificationTests, Fix2199_NodeRemoveRaisesOnTheParentChainNotTheDetachedNode) {
    // The node's OWN registration sees nothing, because the walk starts at the container and the
    // node is not its own ancestor. The parent's sees the Remove. Getting this backwards is the
    // easy mistake, so both are asserted.
    Recorder onNode;
    Recorder onParent;
    auto parent = std::make_shared<XElement>(XName("p"));
    auto child = std::make_shared<XElement>(XName("c"));
    parent->Add(child);
    const auto s1 = listen(onNode, *child);
    const auto s2 = listen(onParent, *parent);
    (void)s1; (void)s2;
    child->Remove();

    EXPECT_EQ(child->getParentProperty(), nullptr);
    EXPECT_EQ(onParent.kinds, (std::vector<XObjectChange>{XObjectChange::Remove}));
    onParent.ExpectBothHalvesAgree("onParent");
    EXPECT_EQ(onParent.senders, (std::vector<void*>{child.get()}));
    EXPECT_EQ(onParent.changingSenders, (std::vector<void*>{child.get()}));
    EXPECT_EQ(onNode.total(), 0)
        << "the walk starts at the container; a node is not its own ancestor";
}

TEST(XLinqChangeNotificationTests, Fix2199_NodeReplaceWithRaisesARemoveThenAnAdd) {
    Recorder onParent;
    auto parent = std::make_shared<XElement>(XName("p"));
    auto oldChild = std::make_shared<XElement>(XName("old"));
    auto newChild = std::make_shared<XElement>(XName("new"));
    parent->Add(oldChild);
    const auto sub = listen(onParent, *parent);
    (void)sub;
    oldChild->ReplaceWith(newChild);

    EXPECT_EQ(parent->Nodes().size(), 1u);
    EXPECT_EQ(onParent.kinds, (std::vector<XObjectChange>{XObjectChange::Remove, XObjectChange::Add}));
    onParent.ExpectBothHalvesAgree("onParent");
    EXPECT_EQ(onParent.senders, (std::vector<void*>{oldChild.get(), newChild.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_AttributeSetValueRaisesAValueOnTheAttribute) {
    Recorder onAttr;
    Recorder onElement;
    auto e = std::make_shared<XElement>(XName("e"));
    auto attr = std::make_shared<XAttribute>(XName("a"), "1");
    e->Add(attr);
    const auto s1 = listen(onAttr, *attr);
    const auto s2 = listen(onElement, *e);
    (void)s1; (void)s2;
    attr->setValueProperty("2");

    EXPECT_EQ(onAttr.kinds, (std::vector<XObjectChange>{XObjectChange::Value}));

    onAttr.ExpectBothHalvesAgree("onAttr");
    EXPECT_EQ(onAttr.senders, (std::vector<void*>{attr.get()}));
    // And it BUBBLES to the owning element, which is the whole point of the ancestor walk.
    EXPECT_EQ(onElement.kinds, (std::vector<XObjectChange>{XObjectChange::Value}));
    onElement.ExpectBothHalvesAgree("onElement");
}

TEST(XLinqChangeNotificationTests, Fix2199_AttributeRemoveRaisesARemove) {
    Recorder onElement;
    auto e = std::make_shared<XElement>(XName("e"));
    auto attr = std::make_shared<XAttribute>(XName("a"), "1");
    e->Add(attr);
    const auto sub = listen(onElement, *e);
    (void)sub;
    attr->Remove();

    EXPECT_EQ(onElement.kinds, (std::vector<XObjectChange>{XObjectChange::Remove}));

    onElement.ExpectBothHalvesAgree("onElement");
    EXPECT_EQ(onElement.senders, (std::vector<void*>{attr.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_TextSetValueRaisesAValue) {
    Recorder r;
    auto t = std::make_shared<XText>("old");
    const auto sub = listen(r, *t);
    (void)sub;
    t->setValueProperty("new");

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Value}));

    r.ExpectBothHalvesAgree("r");
    EXPECT_EQ(r.senders, (std::vector<void*>{t.get()}));
}

TEST(XLinqChangeNotificationTests, Fix2199_CommentSetValueRaisesAValue) {
    Recorder r;
    auto c = std::make_shared<XComment>("old");
    const auto sub = listen(r, *c);
    (void)sub;
    c->setValueProperty("new");

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Value}));

    r.ExpectBothHalvesAgree("r");
}

TEST(XLinqChangeNotificationTests, Fix2199_CDataInheritsTheNotifyingTextSetter) {
    // XCData derives from XText and does not override setValueProperty, so it must notify too.
    // Pinned because an override added later would silently go quiet.
    Recorder r;
    auto cd = std::make_shared<XCData>("old");
    const auto sub = listen(r, *cd);
    (void)sub;
    cd->setValueProperty("new");

    EXPECT_EQ(r.kinds, (std::vector<XObjectChange>{XObjectChange::Value}));

    r.ExpectBothHalvesAgree("r");
}

// --- The ancestor walk ------------------------------------------------------------------------

TEST(XLinqChangeNotificationTests, Fix2199_AnAncestorObservesADescendantChange) {
    Recorder onRoot;
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(mid);
    mid->Add(leaf);
    const auto sub = listen(onRoot, *root);
    (void)sub;
    leaf->setNameProperty(XName("renamed"));

    EXPECT_EQ(onRoot.kinds, (std::vector<XObjectChange>{XObjectChange::Name}));

    onRoot.ExpectBothHalvesAgree("onRoot");
    EXPECT_EQ(onRoot.senders, (std::vector<void*>{leaf.get()}))
        << "the sender is the object CHANGED, not the object observed";
}

TEST(XLinqChangeNotificationTests, Fix2199_EveryAncestorOnTheChainIsNotifiedInnermostFirst) {
    // .NET walks the whole chain (XObject.cs:418-438), not just the nearest registration. Both
    // observers must fire, and a shared trace pins the ORDER, which a per-recorder count cannot.
    std::vector<std::string> trace;
    auto root = std::make_shared<XElement>(XName("root"));
    auto mid = std::make_shared<XElement>(XName("mid"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(mid);
    mid->Add(leaf);

    const auto rootTok = root->add_Changed(
        [&](void*, const XObjectChangeEventArgs&) { trace.emplace_back("root"); });
    const auto midTok = mid->add_Changed(
        [&](void*, const XObjectChangeEventArgs&) { trace.emplace_back("mid"); });
    const auto leafTok = leaf->add_Changed(
        [&](void*, const XObjectChangeEventArgs&) { trace.emplace_back("leaf"); });
    (void)rootTok; (void)midTok; (void)leafTok;

    leaf->setNameProperty(XName("renamed"));
    EXPECT_EQ(trace, (std::vector<std::string>{"leaf", "mid", "root"}));
}

TEST(XLinqChangeNotificationTests, Fix2199_ADocumentObservesAChangeInItsTree) {
    Recorder onDoc;
    auto doc = std::make_shared<XDocument>();
    auto root = std::make_shared<XElement>(XName("root"));
    doc->Add(root);
    const auto sub = listen(onDoc, *doc);
    (void)sub;
    root->setNameProperty(XName("renamed"));

    EXPECT_EQ(onDoc.kinds, (std::vector<XObjectChange>{XObjectChange::Name}));

    onDoc.ExpectBothHalvesAgree("onDoc");
}

// --- Registration and removal -----------------------------------------------------------------

TEST(XLinqChangeNotificationTests, Fix2199_ManyRegistrationsAllFireInRegistrationOrder) {
    auto e = std::make_shared<XElement>(XName("e"));
    std::vector<int> fired;
    std::vector<System::Xml::Linq::XObjectChangeRegistration> tokens;
    for (int i = 0; i < 5; ++i) {
        tokens.push_back(e->add_Changed(
            [&fired, i](void*, const XObjectChangeEventArgs&) { fired.push_back(i); }));
    }
    e->setNameProperty(XName("renamed"));
    EXPECT_EQ(fired, (std::vector<int>{0, 1, 2, 3, 4}));

    // Every token is distinct, which is what makes them usable for removal.
    for (size_t i = 0; i < tokens.size(); ++i) {
        for (size_t j = i + 1; j < tokens.size(); ++j) EXPECT_FALSE(tokens[i] == tokens[j]);
    }
}

TEST(XLinqChangeNotificationTests, Fix2199_ARegistrationCanBeRemovedByItsToken) {
    // THIS IS WHAT THE DIVERGENCE BUYS. Under the old inert surface removal was impossible to
    // express at all; under .NET's handler-taking signature it is impossible to implement in C++.
    Recorder r;
    auto e = std::make_shared<XElement>(XName("e"));
    const auto sub = listen(r, *e);

    e->setNameProperty(XName("one"));
    EXPECT_EQ(r.changed, 1);

    EXPECT_TRUE(e->remove_Changed(sub.changed));
    e->setNameProperty(XName("two"));
    EXPECT_EQ(r.changed, 1) << "the Changed registration is gone";
    EXPECT_EQ(r.changing, 2) << "and the Changing one is untouched -- removal is per registration";

    EXPECT_TRUE(e->remove_Changing(sub.changing));
    e->setNameProperty(XName("three"));
    EXPECT_EQ(r.total(), 3) << "nothing fires once both are removed";
}

TEST(XLinqChangeNotificationTests, Fix2199_RemovingATokenTwiceOrAForeignOneReturnsFalse) {
    Recorder r;
    auto a = std::make_shared<XElement>(XName("a"));
    auto b = std::make_shared<XElement>(XName("b"));
    const auto onA = listen(r, *a);
    const auto onB = listen(r, *b);

    // THE FOREIGN-TOKEN CASE MUST COME FIRST, AND THE FIRST CUT OF THIS TEST GOT IT WRONG.
    // It removed A's registration before trying B's token, which left A's list EMPTY -- so the
    // assertion passed for the wrong reason and a per-object id counter went undetected. A's
    // registration must still be PRESENT for "does B's token match it?" to be a real question.
    EXPECT_FALSE(a->remove_Changed(onB.changed))
        << "ids are process-wide, so B's first token must not match A's first registration";
    EXPECT_FALSE(b->remove_Changed(onA.changed)) << "and symmetrically";
    EXPECT_FALSE(onA.changed == onB.changed)
        << "the tokens themselves are distinct across objects, which is the property that "
           "makes the check above meaningful";

    // A's registration survived both foreign attempts and still fires.
    a->setNameProperty(XName("renamed"));
    EXPECT_EQ(r.changed, 1);

    // Only now the double-removal case.
    EXPECT_TRUE(a->remove_Changed(onA.changed));
    EXPECT_FALSE(a->remove_Changed(onA.changed)) << "already removed";

    // A default-constructed token names nothing.
    EXPECT_FALSE(a->remove_Changed(System::Xml::Linq::XObjectChangeRegistration{}));
    EXPECT_TRUE(System::Xml::Linq::XObjectChangeRegistration{}.IsEmpty());
    EXPECT_FALSE(onA.changing.IsEmpty());

    // Removing from an object that never had any registration at all is false, not a crash.
    auto never = std::make_shared<XElement>(XName("never"));
    EXPECT_FALSE(never->remove_Changed(onA.changing));
}

TEST(XLinqChangeNotificationTests, Fix2199_TokensAreUniqueAcrossObjectsAndAcrossThreads) {
    // The ids come from a PROCESS-WIDE atomic counter, not a per-object or per-thread one. A
    // per-object counter makes A's first token match B's first registration; a thread_local one
    // makes thread 1's first match thread 2's. Both are silent cross-object removals, so both are
    // asserted -- the single-threaded half above cannot see the thread_local variant.
    constexpr int kPerThread = 32;
    std::vector<std::shared_ptr<XElement>> owners;
    std::vector<System::Xml::Linq::XObjectChangeRegistration> t1, t2;
    for (int i = 0; i < 2 * kPerThread; ++i) owners.push_back(std::make_shared<XElement>(XName("e")));

    // Two threads, each registering on ITS OWN objects, so nothing is shared but the counter.
    auto work = [&](int base, std::vector<System::Xml::Linq::XObjectChangeRegistration>& out) {
        for (int i = 0; i < kPerThread; ++i) {
            out.push_back(owners[static_cast<size_t>(base + i)]->add_Changed(
                [](void*, const XObjectChangeEventArgs&) {}));
        }
    };
    std::thread a(work, 0, std::ref(t1));
    std::thread b(work, kPerThread, std::ref(t2));
    a.join();
    b.join();

    std::vector<System::Xml::Linq::XObjectChangeRegistration> all;
    all.insert(all.end(), t1.begin(), t1.end());
    all.insert(all.end(), t2.begin(), t2.end());
    ASSERT_EQ(all.size(), static_cast<size_t>(2 * kPerThread));
    for (size_t i = 0; i < all.size(); ++i) {
        EXPECT_FALSE(all[i].IsEmpty());
        for (size_t j = i + 1; j < all.size(); ++j) {
            EXPECT_FALSE(all[i] == all[j])
                << "tokens " << i << " and " << j << " collided -- the counter is not process-wide";
        }
    }
}

TEST(XLinqChangeNotificationTests, Fix2199_AnEmptyHandlerIsAcceptedAndStillCountsAsARegistration) {
    // .NET's annotation exists whether or not either delegate is null, and `notify` is derived
    // from the ANNOTATION rather than the delegate -- so an empty handler still makes an object a
    // notification point for its descendants. Reproduced deliberately.
    auto root = std::make_shared<XElement>(XName("root"));
    auto leaf = std::make_shared<XElement>(XName("leaf"));
    root->Add(leaf);

    const auto emptyTok = root->add_Changed(XObjectChangeEventHandler{});
    EXPECT_FALSE(emptyTok.IsEmpty());
    EXPECT_NO_THROW(leaf->setNameProperty(XName("renamed")));
    EXPECT_TRUE(root->remove_Changed(emptyTok));
}

TEST(XLinqChangeNotificationTests, Fix2199_AChangedOnlySubscriptionStillReceivesChanged) {
    // THE SUBTLE ONE. .NET's NotifyChanging returns "does any object on the chain carry
    // registrations", NOT "did a changing handler run" -- it tests the annotation and only then
    // invokes the possibly-null delegate. Every call site guards NotifyChanged on that value, so
    // reading it as "a changing handler ran" would silently disable EVERY Changed-only
    // subscription. This case is the one that catches that.
    int changed = 0;
    auto e = std::make_shared<XElement>(XName("e"));
    const auto tok = e->add_Changed([&](void*, const XObjectChangeEventArgs&) { ++changed; });
    (void)tok;
    e->setNameProperty(XName("renamed"));
    EXPECT_EQ(changed, 1);
}

TEST(XLinqChangeNotificationTests, Fix2199_ChangingRunsBeforeTheMutationAndChangedAfter) {
    // Asserted on OBSERVED STATE, not on ordering of counters: a handler that reads the value it
    // is being notified about must see the old one in Changing and the new one in Changed.
    auto e = std::make_shared<XElement>(XName("before"));
    std::string seenWhileChanging, seenWhenChanged;
    const auto t1 = e->add_Changing([&](void*, const XObjectChangeEventArgs&) {
        seenWhileChanging = e->getNameProperty().getLocalNameProperty();
    });
    const auto t2 = e->add_Changed([&](void*, const XObjectChangeEventArgs&) {
        seenWhenChanged = e->getNameProperty().getLocalNameProperty();
    });
    (void)t1; (void)t2;

    e->setNameProperty(XName("after"));
    EXPECT_EQ(seenWhileChanging, "before");
    EXPECT_EQ(seenWhenChanged, "after");
}

TEST(XLinqChangeNotificationTests, Fix2199_AHandlerMayRegisterOrUnregisterDuringANotification) {
    // Handlers are invoked from a SNAPSHOT, so mutating the registration list from inside a
    // handler neither invalidates the walk nor changes the pass in flight.
    //
    // THE FIRST CUT OF THIS TEST DID NOT DISCRIMINATE. It removed the running handler as well as
    // adding one, which ended the live-vector iteration early and hid the difference. It now only
    // ADDS -- and adds enough to force a reallocation, so iterating the live vector would both
    // invoke the new handlers in the same pass and dangle its own iterators.
    auto e = std::make_shared<XElement>(XName("e"));
    int outer = 0, added = 0;
    const auto outerTok = e->add_Changed([&](void*, const XObjectChangeEventArgs&) {
        ++outer;
        for (int i = 0; i < 64; ++i) {
            (void)e->add_Changed([&](void*, const XObjectChangeEventArgs&) { ++added; });
        }
    });
    (void)outerTok;

    EXPECT_NO_THROW(e->setNameProperty(XName("one")));
    EXPECT_EQ(outer, 1);
    EXPECT_EQ(added, 0)
        << "registrations made during a notification take effect on the NEXT one; a live-vector "
           "walk would have invoked them in this pass";

    // They do take effect next time -- so the additions really happened and the case is not
    // passing because registration silently failed.
    added = 0;
    e->setNameProperty(XName("two"));
    EXPECT_EQ(added, 64);

    // And removal from inside a handler is equally safe.
    auto f = std::make_shared<XElement>(XName("f"));
    int ran = 0;
    System::Xml::Linq::XObjectChangeRegistration self;
    self = f->add_Changed([&](void*, const XObjectChangeEventArgs&) {
        ++ran;
        f->remove_Changed(self);
    });
    EXPECT_NO_THROW(f->setNameProperty(XName("one")));
    f->setNameProperty(XName("two"));
    EXPECT_EQ(ran, 1) << "the self-removal took effect";
}

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

TEST(XLinqChangeNotificationTests, Fix2199_TheLayoutGrowthApprovalWasPaidExactlyOnce) {
    // INVERTED BY #2199. Its predecessor asserted `sizeof(XObject) == 2 * sizeof(void*)` and said
    // "if any of these change, the approval sentence is stale and must be re-derived" -- the
    // approval was granted on 2026-08-19, so this now records what it BOUGHT.
    //
    // One pointer: the unique_ptr to the registration block, allocated only on first registration
    // so an unobserved tree pays a null pointer and no allocation. That is the closest analogue of
    // .NET's annotation slot, which is likewise absent until something is annotated.
    EXPECT_EQ(sizeof(System::Xml::Linq::XObject), 3 * sizeof(void*));
    EXPECT_EQ(sizeof(XNode), 3 * sizeof(void*));
    EXPECT_EQ(alignof(System::Xml::Linq::XObject), alignof(void*));

    // The literal figures, measured before and after in build-probe, so the whole approved
    // chain is on the record and a second growth cannot hide inside a relative assertion.
    static_assert(sizeof(System::Xml::Linq::XObject) == 24);          // was 16
    static_assert(sizeof(XNode) == 24);                               // was 16
    static_assert(sizeof(XElement) == 136);                           // was 128
    static_assert(sizeof(XAttribute) == 128);                         // was 120
    static_assert(sizeof(XText) == 56);                               // was 48
    static_assert(sizeof(XComment) == 56);                            // was 48
    static_assert(sizeof(XCData) == 56);                              // was 48
    static_assert(sizeof(XDocument) == 64);                           // was 56
}

TEST(XLinqChangeNotificationTests, Decl2199_AnUnobservedTreePaysNoAllocation) {
    // The half the sizeof pin cannot express, and the reason the growth is one pointer rather
    // than an inline vector pair: a tree nobody listens to allocates nothing for notification.
    // Asserted through behaviour -- building and mutating a tree with no registration anywhere
    // must raise nothing and must not throw.
    auto root = std::make_shared<XElement>(XName("root"));
    for (int i = 0; i < 32; ++i) root->Add(std::make_shared<XElement>(XName("c")));
    EXPECT_NO_THROW(root->setNameProperty(XName("renamed")));
    EXPECT_NO_THROW(root->RemoveNodes());
    EXPECT_TRUE(root->Nodes().empty());
}

// ---------------------------------------------------------------------------------------------
// #1896 -- the Xml.Linq half, which #2199 made visible.
// ---------------------------------------------------------------------------------------------

TEST(XLinqChangeNotificationTests, Fix1896_DeepConstructionIsLinearWhenNobodyIsListening) {
    // TWO quadratic sources were in this path, and only the first was #1896's stated one.
    //
    //   1. XContainer::InsertNodeAt's self-insertion guard walked the ancestor chain on EVERY
    //      insert. Fixed by the same short-circuit as JsonNode's: the inserted node can only be
    //      an ancestor of `this` if it IS a container AND has children.
    //   2. #2199's OWN ancestor walk, added earlier the same day. NotifyChanging visits every
    //      ancestor even when nothing is subscribed -- and .NET has exactly that shape
    //      (XObject.cs:424-427 skips annotation-less ancestors cheaply but still visits each).
    //
    // Measured, same binary and flags (build-probe/1896_probe1_*.log):
    //     depth  20,000     4.668s -> 3.451s (guard only) -> 0.015s (both)
    //     depth 100,000   120.028s -> 89.332s              -> 0.051s
    // The middle column is why the second source had to be found: fixing the guard alone left
    // the path quadratic, and a test asserting only "faster" would have called that a success.
    const double elapsed = buildChainSeconds(40000);
    EXPECT_LT(elapsed, 2.0)
        << "#1896: 40,000 levels took ~14s quadratic and ~0.01s linear. Took " << elapsed
        << "s -- one of the two short-circuits has stopped working.";
}

TEST(XLinqChangeNotificationTests, Fix1896_TheSelfInsertionGuardStillRejectsEverythingItDid) {
    // The guard must be FASTER, NOT WEAKER. Every rejection path, including the one the
    // short-circuit could plausibly have broken.
    {   // Self-insertion into an EMPTY container -- THE TRAP. An empty container skips the
        // ancestor walk, so the `n == this` test must sit OUTSIDE that guard.
        auto e = std::make_shared<XElement>(XName("e"));
        EXPECT_THROW(e->Add(std::static_pointer_cast<XNode>(e)), System::InvalidOperationException);
    }
    {   // Self-insertion into a NON-empty container -- takes the walk.
        auto e = std::make_shared<XElement>(XName("e"));
        e->Add(std::make_shared<XElement>(XName("child")));
        EXPECT_THROW(e->Add(std::static_pointer_cast<XNode>(e)), System::InvalidOperationException);
    }
    {   // Inserting an ancestor into its own descendant.
        auto root = std::make_shared<XElement>(XName("root"));
        auto mid = std::make_shared<XElement>(XName("mid"));
        auto leaf = std::make_shared<XElement>(XName("leaf"));
        root->Add(std::static_pointer_cast<XNode>(mid));
        mid->Add(std::static_pointer_cast<XNode>(leaf));
        EXPECT_THROW(leaf->Add(std::static_pointer_cast<XNode>(root)),
                     System::InvalidOperationException);
    }
    {   // Deeper, so the short-circuit cannot be passing by depth luck.
        auto root = std::make_shared<XElement>(XName("root"));
        XElement* cur = root.get();
        std::shared_ptr<XElement> last;
        for (int i = 0; i < 64; ++i) {
            auto next = std::make_shared<XElement>(XName("e"));
            last = next;
            cur->Add(std::static_pointer_cast<XNode>(next));
            cur = next.get();
        }
        EXPECT_THROW(last->Add(std::static_pointer_cast<XNode>(root)),
                     System::InvalidOperationException);
    }
    {   // A NON-container node is never an ancestor, so it is inserted without a walk -- and must
        // still be inserted correctly rather than refused.
        auto e = std::make_shared<XElement>(XName("e"));
        EXPECT_NO_THROW(e->Add(std::static_pointer_cast<XNode>(std::make_shared<XText>("t"))));
        EXPECT_EQ(e->Nodes().size(), 1u);
    }
}

TEST(XLinqChangeNotificationTests, Fix1896_TheShortCircuitIsExactNotApproximate) {
    // The notification short-circuit skips the ancestor walk only when NO registration exists
    // anywhere in the process. That is exact -- if none exists, no walk could have found one --
    // but it is worth asserting from the other side: with a registration live, an unrelated tree's
    // mutations must still notify normally.
    auto observedRoot = std::make_shared<XElement>(XName("observed"));
    auto observedLeaf = std::make_shared<XElement>(XName("leaf"));
    observedRoot->Add(std::static_pointer_cast<XNode>(observedLeaf));

    Recorder r;
    const auto sub = listen(r, *observedRoot);

    // A DIFFERENT tree, with nothing subscribed, must not deliver anything to this recorder --
    // the global flag must not turn into "notify everyone".
    auto other = std::make_shared<XElement>(XName("other"));
    other->Add(std::make_shared<XElement>(XName("x")));
    other->setNameProperty(XName("renamed"));
    EXPECT_EQ(r.total(), 0);

    // And the observed tree still notifies while that registration is live.
    observedLeaf->setNameProperty(XName("renamed"));
    EXPECT_EQ(r.changed, 1);

    // Once the registration is gone the count returns to zero and the fast path resumes, which
    // must not lose a LATER registration -- the count has to be decremented, not just tested.
    EXPECT_TRUE(observedRoot->remove_Changed(sub.changed));
    EXPECT_TRUE(observedRoot->remove_Changing(sub.changing));
    Recorder again;
    const auto sub2 = listen(again, *observedRoot);
    (void)sub2;
    observedLeaf->setNameProperty(XName("again"));
    EXPECT_EQ(again.changed, 1) << "the process-wide count was decremented and re-incremented";
}

TEST(XLinqChangeNotificationTests, Fix1896_DestroyingAnObservedObjectReleasesItsRegistrations) {
    // The count is decremented by ChangeRegistrations' destructor, not only by remove_*. Without
    // that, a destroyed observed object would leave the count permanently non-zero and every tree
    // in the process would pay the full ancestor walk for ever -- a silent, permanent regression
    // of exactly the defect this ticket fixes.
    {
        auto doomed = std::make_shared<XElement>(XName("doomed"));
        const auto tok = doomed->add_Changed([](void*, const XObjectChangeEventArgs&) {});
        (void)tok;
    }   // destroyed here, WITHOUT remove_Changed being called

    // Observable proxy for "the count went back to zero": a deep build is fast again. A leaked
    // count makes this take seconds rather than milliseconds -- and it still BUILDS CORRECTLY, so
    // only a timing assertion can see it. That is why this is timed rather than merely run.
    const double elapsed = buildChainSeconds(40000);
    EXPECT_LT(elapsed, 2.0)
        << "#1896: the process-wide registration count leaked, so every tree in the process now "
           "pays the full ancestor walk. Took " << elapsed << "s.";
}
