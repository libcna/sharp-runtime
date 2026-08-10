// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2213 (family CMS-A of the modules/core
// memory-safety family, finding SR-AUD-051).
//
// System::Buffer is a byte-oriented API: its four generic members reinterpret a
// std::vector's element storage as raw bytes. Their doc-comments always said the
// element type had to be primitive / trivially copyable, and nothing enforced it,
// so `std::vector<std::string>` compiled and reached `memmove`. That duplicated
// each string's owning pointer and produced an AddressSanitizer-confirmed
// DOUBLE-FREE at vector destruction (build-probe/2210_before.log,
// case `san.buffer_typed_vector_nontrivial`).
//
// Real .NET rejects the same call at run time -- Buffer.BlockCopy throws
// ArgumentException("Object must be an array of primitives.") -- but in C++ the
// element type is known at compile time, so the faithful counterpart is to refuse
// to compile. #2213 added one shared `requireTriviallyCopyable<T>()` to all four
// members. THIS FILE IS THE PROOF THAT THE REJECTION IS REAL: a `static_assert`
// placed in a body that is never instantiated silently enforces nothing, and only
// a per-site compile can tell the difference.
//
// Migration for a caller this rejects: copy elements with System::Array::Copy,
// which is generic over any T and (since #2213) overlap-safe; or convert to a
// std::vector<SharpRuntime::bytecs> first and use Buffer's byte overload. There is
// no spelling of the old call that is both accepted and correct, which is the
// point -- every accepted-before call with an owning element type was corrupting
// memory.
//
// The `#else` branches carry the accepted spelling: the same member with a
// trivially copyable element type. Those are positive claims and are additionally
// pinned at run time by CoreMemorySafetyBufferGenericTests in Core_Base.
//
// NEGATIVE-FIXTURE: component=Core.Base
#include <string>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Buffer.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

int main() {
    std::vector<std::string> owning{"a", "b"};
    std::vector<int> plain{1, 2};

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(buffer-blockcopy-owning-element): element type must be trivially copyable
    //     | static assertion failed
    {
        std::vector<std::string> owningDst{"c", "d"};
        System::Buffer::BlockCopy<std::string>(owning, 0, owningDst, 0,
                                               static_cast<intcs>(sizeof(std::string)));
    }
#else
    {
        std::vector<int> plainDst{0, 0};
        System::Buffer::BlockCopy<int>(plain, 0, plainDst, 0,
                                       static_cast<intcs>(sizeof(int)));
        (void)plainDst;
    }
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(buffer-bytelength-owning-element): element type must be trivially copyable
    //     | static assertion failed
    (void)System::Buffer::ByteLength<std::string>(owning);
#else
    (void)System::Buffer::ByteLength<int>(plain);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(buffer-getbyte-owning-element): element type must be trivially copyable
    //     | static assertion failed
    (void)System::Buffer::GetByte<std::string>(owning, 0);
#else
    (void)System::Buffer::GetByte<int>(plain, 0);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(buffer-setbyte-owning-element): element type must be trivially copyable
    //     | static assertion failed
    System::Buffer::SetByte<std::string>(owning, 0, static_cast<bytecs>(1));
#else
    System::Buffer::SetByte<int>(plain, 0, static_cast<bytecs>(1));
#endif

    (void)owning;
    (void)plain;
    return 0;
}
