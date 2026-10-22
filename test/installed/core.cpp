// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "SharpRuntime/Version.hpp"
#include "System/Guid.hpp"

// Even a full installation must retain Core.Base's narrow public header closure.
#if __has_include("System/IO/MemoryStream.hpp") || __has_include("System/Text/Json/JsonDocument.hpp")
#error Core.Base has acquired unrelated component headers
#endif

int main()
{
    static_assert(!SharpRuntime::getVersionString().empty());
    const auto id = System::Guid::Parse("00112233-4455-6677-8899-aabbccddeeff");
    return id.ToString() == "00112233-4455-6677-8899-aabbccddeeff" ? 0 : 1;
}
