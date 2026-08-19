// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2059 (SR-AUD-088, cause B-D).
//
// #2059 made System::Buffers::MemoryHandle's two components private, matching .NET's
// `private void* _pointer` / `private IPinnable? _pinnable` (MemoryHandle.cs:14-16).
// .NET publishes exactly one of them, `Pointer`, and only as a getter. This port
// published both as mutable data members, so a caller could retarget a live handle at
// an unrelated address, or detach its IPinnable and leak the pin, behind the owner's
// back -- and could then call Dispose() on a handle whose pointer no longer matched
// what was actually pinned.
//
// Nothing in this repository ever touched them -- measured, ZERO first-party direct
// accesses -- so there was no first-party migration. What a CONSUMER loses is the
// spellings that reach a public data member. Each is compiled on its own below; the
// #else branches are the migrated spellings.
//
// Migration: build handles with the two-argument constructor (or take one from
// IPinnable::Pin) and read the address with getPointerProperty(). There is no
// replacement for reading or writing pinnable_, deliberately: .NET exposes no such
// accessor, and a caller that needs to release the pin calls Dispose().
//
// NOTE what this fixture does NOT claim. #2059 also DECLINED to add
// `~MemoryHandle(){ Dispose(); }`, because .NET's MemoryHandle is a struct with no
// finalizer and does not unpin at scope exit either. That absence is parity, not a gap,
// and it is pinned inside the repository by MemoryHandlePinTests rather than here --
// a fixture can only prove that a spelling is rejected, not that a behaviour is absent.
//
// Records: docs/Migration-MemoryHandlePrivateRepresentation.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Buffers
#include <type_traits>

#include "System/Buffers/IPinnable.hpp"
#include "System/Buffers/MemoryHandle.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Buffers::IPinnable;
using System::Buffers::MemoryHandle;

namespace {
struct CountingPinnable final : IPinnable {
    int value = 7;
    MemoryHandle Pin(SharpRuntime::intcs) override { return MemoryHandle(&value, this); }
    void Unpin() override {}
};
} // namespace

int main() {
    CountingPinnable pinnable;
    MemoryHandle     handle = pinnable.Pin(0);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(memoryhandle-direct-pointer-write): is private within this context
    //     | private
    handle.pointer_ = nullptr;
#else
    handle = MemoryHandle(nullptr);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(memoryhandle-direct-pointer-read): is private within this context
    //     | private
    void* raw = handle.pointer_;
    (void)raw;
#else
    void* raw = handle.getPointerProperty();
    (void)raw;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(memoryhandle-detach-pinnable): is private within this context
    //     | private
    // THE SITE THAT BREAKS SILENTLY RATHER THAN LOUDLY, and the reason the member is
    // private at all: detaching the IPinnable makes the subsequent Dispose() a no-op, so
    // the pin leaks with no diagnostic anywhere. It compiles, it runs, and it is wrong.
    handle.pinnable_ = nullptr;
#else
    // There is no migrated spelling, by design -- releasing the pin is what Dispose() is.
    handle.Dispose();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(memoryhandle-aggregate-init): no matching function
    //     | private
    //     | could not convert
    //     | cannot convert
    MemoryHandle braced{&pinnable.value, &pinnable, nullptr};
    (void)braced;
#else
    MemoryHandle braced{&pinnable.value, &pinnable};
    braced.Dispose();
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: both public
    // constructors, the getter, copyability, and the size -- this is an access change and
    // moves no layout, so no consumer needs a rebuild for it.
    const MemoryHandle defaulted{};
    MemoryHandle       fromPointer(&pinnable.value);
    MemoryHandle       copied = fromPointer;
    static_assert(std::is_copy_constructible_v<MemoryHandle>, "still copyable");
    static_assert(sizeof(MemoryHandle) == 24, "#2059 moved no layout");
    copied.Dispose();
    fromPointer.Dispose();
    return (defaulted.getPointerProperty() == nullptr) ? 0 : 1;
}
