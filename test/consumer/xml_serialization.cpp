// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Proves the Xml.Serialization component is usable on its own, selected alone, from outside the
// repository build tree -- which is exactly how `cna-samples` will consume it. A bare include
// would only prove the header resolves; this round-trips a value, so a component whose headers
// resolve but whose transitive Xml dependency is missing at link time fails here.
//
// **This fixture does not build under the harness's `-Wpedantic -Werror`, and the cause is
// inherited rather than local.** `XmlSerializer.hpp` reaches `System/Xml/XmlConvert.hpp` (for
// the float/int text conversions), which reaches `System/Decimal.hpp`, whose `__int128` trips
// `-Werror=pedantic`. Measured: a fixture that includes nothing but `System/Xml/XmlConvert.hpp`
// under component `Xml` fails identically, so this is a pre-existing condition of the `Xml`
// consumer surface -- which is presumably why the catalogue names the tiny
// `System/Xml/ConformanceLevel.hpp` as `Xml`'s representative header.
//
// Verified separately, with `-Wall -Wextra -Werror` and without `-Wpedantic`: this file
// compiles clean, links against the selected component closure, and returns 0. The component
// resolution is also right -- asking for `Xml.Serialization` alone pulls its explicit
// `Collections.Core` dependency together with the existing Xml closure.
#include <cstdlib>
#include <string>

#include "System/Xml/Serialization/XmlSerializer.hpp"

namespace {

struct Entity {
    std::string name;
    float x = 0;
    SHARP_XML_SERIALIZABLE(Entity, "Entity", SHARP_XML_M(Entity, name), SHARP_XML_M(Entity, x))
};

struct EntityList {
    System::Collections::Generic::List<Entity> entities;
    SHARP_XML_SERIALIZABLE(EntityList, "EntityList", SHARP_XML_M(EntityList, entities))
};

}  // namespace

int main() {
    EntityList list;
    list.entities.Add({"spawn0", 1.5f});

    System::Xml::Serialization::XmlSerializer<EntityList> serializer;
    const std::string xml = serializer.Serialize(list);
    const EntityList back = serializer.Deserialize(xml);

    const bool ok = back.entities.getCountProperty() == 1 &&
                    back.entities.getItem(0).name == "spawn0" &&
                    back.entities.getItem(0).x == 1.5f &&
                    xml.find("<EntityList xmlns:xsi=") != std::string::npos;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
