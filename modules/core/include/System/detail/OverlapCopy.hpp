// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>

namespace System::detail {

/**
 * @brief Copies @p count elements from @p source to @p destination, preserving the whole
 * source even when the two ranges overlap.
 *
 * This is the port's counterpart of the `Buffer.Memmove` that every .NET array/span copy
 * ultimately calls. .NET's documented contract for `Span<T>.CopyTo`, `Array.Copy` and
 * friends is that the copy behaves *as if* the source had been staged in a temporary
 * first, so an overlapping copy is well defined and never loses data.
 *
 * A plain forward `std::copy` does **not** implement that contract. When the destination
 * starts strictly inside the source range, the first assignments overwrite source
 * elements that later assignments still have to read, and the tail of the source is lost:
 * copying the first three of `{a,b,c,d}` one place right produced `aaaa` instead of
 * `aabc` at **twelve** public doors (SR-AUD-044). Copying the other way — destination
 * before source — is already correct forward, and was measured correct, so the direction
 * must be *selected* rather than unconditionally reversed.
 *
 * `std::copy_backward` handles the right-overlap case with ordinary element assignment,
 * so unlike `std::memmove` this is correct for element types that own resources, and for
 * trivially copyable types the compiler lowers both branches to the same block move.
 *
 * @note The overlap test uses `std::less`, not the built-in `<`. Relational comparison of
 * pointers into different objects is *unspecified* in ISO C++, while `std::less` is
 * required to yield a total order over pointer values — so the test is well defined even
 * when the two ranges are unrelated, which is the common case.
 *
 * @tparam T The element type.
 * @param source      First element to read; may be null when @p count is zero.
 * @param count       Number of elements to copy.
 * @param destination First element to write; may be null when @p count is zero.
 *
 * @see docs/CoreMemorySafetyFamilyPlan.md (family CMS-C)
 */
template<typename T>
inline void copyOverlapAware(const T* source, std::size_t count, T* destination) {
    // An empty copy, or a copy onto itself, is a no-op. Returning early also keeps the
    // std::copy call below within its precondition, which forbids an output iterator
    // inside [first, last).
    if (count == 0 || source == destination) return;

    const T* const sourceEnd = source + count;
    const std::less<const T*> before{};

    if (before(source, destination) && before(destination, sourceEnd)) {
        // The destination starts inside the source range: assigning forward would
        // clobber source elements before they are read. Assign from the back instead.
        std::copy_backward(source, sourceEnd, destination + count);
    } else {
        // Disjoint, adjacent, or destination-before-source: forward assignment already
        // reads every element before anything can overwrite it.
        std::copy(source, sourceEnd, destination);
    }
}

} // namespace System::detail
