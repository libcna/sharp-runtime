// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    /**
     * @brief Represents a position in a non-contiguous set of memory.
     *
     * C++ counterpart of .NET System.SequencePosition.
     * A position is defined by a segment object pointer and an integer offset within
     * that segment. For single-segment sequences, the object pointer is typically
     * null and the integer is an absolute byte offset.
     *
     * @note <b>The two components are public and mutable here; .NET's are private and
     * readonly.</b> .NET documents that the parts of a position must not be interpreted
     * by anything except the sequence that created it; this port cannot enforce that,
     * because a caller can assign `object_` or `integer_` directly after the sequence
     * handed the position out. Nothing inside this repository does: every other use --
     * ReadOnlySequence, SequenceReader, BuffersExtensions and their suites -- goes
     * through the constructor and through GetObject()/GetInteger(), and the only direct
     * field access anywhere is inside this type. Closing the hole means making the two
     * members private, which is a public source break for any consumer that does touch
     * them; that decision is open and is not taken here.
     */
    struct SequencePosition {
        /** @brief The segment object (may be null for single-segment sequences). */
        void* object_ = nullptr;
        /** @brief The integer offset within the segment. */
        SharpRuntime::intcs integer_ = 0;

        /** @brief Default constructor — represents position zero with no segment. */
        SequencePosition() = default;

        /**
         * @brief Constructs a SequencePosition from a segment object and integer offset.
         * @param object  Pointer to the segment object (may be null).
         * @param integer Integer offset within the segment.
         */
        SequencePosition(void* object, SharpRuntime::intcs integer) noexcept
            : object_(object), integer_(integer) {}

        /** @brief Returns the segment object pointer. */
        [[nodiscard]] void* GetObject() const noexcept { return object_; }
        /** @brief Returns the integer offset. */
        [[nodiscard]] SharpRuntime::intcs GetInteger() const noexcept { return integer_; }

        /**
         * @brief Indicates whether this position has the same components as @p other.
         *
         * C++ counterpart of .NET SequencePosition.Equals(SequencePosition), and the
         * body `operator==` delegates to, so the named and the operator form can never
         * disagree.
         *
         * @note Component equality is <b>not</b> sequence-location identity. Two
         * positions are equal here exactly when their segment pointer and their integer
         * are equal; nothing checks that they were produced by, or are meaningful to,
         * the same sequence. .NET documents the same limitation from the other side --
         * the components must not be interpreted by anything except the creator of the
         * position -- so a position from one sequence that happens to carry the same
         * pair as a position from another compares equal to it.
         */
        [[nodiscard]] bool Equals(const SequencePosition& other) const noexcept {
            return object_ == other.object_ && integer_ == other.integer_;
        }

        /**
         * @brief Returns a hash code for this position.
         *
         * C++ counterpart of .NET SequencePosition.GetHashCode(). It satisfies the one
         * contract callers may rely on: two positions for which Equals() is true always
         * hash the same. It is <b>not</b> .NET's own hash value -- .NET combines the
         * segment object's managed hash with the integer, and this port has neither a
         * managed object nor a readable copy of that combiner (`/rv` is absent), so the
         * segment pointer's own bits are used instead. Do not persist or compare these
         * values across processes or builds.
         *
         * @note The combine is evaluated in `uintcs`, where overflow is defined, and
         * converted once at the end; written directly in `intcs` the addition would be
         * signed-overflow undefined behaviour (the CCF-004 class already recorded for
         * `System::detail::tupleHashCombine`).
         */
        [[nodiscard]] SharpRuntime::intcs GetHashCode() const noexcept {
            const std::uintptr_t bits = reinterpret_cast<std::uintptr_t>(object_);
            // Fold the high half in only where there IS a high half: on a target whose
            // pointer is no wider than uintcs, `bits >> 32` would be a shift at or past
            // the operand width, which is undefined behaviour rather than zero.
            SharpRuntime::uintcs h1;
            if constexpr (sizeof(std::uintptr_t) > sizeof(SharpRuntime::uintcs)) {
                h1 = static_cast<SharpRuntime::uintcs>(bits ^ (bits >> 32));
            } else {
                h1 = static_cast<SharpRuntime::uintcs>(bits);
            }
            const SharpRuntime::uintcs h2 = static_cast<SharpRuntime::uintcs>(integer_);
            return static_cast<SharpRuntime::intcs>(((h1 << 5) + h1) ^ h2);
        }

        /** @brief Returns true if both positions refer to the same location. */
        [[nodiscard]] bool operator==(const SequencePosition& o) const noexcept {
            return Equals(o);
        }
        /** @brief Returns true if the positions differ. */
        [[nodiscard]] bool operator!=(const SequencePosition& o) const noexcept {
            return !Equals(o);
        }
    };

} // namespace System
