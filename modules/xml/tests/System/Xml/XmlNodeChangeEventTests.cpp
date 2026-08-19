// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2079 / SR-AUD-352 — XmlDocument's six node-change events are dispatched.
//
// Measured before the repair with the pre-#2079 bodies compiled from source
// (build-probe/2079_probe1_events.cpp, log 2079_probe1_before.log): SIXTEEN distinct public
// mutation doors dispatched ZERO handlers. The handlers are public std::function DATA
// MEMBERS on XmlDocument, so wiring them changes no type, no signature and no layout — the
// corrected premise of docs/SystemXmlNamespaceReviewPlan.md §6.6, and what makes this the
// only ticket in the namespace that can run caller code that never ran before.
//
// One door deliberately still dispatches nothing, and it is a lifetime decision rather than
// an oversight: XmlNode::RemoveAllChildren() (reached by RemoveAll()/RemoveAll on XmlElement)
// PurgeCache()es every wrapper and DeleteChildren()s the natives, so a NodeRemoved handler's
// XmlNode* would name a destroyed object. That is exactly the borrowed-pointer defect CCF-019
// tracks and is NOT introduced here; it carries follow-up ticket #2086. Pinned below so the
// silence is a checked contract rather than an accident.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "System/Exception.hpp"
#include "System/Xml/XmlAttribute.hpp"
#include "System/Xml/XmlComment.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlDocumentFragment.hpp"
#include "System/Xml/XmlElement.hpp"
#include "System/Xml/XmlNodeChangedAction.hpp"
#include "System/Xml/XmlNodeChangedEventArgs.hpp"
#include "System/Xml/XmlProcessingInstruction.hpp"
#include "System/Xml/XmlText.hpp"

using System::Xml::XmlAttribute;
using System::Xml::XmlDocument;
using System::Xml::XmlElement;
using System::Xml::XmlNode;
using System::Xml::XmlNodeChangedAction;
using System::Xml::XmlNodeChangedEventArgs;

namespace {

struct Event {
    std::string     tag;      // "Inserting", "Inserted", "Removing", "Removed", "Changing", "Changed"
    XmlNodeChangedAction action;
    XmlNode*        node;
    XmlNode*        oldParent;
    XmlNode*        newParent;
    std::string     oldValue;
    std::string     newValue;
    void*           sender;
};

/// Installs all six handlers on @p doc and records every dispatch in order.
class Recorder {
public:
    explicit Recorder(XmlDocument& doc) {
        doc.NodeInserting = make("Inserting");
        doc.NodeInserted  = make("Inserted");
        doc.NodeRemoving  = make("Removing");
        doc.NodeRemoved   = make("Removed");
        doc.NodeChanging  = make("Changing");
        doc.NodeChanged   = make("Changed");
    }

    std::vector<Event> events;

    std::vector<std::string> tags() const {
        std::vector<std::string> out;
        out.reserve(events.size());
        for (const auto& e : events) out.push_back(e.tag);
        return out;
    }

private:
    System::Xml::XmlNodeChangedEventHandler make(const char* tag) {
        return [this, tag](void* sender, XmlNodeChangedEventArgs& e) {
            events.push_back(Event{tag, e.getActionProperty(), e.getNodeProperty(),
                                   e.getOldParentProperty(), e.getNewParentProperty(),
                                   e.getOldValueProperty(), e.getNewValueProperty(), sender});
        };
    }
};

XmlElement* Root(XmlDocument& d) {
    auto* r = d.CreateElement("r");
    d.AppendChild(r);
    return r;
}

} // namespace

// ===========================================================================
// #2079 — insert
// ===========================================================================

TEST(XmlNodeChangeEventTests, AppendChildDispatchesTheInsertPairInOrder) {
    XmlDocument d;
    auto* r = Root(d);
    Recorder rec(d);
    auto* c = d.CreateElement("c");
    r->AppendChild(c);

    ASSERT_EQ(rec.events.size(), 2u);
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    for (const auto& e : rec.events) {
        EXPECT_EQ(e.action, XmlNodeChangedAction::Insert);
        EXPECT_EQ(e.node, c);
        EXPECT_EQ(e.newParent, r);
        EXPECT_EQ(e.sender, static_cast<void*>(&d));
    }
}

TEST(XmlNodeChangeEventTests, TheIngHandlerFiresBeforeAndTheEdHandlerAfterTheMutation) {
    XmlDocument d;
    auto* r = Root(d);
    auto* c = d.CreateElement("c");
    bool attachedDuringInserting = true;
    bool attachedDuringInserted  = false;
    d.NodeInserting = [&](void*, XmlNodeChangedEventArgs&) {
        attachedDuringInserting = r->getHasChildNodesProperty();
    };
    d.NodeInserted = [&](void*, XmlNodeChangedEventArgs&) {
        attachedDuringInserted = r->getHasChildNodesProperty();
    };
    r->AppendChild(c);
    EXPECT_FALSE(attachedDuringInserting) << "NodeInserting must fire BEFORE the mutation";
    EXPECT_TRUE(attachedDuringInserted) << "NodeInserted must fire AFTER the mutation";
}

TEST(XmlNodeChangeEventTests, EveryInsertDoorDispatches) {
    // PrependChild / InsertBefore / InsertAfter — all measured silent before #2079.
    {
        XmlDocument d; auto* r = Root(d); Recorder rec(d);
        r->PrependChild(d.CreateElement("c"));
        EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    }
    {
        XmlDocument d; auto* r = Root(d);
        auto* a = d.CreateElement("a"); r->AppendChild(a);
        Recorder rec(d);
        r->InsertBefore(d.CreateElement("b"), a);
        EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    }
    {
        XmlDocument d; auto* r = Root(d);
        auto* a = d.CreateElement("a"); r->AppendChild(a);
        Recorder rec(d);
        r->InsertAfter(d.CreateElement("b"), a);
        EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    }
}

TEST(XmlNodeChangeEventTests, InsertBeforeAtTheFirstPositionAlsoDispatches) {
    // The prevSibling == nullptr branch is a separate code path in InsertBefore.
    XmlDocument d;
    auto* r = Root(d);
    auto* a = d.CreateElement("a");
    r->AppendChild(a);
    Recorder rec(d);
    r->InsertBefore(d.CreateElement("first"), a);
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    EXPECT_EQ(r->getInnerXmlProperty(), "<first/><a/>");
}

TEST(XmlNodeChangeEventTests, AppendingAFragmentDispatchesOncePerChild) {
    XmlDocument d;
    auto* r = Root(d);
    auto* frag = d.CreateDocumentFragment();
    frag->AppendChild(d.CreateElement("f1"));
    frag->AppendChild(d.CreateElement("f2"));
    Recorder rec(d);
    r->AppendChild(frag);
    // Each child leaves the fragment (Remove pair) and enters r (Insert pair).
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Removing", "Removed", "Inserting", "Inserted",
                                                    "Removing", "Removed", "Inserting", "Inserted"}));
    EXPECT_EQ(r->getInnerXmlProperty(), "<f1/><f2/>");
}

// ===========================================================================
// #2079 — remove and replace
// ===========================================================================

TEST(XmlNodeChangeEventTests, RemoveChildDispatchesTheRemovePairWithTheOldParent) {
    XmlDocument d;
    auto* r = Root(d);
    auto* a = d.CreateElement("a");
    r->AppendChild(a);
    Recorder rec(d);
    r->RemoveChild(a);

    ASSERT_EQ(rec.events.size(), 2u);
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Removing", "Removed"}));
    for (const auto& e : rec.events) {
        EXPECT_EQ(e.action, XmlNodeChangedAction::Remove);
        EXPECT_EQ(e.node, a);
        EXPECT_EQ(e.oldParent, r);
        EXPECT_EQ(e.newParent, nullptr);
    }
}

TEST(XmlNodeChangeEventTests, RemoveChildsNodeIsStillAliveWhenNodeRemovedRuns) {
    // The whole reason NodeRemoved is safe here: DetachNode moves the node under the
    // document's scratch parent instead of destroying it.
    XmlDocument d;
    auto* r = Root(d);
    auto* a = d.CreateElement("a");
    a->AppendChild(d.CreateTextNode("payload"));
    r->AppendChild(a);
    std::string seen;
    d.NodeRemoved = [&](void*, XmlNodeChangedEventArgs& e) {
        ASSERT_NE(e.getNodeProperty(), nullptr);
        seen = e.getNodeProperty()->getInnerTextProperty();
    };
    r->RemoveChild(a);
    EXPECT_EQ(seen, "payload");
}

TEST(XmlNodeChangeEventTests, ReplaceChildDispatchesInsertThenRemove) {
    XmlDocument d;
    auto* r = Root(d);
    auto* a = d.CreateElement("a");
    r->AppendChild(a);
    Recorder rec(d);
    r->ReplaceChild(d.CreateElement("b"), a);
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted", "Removing", "Removed"}));
    EXPECT_EQ(r->getInnerXmlProperty(), "<b/>");
}

// ===========================================================================
// #2079 — value change
// ===========================================================================

TEST(XmlNodeChangeEventTests, TextValueChangeDispatchesTheChangePairCarryingBothValues) {
    XmlDocument d;
    auto* r = Root(d);
    auto* t = d.CreateTextNode("old");
    r->AppendChild(t);
    Recorder rec(d);
    t->setValueProperty("new");

    ASSERT_EQ(rec.events.size(), 2u);
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Changing", "Changed"}));
    for (const auto& e : rec.events) {
        EXPECT_EQ(e.action, XmlNodeChangedAction::Change);
        EXPECT_EQ(e.node, t);
        EXPECT_EQ(e.oldValue, "old");
        EXPECT_EQ(e.newValue, "new");
    }
}

TEST(XmlNodeChangeEventTests, EveryCharacterDataMutatorRoutesThroughTheSameChokePoint) {
    // AppendData/InsertData/DeleteData/ReplaceData/setInnerText all reach setDataProperty.
    const auto tagsFor = [](void (*mutate)(System::Xml::XmlText*)) {
        XmlDocument d;
        auto* r = Root(d);
        auto* t = d.CreateTextNode("abcdef");
        r->AppendChild(t);
        Recorder rec(d);
        mutate(t);
        return rec.tags();
    };
    const std::vector<std::string> pair{"Changing", "Changed"};
    EXPECT_EQ(tagsFor([](System::Xml::XmlText* t) { t->AppendData("g"); }), pair);
    EXPECT_EQ(tagsFor([](System::Xml::XmlText* t) { t->InsertData(1, "X"); }), pair);
    EXPECT_EQ(tagsFor([](System::Xml::XmlText* t) { t->DeleteData(1, 2); }), pair);
    EXPECT_EQ(tagsFor([](System::Xml::XmlText* t) { t->ReplaceData(0, 1, "Z"); }), pair);
    EXPECT_EQ(tagsFor([](System::Xml::XmlText* t) { t->setInnerTextProperty("q"); }), pair);
}

TEST(XmlNodeChangeEventTests, CommentAttributeAndProcessingInstructionValueChangesDispatch) {
    {
        XmlDocument d; auto* r = Root(d);
        auto* c = d.CreateComment("old"); r->AppendChild(c);
        Recorder rec(d);
        c->setValueProperty("new");
        ASSERT_EQ(rec.tags(), (std::vector<std::string>{"Changing", "Changed"}));
        EXPECT_EQ(rec.events[0].oldValue, "old");
        EXPECT_EQ(rec.events[1].newValue, "new");
    }
    {
        XmlDocument d; auto* r = Root(d);
        r->SetAttribute("a", "1");
        XmlAttribute* attr = r->GetAttributeNode("a");
        ASSERT_NE(attr, nullptr);
        Recorder rec(d);
        attr->setValueProperty("2");
        ASSERT_EQ(rec.tags(), (std::vector<std::string>{"Changing", "Changed"}));
        EXPECT_EQ(rec.events[0].oldValue, "1");
        EXPECT_EQ(rec.events[1].newValue, "2");
    }
    {
        XmlDocument d;
        auto* pi = d.CreateProcessingInstruction("t", "old");
        d.AppendChild(pi);
        Root(d);
        Recorder rec(d);
        pi->setDataProperty("new");
        ASSERT_EQ(rec.tags(), (std::vector<std::string>{"Changing", "Changed"}));
        EXPECT_EQ(rec.events[0].oldValue, "old");
        EXPECT_EQ(rec.events[1].newValue, "new");
    }
}

// ===========================================================================
// #2079 — the composite content setters
// ===========================================================================

TEST(XmlNodeChangeEventTests, InnerTextAndInnerXmlBothDispatchTheirInserts) {
    {
        XmlDocument d; auto* r = Root(d); Recorder rec(d);
        r->setInnerTextProperty("hello");
        EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted"}));
    }
    {
        // setInnerXml inserts natively rather than through AppendChild, so it needs its own
        // dispatch or it would be the one insert door that stayed silent.
        XmlDocument d; auto* r = Root(d); Recorder rec(d);
        r->setInnerXmlProperty("<x/><y/>");
        EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Inserting", "Inserted",
                                                        "Inserting", "Inserted"}));
        EXPECT_EQ(r->getInnerXmlProperty(), "<x/><y/>");
    }
}

// INVERTED by #2086. The pin said RemoveAllChildren "destroys its children, so a NodeRemoved
// handler would receive an XmlNode* naming freed storage", and #2086 recorded two candidate
// approaches with neither selected. The reference selects one: XmlNode.RemoveAll is literally a
// loop over RemoveChild, which in this port detaches rather than destroys -- so the hazard does
// not arise on that route and the pair is symmetric, one per child.
TEST(XmlNodeChangeEventTests, Fix2086_RemoveAllDispatchesTheFullPairPerChild) {
    XmlDocument d;
    auto* r = Root(d);
    r->AppendChild(d.CreateElement("a"));
    r->AppendChild(d.CreateElement("b"));
    Recorder rec(d);
    r->RemoveAll();
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Removing", "Removed",
                                                    "Removing", "Removed"}));
    EXPECT_FALSE(r->getHasChildNodesProperty());
}

TEST(XmlNodeChangeEventTests, Fix2086_TheNodeAHandlerReceivesIsALiveChild) {
    // THE assertion that says the lifetime hazard is gone rather than merely unobserved: the
    // handler dereferences the node it is given, in the order the children were removed. Against
    // the old body this would have been a use-after-free; against a body that dispatched the
    // node's PARENT or a null it fails outright.
    XmlDocument d;
    auto* r = Root(d);
    r->AppendChild(d.CreateElement("a"));
    r->AppendChild(d.CreateElement("b"));
    r->AppendChild(d.CreateElement("c"));

    std::vector<std::string> seen;
    d.NodeRemoved = [&seen](void*, XmlNodeChangedEventArgs& e) {
        XmlNode* node = e.getNodeProperty();
        seen.push_back(node ? node->getNameProperty() : std::string("<null>"));
    };
    r->RemoveAll();
    EXPECT_EQ(seen, (std::vector<std::string>{"a", "b", "c"}));
}

TEST(XmlNodeChangeEventTests, Fix2086_RemovedChildrenOutliveTheCall) {
    // .NET's removed node becomes an orphan the caller may still use; this port's DetachNode
    // gives the same guarantee. Reading the wrapper after RemoveAll is the whole difference
    // between detaching and destroying.
    XmlDocument d;
    auto* r = Root(d);
    auto* a = d.CreateElement("a");
    r->AppendChild(a);
    r->RemoveAll();
    EXPECT_EQ(a->getNameProperty(), "a");
    EXPECT_EQ(a->getParentNodeProperty(), nullptr);
}

TEST(XmlNodeChangeEventTests, Fix2086_InnerTextAndInnerXmlSettersDispatchTheirRemovalsToo) {
    // The setters reach RemoveAllChildren, so replacing existing content now reports the
    // removals as well as the inserts. Before #2086 only the inserts were visible, which made a
    // handler counting events see content appear from nowhere.
    XmlDocument d;
    auto* r = Root(d);
    r->AppendChild(d.CreateElement("old"));
    Recorder rec(d);
    r->setInnerTextProperty("hello");
    EXPECT_EQ(rec.tags(), (std::vector<std::string>{"Removing", "Removed",
                                                    "Inserting", "Inserted"}));
    EXPECT_EQ(r->getInnerTextProperty(), "hello");
}

// ===========================================================================
// #2079 — the safety contracts
// ===========================================================================

TEST(XmlNodeChangeEventTests, NoHandlerInstalledIsStillANoOp) {
    XmlDocument d;
    auto* r = Root(d);
    auto* c = d.CreateElement("c");
    ASSERT_NO_THROW(r->AppendChild(c));
    ASSERT_NO_THROW(r->RemoveChild(c));
    ASSERT_NO_THROW(r->setInnerTextProperty("t"));
    EXPECT_EQ(d.getOuterXmlProperty(), "<r>t</r>");
}

TEST(XmlNodeChangeEventTests, OnlyTheInstalledHandlerOfAPairIsRequired) {
    XmlDocument d;
    auto* r = Root(d);
    int inserted = 0;
    d.NodeInserted = [&](void*, XmlNodeChangedEventArgs&) { ++inserted; };
    // NodeInserting deliberately left unset.
    r->AppendChild(d.CreateElement("c"));
    EXPECT_EQ(inserted, 1);
}

TEST(XmlNodeChangeEventTests, AThrowingIngHandlerLeavesTheTreeUntouched) {
    XmlDocument d;
    auto* r = Root(d);
    d.NodeInserting = [](void*, XmlNodeChangedEventArgs&) {
        throw System::Exception("handler refused the insert");
    };
    auto* c = d.CreateElement("c");
    EXPECT_THROW(r->AppendChild(c), System::Exception);
    EXPECT_FALSE(r->getHasChildNodesProperty()) << "the *ing handler fires before the mutation";
    EXPECT_EQ(d.getOuterXmlProperty(), "<r/>");
}

TEST(XmlNodeChangeEventTests, AThrowingEdHandlerLeavesTheTreeFullyMutated) {
    XmlDocument d;
    auto* r = Root(d);
    d.NodeInserted = [](void*, XmlNodeChangedEventArgs&) {
        throw System::Exception("handler threw after the insert");
    };
    EXPECT_THROW(r->AppendChild(d.CreateElement("c")), System::Exception);
    // Not corrupt: the mutation completed before the handler ran.
    EXPECT_EQ(d.getOuterXmlProperty(), "<r><c/></r>");
}

TEST(XmlNodeChangeEventTests, AHandlerThatMutatesTheTreeDuringDispatchIsSafe) {
    XmlDocument d;
    auto* r = Root(d);
    int depth = 0;
    d.NodeInserted = [&](void*, XmlNodeChangedEventArgs&) {
        if (++depth == 1) r->AppendChild(d.CreateElement("reentrant"));
    };
    r->AppendChild(d.CreateElement("first"));
    EXPECT_EQ(depth, 2);
    EXPECT_EQ(r->getInnerXmlProperty(), "<first/><reentrant/>");
}

TEST(XmlNodeChangeEventTests, AHandlerThatClearsItsOwnFieldMidDispatchIsSafe) {
    // The dispatcher copies the std::function before invoking it, so a handler that
    // reassigns the very field it is running from does not destroy itself mid-call.
    XmlDocument d;
    auto* r = Root(d);
    int calls = 0;
    d.NodeInserted = [&](void*, XmlNodeChangedEventArgs&) {
        ++calls;
        d.NodeInserted = nullptr;
    };
    ASSERT_NO_THROW(r->AppendChild(d.CreateElement("a")));
    ASSERT_NO_THROW(r->AppendChild(d.CreateElement("b")));
    EXPECT_EQ(calls, 1);
}
