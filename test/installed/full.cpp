// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/MemoryStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/ZipArchive.hpp"
#include "System/Text/Json/JsonDocument.hpp"
#include "System/Xml/XmlDocument.hpp"
#include <memory>

int main()
{
    using namespace System::IO;
    using namespace System::IO::Compression;
    const auto json = System::Text::Json::JsonDocument::Parse("42");
    if (json->getRootElementProperty().GetInt32() != 42) return 1;
    System::Xml::XmlDocument xml;
    xml.LoadXml("<root>installed</root>");
    if (xml.getDocumentElementProperty()->getInnerTextProperty() != "installed") return 2;

    MemoryStream deflated;
    {
        DeflateStream writer(&deflated, CompressionMode::Compress, true);
        writer.WriteByte(42);
        writer.Close();
    }
    deflated.setPositionProperty(0);
    DeflateStream reader(&deflated, CompressionMode::Decompress, true);
    if (reader.ReadByte() != 42 || reader.ReadByte() != -1) return 3;

    MemoryStream zipped;
    {
        ZipArchive archive(&zipped, ZipArchiveMode::Create);
        auto entry = archive.CreateEntry("value");
        const std::unique_ptr<Stream> output(entry.Open());
        output->WriteByte(43);
        archive.Dispose();
    }
    zipped.setPositionProperty(0);
    ZipArchive archive(&zipped, ZipArchiveMode::Read);
    auto entry = archive.GetEntry("value");
    const std::unique_ptr<Stream> input(entry.Open());
    return input->ReadByte() == 43 && input->ReadByte() == -1 ? 0 : 4;
}
