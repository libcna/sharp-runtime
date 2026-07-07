// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Text/Json/Nodes/JsonArray.hpp"
#include "System/Text/Json/Nodes/JsonNode.hpp"
#include "System/Text/Json/Nodes/JsonObject.hpp"
#include "System/Text/Json/Nodes/JsonValue.hpp"

using System::Text::Json::JsonValueKind;
using System::Text::Json::Nodes::JsonArray;
using System::Text::Json::Nodes::JsonNode;
using System::Text::Json::Nodes::JsonObject;
using System::Text::Json::Nodes::JsonValue;

TEST(JsonValueTests, Create_String_RoundTrips) {
    auto v = JsonValue::Create(std::string("hello"));
    EXPECT_EQ(v->GetValueKind(), JsonValueKind::String);
    EXPECT_EQ(v->GetString(), "hello");
}

TEST(JsonValueTests, Create_Int_RoundTrips) {
    auto v = JsonValue::Create(static_cast<SharpRuntime::intcs>(42));
    EXPECT_EQ(v->GetValueKind(), JsonValueKind::Number);
    EXPECT_EQ(v->GetInt32(), 42);
}

TEST(JsonValueTests, Create_Bool_RoundTrips) {
    auto v = JsonValue::Create(true);
    EXPECT_EQ(v->GetValueKind(), JsonValueKind::True);
    EXPECT_TRUE(v->GetBoolean());
}

TEST(JsonValueTests, GetString_WrongKind_Throws) {
    auto v = JsonValue::Create(static_cast<SharpRuntime::intcs>(1));
    EXPECT_THROW(v->GetString(), std::exception);
}

TEST(JsonValueTests, ToJsonString) {
    auto v = JsonValue::Create(std::string("x"));
    EXPECT_EQ(v->ToJsonString(), "\"x\"");
}

TEST(JsonArrayTests, EmptyArray_ToJsonString) {
    JsonArray arr;
    EXPECT_EQ(arr.getCountProperty(), 0);
    EXPECT_EQ(arr.ToJsonString(), "[]");
}

TEST(JsonArrayTests, Add_AppendsItems) {
    JsonArray arr;
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(2)));
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(3)));
    EXPECT_EQ(arr.getCountProperty(), 3);
    EXPECT_EQ(arr.ToJsonString(), "[1,2,3]");
}

TEST(JsonArrayTests, Indexer_ReturnsItem) {
    JsonArray arr;
    arr.Add(JsonValue::Create(std::string("a")));
    EXPECT_EQ(arr[0]->AsValue().GetString(), "a");
}

TEST(JsonArrayTests, Indexer_OutOfRange_Throws) {
    JsonArray arr;
    EXPECT_THROW(arr[0], System::ArgumentOutOfRangeException);
}

TEST(JsonArrayTests, Add_SetsParent) {
    auto arr = std::make_shared<JsonArray>();
    auto item = JsonValue::Create(static_cast<SharpRuntime::intcs>(1));
    arr->Add(item);
    EXPECT_EQ(item->getParentProperty(), arr.get());
}

TEST(JsonArrayTests, RemoveAt_RemovesItem) {
    JsonArray arr;
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(2)));
    arr.RemoveAt(0);
    EXPECT_EQ(arr.getCountProperty(), 1);
    EXPECT_EQ(arr[0]->AsValue().GetInt32(), 2);
}

TEST(JsonArrayTests, Insert_InsertsAtIndex) {
    JsonArray arr;
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(3)));
    arr.Insert(1, JsonValue::Create(static_cast<SharpRuntime::intcs>(2)));
    EXPECT_EQ(arr.ToJsonString(), "[1,2,3]");
}

TEST(JsonArrayTests, ArrayWithNullElement) {
    JsonArray arr;
    arr.Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    arr.Add(nullptr);
    EXPECT_EQ(arr.ToJsonString(), "[1,null]");
}

TEST(JsonArrayTests, DeepClone_ProducesIndependentCopy) {
    auto arr = std::make_shared<JsonArray>();
    arr->Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    auto clone = arr->DeepClone();
    EXPECT_EQ(clone->ToJsonString(), arr->ToJsonString());
    EXPECT_NE(clone.get(), arr.get());
    EXPECT_EQ(clone->getParentProperty(), nullptr);
}

TEST(JsonObjectTests, EmptyObject_ToJsonString) {
    JsonObject obj;
    EXPECT_EQ(obj.getCountProperty(), 0);
    EXPECT_EQ(obj.ToJsonString(), "{}");
}

TEST(JsonObjectTests, Add_And_Indexer) {
    JsonObject obj;
    obj.Add("name", JsonValue::Create(std::string("Robert")));
    obj.Add("age", JsonValue::Create(static_cast<SharpRuntime::intcs>(42)));
    EXPECT_EQ(obj["name"]->AsValue().GetString(), "Robert");
    EXPECT_EQ(obj["age"]->AsValue().GetInt32(), 42);
}

TEST(JsonObjectTests, Add_DuplicateKey_Throws) {
    JsonObject obj;
    obj.Add("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    EXPECT_THROW(obj.Add("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(2))), System::ArgumentException);
}

TEST(JsonObjectTests, Indexer_Missing_Throws) {
    JsonObject obj;
    EXPECT_THROW(obj["missing"], System::Collections::Generic::KeyNotFoundException);
}

TEST(JsonObjectTests, TryGetPropertyValue) {
    JsonObject obj;
    obj.Add("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    std::shared_ptr<JsonNode> out;
    EXPECT_TRUE(obj.TryGetPropertyValue("x", out));
    EXPECT_EQ(out->AsValue().GetInt32(), 1);
    EXPECT_FALSE(obj.TryGetPropertyValue("missing", out));
}

TEST(JsonObjectTests, Remove_RemovesProperty) {
    JsonObject obj;
    obj.Add("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    EXPECT_TRUE(obj.Remove("x"));
    EXPECT_FALSE(obj.ContainsKey("x"));
    EXPECT_FALSE(obj.Remove("x"));
}

TEST(JsonObjectTests, SetItem_UpdatesExistingOrAdds) {
    JsonObject obj;
    obj.Add("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    obj.SetItem("x", JsonValue::Create(static_cast<SharpRuntime::intcs>(2)));
    EXPECT_EQ(obj["x"]->AsValue().GetInt32(), 2);
    obj.SetItem("y", JsonValue::Create(static_cast<SharpRuntime::intcs>(3)));
    EXPECT_EQ(obj["y"]->AsValue().GetInt32(), 3);
}

TEST(JsonObjectTests, Add_SetsParent) {
    auto obj = std::make_shared<JsonObject>();
    auto value = JsonValue::Create(static_cast<SharpRuntime::intcs>(1));
    obj->Add("x", value);
    EXPECT_EQ(value->getParentProperty(), obj.get());
}

TEST(JsonObjectTests, PreservesInsertionOrder) {
    JsonObject obj;
    obj.Add("z", JsonValue::Create(static_cast<SharpRuntime::intcs>(1)));
    obj.Add("a", JsonValue::Create(static_cast<SharpRuntime::intcs>(2)));
    obj.Add("m", JsonValue::Create(static_cast<SharpRuntime::intcs>(3)));
    EXPECT_EQ(obj.ToJsonString(), R"({"z":1,"a":2,"m":3})");
}

TEST(JsonNodeTests, Parse_Object) {
    auto node = JsonNode::Parse(R"({"x":1,"y":"two"})");
    ASSERT_NE(node, nullptr);
    auto& obj = node->AsObject();
    EXPECT_EQ(obj["x"]->AsValue().GetInt32(), 1);
    EXPECT_EQ(obj["y"]->AsValue().GetString(), "two");
}

TEST(JsonNodeTests, Parse_Array) {
    auto node = JsonNode::Parse("[1,2,3]");
    ASSERT_NE(node, nullptr);
    auto& arr = node->AsArray();
    EXPECT_EQ(arr.getCountProperty(), 3);
}

TEST(JsonNodeTests, Parse_NestedStructure) {
    auto node = JsonNode::Parse(R"({"tags":["a","b"],"nested":{"k":true}})");
    auto& root = node->AsObject();
    auto& tags = root["tags"]->AsArray();
    EXPECT_EQ(tags.getCountProperty(), 2);
    EXPECT_EQ(tags[0]->AsValue().GetString(), "a");
    auto& nested = root["nested"]->AsObject();
    EXPECT_TRUE(nested["k"]->AsValue().GetBoolean());
}

TEST(JsonNodeTests, Parse_NullLiteral_ReturnsNullptr) {
    auto node = JsonNode::Parse("null");
    EXPECT_EQ(node, nullptr);
}

TEST(JsonNodeTests, Parse_ObjectWithNullProperty) {
    auto node = JsonNode::Parse(R"({"x":null})");
    auto& obj = node->AsObject();
    std::shared_ptr<JsonNode> x;
    EXPECT_TRUE(obj.TryGetPropertyValue("x", x));
    EXPECT_EQ(x, nullptr);
}

TEST(JsonNodeTests, Parse_InvalidJson_Throws) {
    EXPECT_THROW(JsonNode::Parse("{bad"), std::exception);
}

TEST(JsonNodeTests, AsArray_WrongKind_Throws) {
    auto node = JsonValue::Create(static_cast<SharpRuntime::intcs>(1));
    EXPECT_THROW(node->AsArray(), System::InvalidOperationException);
}

TEST(JsonNodeTests, GetRoot_ReturnsTopmostAncestor) {
    auto root = std::make_shared<JsonObject>();
    auto array = std::make_shared<JsonArray>();
    root->Add("items", array);
    auto value = JsonValue::Create(static_cast<SharpRuntime::intcs>(1));
    array->Add(value);
    EXPECT_EQ(value->getRootProperty(), root.get());
}

TEST(JsonNodeTests, DeepEquals_ComparesStructurally) {
    auto a = JsonNode::Parse(R"({"x":1,"y":[1,2]})");
    auto b = JsonNode::Parse(R"({"x":1,"y":[1,2]})");
    auto c = JsonNode::Parse(R"({"x":2})");
    EXPECT_TRUE(JsonNode::DeepEquals(a.get(), b.get()));
    EXPECT_FALSE(JsonNode::DeepEquals(a.get(), c.get()));
    EXPECT_TRUE(JsonNode::DeepEquals(nullptr, nullptr));
    EXPECT_FALSE(JsonNode::DeepEquals(a.get(), nullptr));
}

TEST(JsonNodeTests, WriteThenParse_RoundTrips) {
    auto obj = std::make_shared<JsonObject>();
    obj->Add("name", JsonValue::Create(std::string("Bob")));
    obj->Add("scores", std::make_shared<JsonArray>());
    obj->operator[]("scores")->AsArray().Add(JsonValue::Create(static_cast<SharpRuntime::intcs>(10)));
    std::string json = obj->ToJsonString();
    auto reparsed = JsonNode::Parse(json);
    EXPECT_TRUE(JsonNode::DeepEquals(obj.get(), reparsed.get()));
}
