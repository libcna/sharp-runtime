// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::IO::Hashing::Detail {

    using SharpRuntime::bytecs;
    using SharpRuntime::uintcs;
    using SharpRuntime::ulongcs;

    /**
     * @brief The module's one definition of what "little-endian" means.
     *
     * xxHash is specified over **little-endian** lane loads: the published check values are a
     * property of the algorithm, not of the machine it runs on. The functions this header
     * replaces were named `ReadUInt32LE`/`ReadUInt64LE`/`WriteUInt64LE` and implemented as a
     * `memcpy` of a native integer, which is the same thing only on a little-endian host. On a
     * big-endian target every lane would be byte-swapped and the module would compute hashes that
     * are not xxHash — silently, and only there.
     *
     * These helpers assemble the value from its bytes, so the byte order is a property of the
     * code rather than of the target. On a little-endian host the shift-or pattern compiles back
     * to a single load, so nothing is given up for it.
     *
     * **What is proved here and what is not.** These are testable *by construction*: a fixed byte
     * pattern must map to one specific numeric value, an assertion that holds regardless of the
     * host, and `HashingTests` pins exactly that. What is **not** demonstrated anywhere in this
     * repository is that the module computes correct hashes when **executed** on a big-endian
     * machine — there is no such host here and no cross-run in this repository's CI. That is an
     * environment limit, not a gap in the reasoning, and it is stated rather than papered over.
     */

    /** Reads four bytes at @p source as a little-endian 32-bit value, on any host. */
    [[nodiscard]] inline uintcs ReadUInt32LittleEndian(const bytecs* source) {
        return static_cast<uintcs>(source[0])
             | (static_cast<uintcs>(source[1]) << 8)
             | (static_cast<uintcs>(source[2]) << 16)
             | (static_cast<uintcs>(source[3]) << 24);
    }

    /** Reads eight bytes at @p source as a little-endian 64-bit value, on any host. */
    [[nodiscard]] inline ulongcs ReadUInt64LittleEndian(const bytecs* source) {
        return static_cast<ulongcs>(ReadUInt32LittleEndian(source))
             | (static_cast<ulongcs>(ReadUInt32LittleEndian(source + 4)) << 32);
    }

    /** Writes @p value to @p destination as eight little-endian bytes, on any host. */
    inline void WriteUInt64LittleEndian(bytecs* destination, ulongcs value) {
        for (int i = 0; i < 8; ++i) {
            destination[i] = static_cast<bytecs>(value >> (8 * i));
        }
    }

} // namespace System::IO::Hashing::Detail
