// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "System/Xml/Serialization/detail/XmlLeafConvert.hpp"
#include "System/Xml/Serialization/detail/XmlMember.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlElement.hpp"
#include "System/Xml/XmlException.hpp"

namespace System::Xml::Serialization {

    /**
     * @brief Provides methods for serializing objects to XML and deserializing XML to objects.
     *
     * C++ counterpart of .NET `System.Xml.Serialization.XmlSerializer`, scoped to what
     * `SAMPLES-DEC-008` actually needs: the closed set of save-data types reachable from
     * ShipGame's `EntityList`/`LightList` and RolePlayingGame's `Session` (see
     * `docs/XmlSerializationScope.md` for the full inventory and the evidence that none of them
     * use `[XmlInclude]`/xsi:type polymorphism).
     *
     * @note Real .NET discovers a type's serializable members and their order via reflection.
     * That is a permanent deviation here (CLAUDE.md), so a type opts in explicitly with
     * `SHARP_XML_SERIALIZABLE(Type, "RootElementName", SHARP_XML_M(Type, member1), ...)` --
     * the same compile-time-customization-point shape `JsonSerializer` already uses via
     * `nlohmann`'s ADL hooks, just with no vendored library underneath since none exists for
     * XML object-mapping (`tinyxml2` is a parser, not a mapper).
     *
     * **Deliberately out of scope** (tracked in `docs/XmlSerializationScope.md`, not silently
     * missing): `[XmlInclude]`/xsi:type polymorphic dispatch, `[XmlArray]`/`[XmlArrayItem]`
     * overrides, `[XmlAttribute]`-mapped members, circular-reference detection, and root-level
     * `List<T>` where `T` is itself generic (XNA's `ArrayOfWorldEntryOfChest`-style name
     * mangling). None of these are exercised by the reachable call sites in the three DEC-008
     * samples' `Session.cs`/`EntityList.cs`/`LightList.cs`.
     */
    template <typename T>
    class XmlSerializer {
    public:
        XmlSerializer() = default;

        /** @brief Serializes @p value to an XML document string, root element carrying the
         * `xsi`/`xsd` namespace declarations .NET's default `XmlSerializer` always writes. */
        [[nodiscard]] std::string Serialize(const T& value) const {
            System::Xml::XmlDocument doc;
            doc.AppendChild(doc.CreateXmlDeclaration("1.0", "utf-8", ""));

            if constexpr (detail::IsXmlListV<T>) {
                using Item = typename T::value_type;
                std::string rootName = std::string("ArrayOf") + SharpXmlRootName(static_cast<const Item*>(nullptr));
                System::Xml::XmlElement* root = MakeRootElement(doc, rootName);
                for (const Item& item : value) {
                    WriteValue(doc, root, SharpXmlRootName(static_cast<const Item*>(nullptr)), item);
                }
            } else {
                static_assert(detail::XmlComposite<T>,
                              "XmlSerializer<T>::Serialize: T must be SHARP_XML_SERIALIZABLE, or "
                              "a std::vector of one.");
                System::Xml::XmlElement* root = MakeRootElement(doc, SharpXmlRootName(static_cast<const T*>(nullptr)));
                WriteMembers(doc, root, value);
            }

            return doc.getOuterXmlProperty();
        }

        /** @brief Parses @p xml produced by (or wire-compatible with) `Serialize`, back into a
         * @p T. @throws System::Xml::XmlException on malformed XML or a missing required
         * element. */
        [[nodiscard]] T Deserialize(const std::string& xml) const {
            System::Xml::XmlDocument doc;
            doc.LoadXml(xml);
            System::Xml::XmlElement* root = doc.getDocumentElementProperty();
            if (root == nullptr) {
                throw System::Xml::XmlException("XmlSerializer::Deserialize: no root element.");
            }

            T result{};
            if constexpr (detail::IsXmlListV<T>) {
                using Item = typename T::value_type;
                const char* itemName = SharpXmlRootName(static_cast<const Item*>(nullptr));
                for (System::Xml::XmlNode* child = root->getFirstChildProperty(); child != nullptr;
                     child = child->getNextSiblingProperty()) {
                    if (child->getNameProperty() != itemName) continue;
                    Item item{};
                    ReadMembers(static_cast<System::Xml::XmlElement*>(child), item);
                    result.push_back(std::move(item));
                }
            } else {
                static_assert(detail::XmlComposite<T>,
                              "XmlSerializer<T>::Deserialize: T must be SHARP_XML_SERIALIZABLE, "
                              "or a std::vector of one.");
                ReadMembers(root, result);
            }
            return result;
        }

    private:
        [[nodiscard]] static System::Xml::XmlElement* MakeRootElement(System::Xml::XmlDocument& doc,
                                                                        const std::string& name) {
            System::Xml::XmlElement* root = doc.CreateElement(name);
            root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
            root->SetAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
            doc.AppendChild(root);
            return root;
        }

        // --- write side --------------------------------------------------------------------

        template <typename Value>
        static void WriteValue(System::Xml::XmlDocument& doc, System::Xml::XmlElement* parent,
                                const std::string& elementName, const Value& value) {
            if constexpr (detail::IsXmlListV<Value>) {
                System::Xml::XmlElement* wrapper = doc.CreateElement(elementName);
                parent->AppendChild(wrapper);
                using Item = typename Value::value_type;
                for (const Item& item : value) {
                    WriteValue(doc, wrapper, SharpXmlRootName(static_cast<const Item*>(nullptr)), item);
                }
            } else if constexpr (detail::XmlComposite<Value>) {
                System::Xml::XmlElement* element = doc.CreateElement(elementName);
                parent->AppendChild(element);
                WriteMembers(doc, element, value);
            } else {
                System::Xml::XmlElement* element = doc.CreateElement(elementName);
                parent->AppendChild(element);
                element->setInnerTextProperty(detail::ToXmlText(value));
            }
        }

        template <typename Composite>
        static void WriteMembers(System::Xml::XmlDocument& doc, System::Xml::XmlElement* element,
                                  const Composite& value) {
            auto members = SharpXmlMembers(static_cast<const Composite*>(nullptr));
            std::apply(
                [&](const auto&... member) { (WriteValue(doc, element, member.name, value.*(member.ptr)), ...); },
                members);
        }

        // --- read side -----------------------------------------------------------------------

        template <typename Value>
        static void ReadInto(System::Xml::XmlElement* element, Value& out) {
            if constexpr (detail::IsXmlListV<Value>) {
                using Item = typename Value::value_type;
                const char* itemName = SharpXmlRootName(static_cast<const Item*>(nullptr));
                for (System::Xml::XmlNode* child = element->getFirstChildProperty(); child != nullptr;
                     child = child->getNextSiblingProperty()) {
                    if (child->getNameProperty() != itemName) continue;
                    Item item{};
                    ReadInto(static_cast<System::Xml::XmlElement*>(child), item);
                    out.push_back(std::move(item));
                }
            } else if constexpr (detail::XmlComposite<Value>) {
                ReadMembers(element, out);
            } else {
                out = detail::FromXmlText<Value>(element->getInnerTextProperty());
            }
        }

        template <typename Composite>
        static void ReadMembers(System::Xml::XmlElement* element, Composite& out) {
            auto members = SharpXmlMembers(static_cast<const Composite*>(nullptr));
            std::apply(
                [&](const auto&... member) {
                    (
                        [&] {
                            System::Xml::XmlNode* found = nullptr;
                            for (System::Xml::XmlNode* child = element->getFirstChildProperty(); child != nullptr;
                                 child = child->getNextSiblingProperty()) {
                                if (child->getNameProperty() == member.name) {
                                    found = child;
                                    break;
                                }
                            }
                            if (found == nullptr) {
                                throw System::Xml::XmlException(std::string("XmlSerializer::Deserialize: missing element <") +
                                                                 member.name + ">.");
                            }
                            ReadInto(static_cast<System::Xml::XmlElement*>(found), out.*(member.ptr));
                        }(),
                        ...);
                },
                members);
        }
    };

}  // namespace System::Xml::Serialization
