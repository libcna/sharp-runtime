// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <type_traits>
#include "System/ValueType.hpp"

using System::ValueType;

namespace {

// Uses base Equals (identity)
class SimpleValueType final : public ValueType {
public:
    SimpleValueType() = default;
};

// Overrides Equals with field equality
class ConcreteValueType final : public ValueType {
    int val_;
public:
    explicit ConcreteValueType(int v) : val_(v) {}
    bool Equals(const ValueType& other) const override {
        const auto* o = dynamic_cast<const ConcreteValueType*>(&other);
        return o && o->val_ == val_;
    }
    int GetHashCode() const override { return val_; }
    std::string ToString() const override { return std::to_string(val_); }
};
}

TEST(ValueTypeTest, DefaultEqualsIdentity) {
    SimpleValueType a, b;
    EXPECT_TRUE(a.Equals(a));
    EXPECT_FALSE(a.Equals(b));
}

TEST(ValueTypeTest, OverriddenEquals) {
    ConcreteValueType a(42), b(42), c(99);
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.Equals(c));
}

TEST(ValueTypeTest, GetHashCode) {
    ConcreteValueType a(5), b(5);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(ValueTypeTest, ToString) {
    ConcreteValueType a(7);
    EXPECT_EQ(a.ToString(), "7");
}

TEST(ValueTypeTest, BaseToString) {
    ValueType* p = new ConcreteValueType(3);
    EXPECT_EQ(p->ToString(), "3");
    delete p;
}

// ---------------------------------------------------------------------------
// #2322 / SR-AUD-068 — the base is no longer directly constructible
// ---------------------------------------------------------------------------

TEST(ValueTypeContractTests, Fix2322_TheBaseIsNoLongerDirectlyConstructible) {
    // .NET declares `public abstract class ValueType`, so C# rejects the equivalent of
    // `System::ValueType v;`. This port compiled it until #2322.
    static_assert(!std::is_default_constructible_v<ValueType>,
                  "#2322: the constructor is protected");
    static_assert(!std::is_copy_constructible_v<ValueType>,
                  "#2322: and the copy member went with it, so the base cannot be sliced out");
    static_assert(std::is_default_constructible_v<SimpleValueType>,
                  "a derived type must still be constructible -- that is the point");

    // NOT abstract, and the distinction is deliberate rather than an oversight: a C++ class is
    // abstract only by having a pure virtual, and .NET's ValueType.ToString() has a real body, so
    // making one pure here would invent surface the reference does not have. The protected
    // constructor gets the property that matters -- base yes, object no.
    static_assert(!std::is_abstract_v<ValueType>,
                  "#2322 deliberately did NOT invent a pure virtual to force abstractness");
}

TEST(ValueTypeContractTests, Decl2322_TheIdentityDefaultsArePermanentDeviations) {
    // NOT A REPAIR -- A DECLARATION. .NET's Equals and GetHashCode compare fields and its
    // ToString returns the runtime type name. All three are reflection, permanently out of scope
    // per CLAUDE.md, and a C++ base class can neither enumerate a derived class's fields nor
    // learn its name.
    SimpleValueType a, b;
    EXPECT_FALSE(a.Equals(b)) << "identity, not value -- .NET would compare fields";
    EXPECT_TRUE(a.Equals(a));

    // The ToString literal is the third of the three, and the one most easily mistaken for a bug.
    EXPECT_EQ("System.ValueType", a.ToString())
        << "the runtime type name is reflection; a derived type must override this itself";

    // Both hooks a derived type needs are already virtual, which is why #2322 invented none.
    static_assert(std::is_polymorphic_v<ValueType>, "Equals/GetHashCode/ToString are virtual");
}
