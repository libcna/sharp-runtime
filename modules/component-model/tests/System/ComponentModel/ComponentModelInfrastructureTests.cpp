// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <gtest/gtest.h>

#include <any>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Hashtable.hpp"
#include "System/ComponentModel/Design/Serialization/InstanceDescriptor.hpp"
#include "System/ComponentModel/BrowsableAttribute.hpp"
#include "System/ComponentModel/DefaultValueAttribute.hpp"
#include "System/ComponentModel/ExpandableObjectConverter.hpp"
#include "System/ComponentModel/Int32Converter.hpp"
#include "System/ComponentModel/PropertyDescriptorCollection.hpp"
#include "System/ComponentModel/SingleConverter.hpp"
#include "System/ComponentModel/TypeConverterAttribute.hpp"
#include "System/ComponentModel/TypeDescriptor.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Reflection/ConstructorInfo.hpp"

namespace {

using System::ComponentModel::AttributeCollection;
using System::ComponentModel::ExpandableObjectConverter;
using System::ComponentModel::ITypeDescriptorContext;
using System::ComponentModel::PropertyDescriptor;
using System::ComponentModel::PropertyDescriptorCollection;
using System::ComponentModel::TypeConverter;
using System::ComponentModel::TypeConverterAttribute;
using System::ComponentModel::TypeDescriptor;
using System::ComponentModel::Design::Serialization::InstanceDescriptor;

struct SampleComponent {
    SharpRuntime::intcs Number = 0;
    std::string Text;
};

struct NestedSample {
    SampleComponent Value;
};

class SamplePropertyDescriptor final : public PropertyDescriptor {
public:
    enum class Member { Number, Text };

    SamplePropertyDescriptor(std::string name, Member member, AttributeCollection attributes = {})
        : PropertyDescriptor(std::move(name), std::move(attributes)), member_(member) {}

    [[nodiscard]] System::Type getComponentTypeProperty() const override {
        return System::Type::From<SampleComponent>();
    }

    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

    [[nodiscard]] System::Type getPropertyTypeProperty() const override {
        return member_ == Member::Number ? System::Type::From<SharpRuntime::intcs>()
                                         : System::Type::From<std::string>();
    }

    [[nodiscard]] bool CanResetValue(const std::any& component) const override {
        (void)component;
        return false;
    }

    [[nodiscard]] std::any GetValue(const std::any& component) const override {
        const SampleComponent& sample = std::any_cast<const SampleComponent&>(component);
        return member_ == Member::Number ? std::any(sample.Number) : std::any(sample.Text);
    }

    void ResetValue(std::any& component) const override { (void)component; }

    void SetValue(std::any& component, const std::any& value) const override {
        SampleComponent& sample = std::any_cast<SampleComponent&>(component);
        if (member_ == Member::Number) {
            sample.Number = std::any_cast<SharpRuntime::intcs>(value);
        } else {
            sample.Text = std::any_cast<std::string>(value);
        }
    }

    [[nodiscard]] bool ShouldSerializeValue(const std::any& component) const override {
        (void)component;
        return true;
    }

private:
    Member member_;
};

PropertyDescriptorCollection sampleProperties() {
    return PropertyDescriptorCollection({
        std::make_shared<SamplePropertyDescriptor>("Number", SamplePropertyDescriptor::Member::Number),
        std::make_shared<SamplePropertyDescriptor>("Text", SamplePropertyDescriptor::Member::Text),
    });
}

class MarkerConverterA final : public TypeConverter {};
class MarkerConverterB final : public TypeConverter {};

class NullAcceptingConverter final : public TypeConverter {
public:
    using TypeConverter::ConvertFrom;

    [[nodiscard]] std::any ConvertFrom(ITypeDescriptorContext* context,
                                       const System::Globalization::CultureInfo* culture,
                                       const std::any& value) const override {
        if (!value.has_value()) return std::any(std::string("accepted null"));
        return TypeConverter::ConvertFrom(context, culture, value);
    }
};

class DefaultedNumberDescriptor final : public TypeConverter::SimplePropertyDescriptor {
public:
    DefaultedNumberDescriptor()
        : SimplePropertyDescriptor(System::Type::From<SampleComponent>(), "Number",
              System::Type::From<SharpRuntime::intcs>(),
              AttributeCollection({std::make_shared<System::ComponentModel::DefaultValueAttribute>(
                  SharpRuntime::intcs{5})})) {}

    [[nodiscard]] std::any GetValue(const std::any& component) const override {
        return std::any(std::any_cast<const SampleComponent&>(component).Number);
    }

    void SetValue(std::any& component, const std::any& value) const override {
        std::any_cast<SampleComponent&>(component).Number = std::any_cast<SharpRuntime::intcs>(value);
    }
};

struct RegisteredType {};
struct ConcurrentType {};
struct UnknownType {};

} // namespace

TEST(TypeConverterTests, BaseSupportsStringsAndInstanceDescriptorsOnly) {
    const TypeConverter converter;
    EXPECT_TRUE(converter.CanConvertFrom(System::Type::From<InstanceDescriptor>()));
    EXPECT_FALSE(converter.CanConvertFrom(System::Type::From<std::string>()));
    EXPECT_TRUE(converter.CanConvertTo(System::Type::From<std::string>()));
    EXPECT_FALSE(converter.CanConvertTo(System::Type::From<SharpRuntime::intcs>()));
    EXPECT_EQ(converter.ConvertToString(std::any(SharpRuntime::intcs{42})), "42");
    EXPECT_THROW((void)converter.ConvertFrom(std::any(std::string("42"))), System::NotSupportedException);
}

TEST(TypeConverterTests, NumericStringHelpersPropagateCulture) {
    const System::ComponentModel::SingleConverter converter;
    EXPECT_TRUE(converter.CanConvertFrom(System::Type::From<std::string>()));
    EXPECT_TRUE(converter.CanConvertFrom(System::Type::From<std::string_view>()));
    EXPECT_TRUE(converter.CanConvertFrom(System::Type::From<const char*>()));
    EXPECT_TRUE(converter.CanConvertFrom(System::Type::From<char*>()));
    const System::Globalization::CultureInfo cs("cs-CZ");
    const std::any parsed = converter.ConvertFromString(nullptr, &cs, " 12,5 ");
    EXPECT_FLOAT_EQ(std::any_cast<float>(parsed), 12.5F);
    EXPECT_EQ(converter.ConvertToString(nullptr, &cs, std::any(12.5F)), "12,5");
    EXPECT_THROW((void)converter.ConvertFromString(nullptr, &cs, "12.5"), System::Exception);
}

TEST(TypeConverterTests, IsValidLetsAConverterDecideWhetherNullIsValid) {
    const TypeConverter base;
    const NullAcceptingConverter accepting;
    EXPECT_FALSE(base.IsValid(std::any{}));
    EXPECT_TRUE(accepting.IsValid(std::any{}));
}

TEST(PropertyDescriptorTests, MetadataAndValueAccessAreMeaningful) {
    const auto descriptor = std::make_shared<SamplePropertyDescriptor>(
        "Number", SamplePropertyDescriptor::Member::Number);
    EXPECT_EQ(descriptor->getNameProperty(), "Number");
    EXPECT_EQ(descriptor->getComponentTypeProperty(), System::Type::From<SampleComponent>());
    EXPECT_EQ(descriptor->getPropertyTypeProperty(), System::Type::From<SharpRuntime::intcs>());
    EXPECT_FALSE(descriptor->getIsReadOnlyProperty());
    EXPECT_FALSE(descriptor->CanResetValue(std::any(SampleComponent{})));
    EXPECT_TRUE(descriptor->ShouldSerializeValue(std::any(SampleComponent{})));

    std::any component = SampleComponent{7, "before"};
    EXPECT_EQ(std::any_cast<SharpRuntime::intcs>(descriptor->GetValue(component)), 7);
    descriptor->SetValue(component, std::any(SharpRuntime::intcs{19}));
    EXPECT_EQ(std::any_cast<SampleComponent>(component).Number, 19);
}

TEST(PropertyDescriptorTests, EqualityUsesNameAndPropertyType) {
    const SamplePropertyDescriptor first("Number", SamplePropertyDescriptor::Member::Number);
    const SamplePropertyDescriptor same("Number", SamplePropertyDescriptor::Member::Number);
    const SamplePropertyDescriptor different("Text", SamplePropertyDescriptor::Member::Text);
    EXPECT_TRUE(first.Equals(same));
    EXPECT_EQ(first.GetHashCode(), same.GetHashCode());
    EXPECT_FALSE(first.Equals(different));
}

TEST(PropertyDescriptorTests, SimpleDescriptorMatchesDotNetDefaultResetSemantics) {
    const DefaultedNumberDescriptor descriptor;
    std::any component = SampleComponent{5, {}};
    EXPECT_TRUE(descriptor.CanResetValue(component));
    EXPECT_FALSE(descriptor.ShouldSerializeValue(component));

    std::any_cast<SampleComponent&>(component).Number = 19;
    EXPECT_FALSE(descriptor.CanResetValue(component));
    descriptor.ResetValue(component);
    EXPECT_EQ(std::any_cast<const SampleComponent&>(component).Number, 5);
}

TEST(PropertyDescriptorCollectionTests, EmptyLookupAndBoundsMatchTheCollectionContract) {
    EXPECT_EQ(PropertyDescriptorCollection::Empty.getCountProperty(), 0);
    EXPECT_TRUE(PropertyDescriptorCollection::Empty.getIsReadOnlyProperty());
    EXPECT_EQ(PropertyDescriptorCollection::Empty.Find("missing", false), nullptr);
    EXPECT_THROW((void)PropertyDescriptorCollection::Empty.getItem(0), System::IndexOutOfRangeException);
    auto emptyCopy = PropertyDescriptorCollection::Empty;
    EXPECT_THROW(emptyCopy.Clear(), System::NotSupportedException);
}

TEST(PropertyDescriptorCollectionTests, IndexNameIterationAndStableOrderingWork) {
    PropertyDescriptorCollection properties = sampleProperties();
    ASSERT_EQ(properties.getCountProperty(), 2);
    EXPECT_EQ(properties.getItem(0)->getNameProperty(), "Number");
    EXPECT_EQ(properties.getItem("Text")->getNameProperty(), "Text");
    EXPECT_EQ(properties.Find("number", false), nullptr);
    EXPECT_EQ(properties.Find("number", true)->getNameProperty(), "Number");

    std::vector<std::string> names;
    for (const auto& property : properties) names.push_back(property->getNameProperty());
    EXPECT_EQ(names, (std::vector<std::string>{"Number", "Text"}));

    const PropertyDescriptorCollection reversed = properties.Sort({"Text", "Number"});
    EXPECT_EQ(reversed.getItem(0)->getNameProperty(), "Text");
    EXPECT_EQ(reversed.getItem(1)->getNameProperty(), "Number");
    EXPECT_EQ(properties.getItem(0)->getNameProperty(), "Number") << "Sort must not mutate the source";
}

TEST(PropertyDescriptorCollectionTests, CopiesShareDescriptorLifetimeButOwnTheirOrder) {
    PropertyDescriptorCollection original = sampleProperties();
    PropertyDescriptorCollection copy = original;
    ASSERT_EQ(copy.getItem(0), original.getItem(0));
    copy.RemoveAt(0);
    EXPECT_EQ(copy.getCountProperty(), 1);
    EXPECT_EQ(original.getCountProperty(), 2);
    EXPECT_THROW(copy.RemoveAt(3), System::ArgumentOutOfRangeException);
}

TEST(ExpandableObjectConverterTests, ReturnsExplicitlyRegisteredProperties) {
    TypeDescriptor::RegisterType<SampleComponent>({}, sampleProperties());
    const ExpandableObjectConverter converter;
    EXPECT_TRUE(converter.GetPropertiesSupported());
    const auto properties = converter.GetProperties(std::any(SampleComponent{}));
    ASSERT_EQ(properties.getCountProperty(), 2);
    EXPECT_EQ(properties.getItem(0)->getNameProperty(), "Number");
    EXPECT_EQ(properties.getItem(1)->getNameProperty(), "Text");
}

TEST(ExpandableObjectConverterTests, ConvenienceOverloadFiltersNonBrowsableProperties) {
    PropertyDescriptorCollection properties({
        std::make_shared<SamplePropertyDescriptor>("Number", SamplePropertyDescriptor::Member::Number),
        std::make_shared<SamplePropertyDescriptor>(
            "Text", SamplePropertyDescriptor::Member::Text,
            AttributeCollection({std::make_shared<System::ComponentModel::BrowsableAttribute>(false)})),
    });
    TypeDescriptor::RegisterType<SampleComponent>({}, properties);
    const ExpandableObjectConverter converter;

    const auto browsable = converter.GetProperties(std::any(SampleComponent{}));
    ASSERT_EQ(browsable.getCountProperty(), 1);
    EXPECT_EQ(browsable.getItem(0)->getNameProperty(), "Number");

    const auto unfiltered = converter.GetProperties(
        nullptr, std::any(SampleComponent{}), AttributeCollection::Empty);
    EXPECT_EQ(unfiltered.getCountProperty(), 2);
}

TEST(TypeDescriptorTests, UnknownAndIntrinsicTypesUseDeterministicConverters) {
    const auto unknown = TypeDescriptor::GetConverter(System::Type::From<UnknownType>());
    ASSERT_NE(unknown, nullptr);
    EXPECT_EQ(typeid(*unknown), typeid(TypeConverter));
    EXPECT_EQ(unknown, TypeDescriptor::GetConverter(System::Type::From<UnknownType>()));

    const auto integer = TypeDescriptor::GetConverter(System::Type::From<SharpRuntime::intcs>());
    EXPECT_NE(dynamic_cast<System::ComponentModel::Int32Converter*>(integer.get()), nullptr);
    EXPECT_EQ(std::any_cast<SharpRuntime::intcs>(integer->ConvertFromInvariantString("123")), 123);
}

TEST(TypeDescriptorTests, RegistrationIsLazyCachedAndReplaceable) {
    std::atomic<int> creations{0};
    auto firstAttribute = std::make_shared<TypeConverterAttribute>(
        System::Type::From<MarkerConverterA>(), [&creations] {
            ++creations;
            return std::shared_ptr<TypeConverter>(std::make_shared<MarkerConverterA>());
        });
    TypeDescriptor::RegisterType<RegisteredType>({firstAttribute});
    EXPECT_EQ(creations.load(), 0);
    const auto first = TypeDescriptor::GetConverter(System::Type::From<RegisteredType>());
    EXPECT_EQ(creations.load(), 1);
    EXPECT_EQ(first, TypeDescriptor::GetConverter(System::Type::From<RegisteredType>()));
    EXPECT_EQ(creations.load(), 1);

    TypeDescriptor::AddAttributes(System::Type::From<RegisteredType>(), {
        std::make_shared<TypeConverterAttribute>(TypeConverterAttribute::Of<MarkerConverterB>())});
    const auto replacement = TypeDescriptor::GetConverter(System::Type::From<RegisteredType>());
    EXPECT_NE(replacement, first);
    EXPECT_NE(dynamic_cast<MarkerConverterB*>(replacement.get()), nullptr);
}

TEST(TypeDescriptorTests, ConcurrentLookupsShareOnePublishedConverter) {
    std::atomic<int> creations{0};
    TypeDescriptor::RegisterType<ConcurrentType>({std::make_shared<TypeConverterAttribute>(
        System::Type::From<MarkerConverterA>(), [&creations] {
            ++creations;
            return std::shared_ptr<TypeConverter>(std::make_shared<MarkerConverterA>());
        })});

    std::vector<std::shared_ptr<TypeConverter>> results(16);
    std::vector<std::thread> threads;
    threads.reserve(results.size());
    for (std::size_t i = 0; i < results.size(); ++i) {
        threads.emplace_back([i, &results] {
            results[i] = TypeDescriptor::GetConverter(System::Type::From<ConcurrentType>());
        });
    }
    for (auto& thread : threads) thread.join();
    for (const auto& result : results) EXPECT_EQ(result, results.front());
    EXPECT_GE(creations.load(), 1);
}

TEST(InstanceDescriptorTests, StoresMetadataArgumentsCompletenessAndInvokes) {
    const auto constructor = System::Reflection::ConstructorInfo::Of<SampleComponent,
        SharpRuntime::intcs, std::string>({"number", "text"});
    const InstanceDescriptor descriptor(constructor,
        {std::any(SharpRuntime::intcs{17}), std::any(std::string("value"))}, false);
    EXPECT_EQ(descriptor.getMemberInfoProperty(), constructor);
    EXPECT_EQ(descriptor.getArgumentsProperty().size(), 2U);
    EXPECT_FALSE(descriptor.getIsCompleteProperty());

    const SampleComponent rebuilt = std::any_cast<SampleComponent>(descriptor.Invoke());
    EXPECT_EQ(rebuilt.Number, 17);
    EXPECT_EQ(rebuilt.Text, "value");
}

TEST(InstanceDescriptorTests, RecursivelyInvokesNestedDescriptors) {
    const auto innerConstructor = System::Reflection::ConstructorInfo::Of<SampleComponent,
        SharpRuntime::intcs, std::string>();
    const InstanceDescriptor inner(innerConstructor,
        {std::any(SharpRuntime::intcs{23}), std::any(std::string("nested"))});
    const auto outerConstructor = System::Reflection::ConstructorInfo::Of<NestedSample, SampleComponent>();
    const InstanceDescriptor outer(outerConstructor, {std::any(inner)});

    const NestedSample rebuilt = std::any_cast<NestedSample>(outer.Invoke());
    EXPECT_EQ(rebuilt.Value.Number, 23);
    EXPECT_EQ(rebuilt.Value.Text, "nested");
}

TEST(InstanceDescriptorTests, ValidatesCountAndReportsWrongArgumentType) {
    const auto constructor = System::Reflection::ConstructorInfo::Of<SampleComponent,
        SharpRuntime::intcs, std::string>();
    EXPECT_THROW((InstanceDescriptor(constructor, {std::any(SharpRuntime::intcs{1})})),
                 System::ArgumentException);
    const InstanceDescriptor wrongType(constructor,
        {std::any(std::string("not an integer")), std::any(std::string("value"))});
    EXPECT_THROW((void)wrongType.Invoke(), System::ArgumentException);
}

TEST(InstanceDescriptorTests, NullMemberIsAStableEmptyDescription) {
    const InstanceDescriptor descriptor(nullptr, {}, true);
    EXPECT_EQ(descriptor.getMemberInfoProperty(), nullptr);
    EXPECT_TRUE(descriptor.getArgumentsProperty().empty());
    EXPECT_TRUE(descriptor.getIsCompleteProperty());
    EXPECT_FALSE(descriptor.Invoke().has_value());
}
