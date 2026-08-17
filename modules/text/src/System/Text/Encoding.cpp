// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/Encoding.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/ASCIIEncoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"
#include "System/Text/UTF32Encoding.hpp"
#include "System/Text/UTF7Encoding.hpp"
#include "System/Text/Latin1Encoding.hpp"

namespace System::Text
{
    std::shared_ptr<Encoding> Encoding::UTF8()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<UTF8Encoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::ASCII()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<ASCIIEncoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::Unicode()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<UnicodeEncoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::BigEndianUnicode()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<UnicodeEncoding>(true, true);
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::UTF32()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<UTF32Encoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::UTF7()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<UTF7Encoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::shared_ptr<Encoding> Encoding::Latin1()
    {
        static std::shared_ptr<Encoding> instance = [] {
            auto e = std::make_shared<Latin1Encoding>();
            // #2013: the seven factory instances are READ-ONLY, matching .NET's
            // ASCIIEncoding.s_default and its siblings. A caller that wants to install a
            // fallback calls Clone() first; before this, mutating one of these changed what
            // every other caller in the process decoded, with a measured race against a
            // concurrent conversion.
            e->markReadOnly();
            return e;
        }();
        return instance;
    }

    std::vector<SharpRuntime::bytecs> Encoding::GetBytes(const std::string& str) const
    {
        return std::vector<SharpRuntime::bytecs>(str.begin(), str.end());
    }

    std::string Encoding::GetString(
        const SharpRuntime::bytecs* data,
        SharpRuntime::intcs index,
        SharpRuntime::intcs count) const
    {
        // Ticket #2007 (SR-AUD-286): the previous `data == nullptr || count <= 0` guard
        // silently accepted a NEGATIVE index and formed `data + index`, so
        // GetString(p, -1, 1) read the byte before the caller's buffer. The shared
        // validator rejects it, and every valid triple reaches the identical construction.
        const auto range = detail::checkedRawDecodeRange(data, index, count);
        if (!range.any())
        {
            return {};
        }

        return std::string(
            reinterpret_cast<const char*>(data) + range.begin,
            range.end - range.begin);
    }
}