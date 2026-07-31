// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/28/25.
//

#include "System/Random.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

#include <atomic>
#include <new>

namespace System {

    // -----------------------------------------------------------------------
    // Knuth "subtractive" generator -- exact port of .NET's Random.CompatPrng
    // (Random.CompatImpl.cs). See the class doc-comment in Random.hpp for why
    // this specific algorithm was chosen over a faster generic PRNG.
    //
    // C#'s default arithmetic is *unchecked*: int subtraction/addition silently wraps
    // modulo 2^32 on overflow rather than throwing, and this algorithm's intermediate
    // values (e.g. the raw, never-corrected seedArray_[55] seed) rely on that wraparound
    // to reach the correct final state. Plain int32_t arithmetic would invoke C++ signed-
    // overflow UB for the same inputs, so every subtraction below is done in uint32_t
    // (where wraparound is guaranteed, defined behavior) and cast back with static_cast,
    // which C++20 guarantees reproduces the identical two's-complement bit pattern C#
    // produces -- i.e. bit-for-bit the same result, not just "no crash".
    // -----------------------------------------------------------------------

    namespace {
        inline int32_t wrappingSub(int32_t a, int32_t b) {
            return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
        }

        // -------------------------------------------------------------------
        // Ownership boundary for the shared instance
        // (CCF-009 / SR-AUD-010, ticket #1902).
        //
        // getSharedProperty() returns a *reference*, so the object it names has
        // to stay one object with one stable address on every thread. .NET
        // rejects the obvious shortcut -- a per-thread instance -- explicitly:
        // "It's also possible that one thread could retrieve Shared and pass it
        // to another thread, so Shared can't just access a ThreadStatic to
        // return a Random instance stored there" (Random.cs, lines 755-759).
        // The shared *object* therefore stays shared, and the mutable generator
        // *state* it draws from becomes per-thread: every generator entry point
        // in this class funnels through internalSample(), which is the only
        // method that writes seedArray_/inext_/inextp_, so routing there covers
        // the whole public surface -- including the header-inline templates and
        // the protected Sample() seam -- with one test.
        //
        // The check is a pointer comparison, deliberately: a `bool isShared_`
        // member would have changed sizeof(Random), and Random's state lives in
        // the public header.
        //
        // The address is published by getSharedProperty()'s initialiser and read
        // by internalSample() on other threads, so it is atomic. It stays null
        // for the duration of the shared instance's own construction, which is
        // intentional -- a generator call made from inside that construction
        // takes the ordinary path on the real object rather than re-entering the
        // static initialisation it is already inside.
        std::atomic<const Random*> sharedInstanceAddress{nullptr};

        /**
         * @brief Produces one distinct seed per thread for the shared instance's
         *        stand-in generator.
         *
         * Not derived from std::random_device alone: on a platform whose
         * random_device is deterministic (MinGW-w64's historically was) every
         * thread would seed identically and produce the identical stream, which
         * is the failure §6 of docs/SharedPrngConcurrencyPlan.md warns about.
         * `base` supplies process entropy where it exists; the counter
         * guarantees distinctness where it does not.
         */
        intcs nextThreadSeed() {
            static const uint32_t base = std::random_device{}();
            static std::atomic<uint32_t> counter{0};
            const uint32_t n = counter.fetch_add(1, std::memory_order_relaxed);
            // Reduced to 31 bits because initializeSeed() takes |seed|, so a
            // negative seed would collide with its positive twin. Multiplying by
            // an odd constant is a bijection modulo 2^31, and reducing a mod-2^32
            // computation modulo 2^31 is the same as computing it modulo 2^31,
            // so distinct counter values always yield distinct seeds.
            return static_cast<intcs>((base + n * 2654435761u) & 0x7FFFFFFFu);
        }

        /**
         * @brief The calling thread's own generator, standing in for the shared
         *        instance's state.
         *
         * A full Random, so the per-thread stream is produced by exactly the
         * same ported algorithm rather than a second one kept in sync by hand.
         *
         * Constructed in thread-local storage and deliberately never destroyed.
         * A plain `thread_local Random` would be destroyed at thread exit, which
         * would leave any code that draws from the shared instance *during*
         * thread teardown -- another thread_local's destructor, say -- holding a
         * reference to a destroyed object. The old process-wide instance had no
         * such window, so the repair must not open one. Skipping the destructor
         * leaks nothing: the storage is thread-local, not heap, and Random owns
         * no resource. It also keeps this translation unit free of a
         * `__cxa_thread_atexit` dependency, which matters for the compile-only
         * MinGW and Emscripten targets.
         */
        Random& sharedStateForThisThread() {
            alignas(Random) static thread_local unsigned char storage[sizeof(Random)];
            static thread_local Random* state = nullptr;
            if (state == nullptr) state = ::new (static_cast<void*>(storage)) Random(nextThreadSeed());
            return *state;
        }
    }

    void Random::initializeSeed(intcs seed) {
        int32_t subtraction = (seed == std::numeric_limits<int32_t>::min())
            ? std::numeric_limits<int32_t>::max()
            : std::abs(static_cast<int32_t>(seed));
        int32_t mj = 161803398 - subtraction;
        seedArray_[55] = mj;
        int32_t mk = 1;

        int ii = 0;
        for (int i = 1; i < 55; i++) {
            // The range [1..55] is special (Knuth) and so we're wasting the 0'th position.
            ii += 21;
            if (ii >= 55) ii -= 55;

            seedArray_[static_cast<size_t>(ii)] = mk;
            mk = wrappingSub(mj, mk);
            if (mk < 0) mk += std::numeric_limits<int32_t>::max();

            mj = seedArray_[static_cast<size_t>(ii)];
        }

        for (int k = 1; k < 5; k++) {
            for (int i = 1; i < 56; i++) {
                int n = i + 30;
                if (n >= 55) n -= 55;

                seedArray_[static_cast<size_t>(i)] =
                    wrappingSub(seedArray_[static_cast<size_t>(i)], seedArray_[static_cast<size_t>(1 + n)]);
                if (seedArray_[static_cast<size_t>(i)] < 0)
                    seedArray_[static_cast<size_t>(i)] += std::numeric_limits<int32_t>::max();
            }
        }

        inext_ = 0;
        inextp_ = 21;
    }

    int32_t Random::internalSample() {
        // The shared instance owns no mutable state of its own -- see the
        // ownership-boundary comment above. This is not recursion: the
        // stand-in is a different object, so it falls through to the body
        // below. Its cost is one acquire load and one predictable branch per
        // draw, paid by every generator call including per byte of NextBytes;
        // hoisting it out of the loops would mean a new private member
        // function, i.e. a public-header edit, which this repair avoids.
        if (this == sharedInstanceAddress.load(std::memory_order_acquire))
            return sharedStateForThisThread().internalSample();

        int locINext = inext_;
        if (++locINext >= 56) locINext = 1;

        int locINextp = inextp_;
        if (++locINextp >= 56) locINextp = 1;

        int32_t retVal = wrappingSub(seedArray_[static_cast<size_t>(locINext)], seedArray_[static_cast<size_t>(locINextp)]);

        if (retVal == std::numeric_limits<int32_t>::max()) retVal--;
        if (retVal < 0) retVal += std::numeric_limits<int32_t>::max();

        seedArray_[static_cast<size_t>(locINext)] = retVal;
        inext_ = locINext;
        inextp_ = locINextp;

        return retVal;
    }

    double Random::getSampleForLargeRange() {
        // The distribution of the double returned by prngSample() is not good enough for a
        // large range. If we used it directly for a range like [int.MinValue..int.MaxValue),
        // we'd end up getting even numbers only.
        int32_t result = internalSample();

        // Can't use addition here: the distribution would be bad if we did. Decide the sign
        // based on a second sample instead.
        if (internalSample() % 2 == 0) result = -result;

        double d = result;
        d += std::numeric_limits<int32_t>::max() - 1; // range [0..2*int.MaxValue-1)
        d /= 2.0 * static_cast<double>(std::numeric_limits<int32_t>::max()) - 1;
        return d;
    }

    uint64_t Random::nextUInt64() {
        return (static_cast<uint64_t>(static_cast<uint32_t>(Next(1 << 22)))) |
               (static_cast<uint64_t>(static_cast<uint32_t>(Next(1 << 22))) << 22) |
               (static_cast<uint64_t>(static_cast<uint32_t>(Next(1 << 20))) << 44);
    }

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    Random::Random() {
        initializeSeed(static_cast<intcs>(std::random_device{}()));
    }

    Random::Random(intcs seed) {
        initializeSeed(seed);
    }

    // -----------------------------------------------------------------------
    // Int32
    // -----------------------------------------------------------------------

    intcs Random::Next()
    {
        return internalSample();
    }

    intcs Random::Next(intcs maxValue)
    {
        if (maxValue < 0)
        {
            throw ArgumentOutOfRangeException("maxValue");
        }

        return static_cast<intcs>(prngSample() * static_cast<double>(maxValue));
    }

    intcs Random::Next(intcs minValue, intcs maxValue)
    {
        if (minValue > maxValue)
        {
            throw ArgumentOutOfRangeException("minValue");
        }

        int64_t range = static_cast<int64_t>(maxValue) - static_cast<int64_t>(minValue);
        if (range <= std::numeric_limits<int32_t>::max())
        {
            return static_cast<intcs>(prngSample() * static_cast<double>(range)) + minValue;
        }
        return static_cast<intcs>(static_cast<int64_t>(getSampleForLargeRange() * static_cast<double>(range)) + minValue);
    }

    double Random::NextDouble()
    {
        return prngSample();
    }

    float Random::NextSingle()
    {
        for (;;)
        {
            float f = static_cast<float>(prngSample());
            if (f < 1.0f) return f; // reject 1.0f, which is rare but possible due to rounding
        }
    }

    // -----------------------------------------------------------------------
    // Int64
    // -----------------------------------------------------------------------

    longcs Random::NextInt64()
    {
        for (;;)
        {
            // Get top 63 bits to get a value in [0, long.MaxValue], but retry if it's actually
            // long.MaxValue, since this method is defined to return a value in [0, long.MaxValue).
            uint64_t result = nextUInt64() >> 1;
            if (result != static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                return static_cast<longcs>(result);
        }
    }

    longcs Random::NextInt64(longcs maxValue)
    {
        if (maxValue < 0)
            throw ArgumentOutOfRangeException("maxValue");
        return NextInt64(0LL, maxValue);
    }

    longcs Random::NextInt64(longcs minValue, longcs maxValue)
    {
        if (minValue > maxValue)
            throw ArgumentOutOfRangeException("minValue");

        // Computed entirely in unsigned space to avoid signed-overflow UB when the range
        // spans more than half of intcs's representable values (e.g. minValue near
        // LONGCS_MIN, maxValue near LONGCS_MAX) -- matches C#'s `(ulong)(maxValue - minValue)`,
        // which relies on defined-in-C# (but UB in C++ if done in signed arithmetic) wraparound.
        uint64_t exclusiveRange = static_cast<uint64_t>(maxValue) - static_cast<uint64_t>(minValue);

        if (exclusiveRange > 1)
        {
            // Narrow down to the smallest range [0, 2^bits] that contains maxValue - minValue,
            // then repeatedly generate a value in that outer range until one falls in the inner
            // range.
            int bits = 0;
            {
                uint64_t v = exclusiveRange - 1;
                while (v > 0) { ++bits; v >>= 1; }
                if (bits == 0) bits = 1;
            }
            for (;;)
            {
                uint64_t result = nextUInt64() >> (64 - bits);
                if (result < exclusiveRange)
                    return static_cast<longcs>(result + static_cast<uint64_t>(minValue));
            }
        }

        return minValue;
    }

    // -----------------------------------------------------------------------
    // Byte buffers
    // -----------------------------------------------------------------------

    void Random::NextBytes(std::vector<bytecs>& buffer)
    {
        for (auto& b : buffer)
            b = static_cast<bytecs>(internalSample());
    }

    void Random::NextBytes(Span<bytecs> buffer)
    {
        for (intcs i = 0; i < buffer.getLengthProperty(); ++i)
            buffer[i] = static_cast<bytecs>(internalSample());
    }

    Random& Random::getSharedProperty()
    {
        static Random instance;
        // Published after `instance` is fully constructed and before this
        // function can hand its address to any thread, so every internalSample()
        // that can observe the shared instance observes the published address
        // too. Once published, `instance` is never written again.
        [[maybe_unused]] static const bool addressPublished = [] {
            sharedInstanceAddress.store(&instance, std::memory_order_release);
            return true;
        }();
        return instance;
    }

    std::string Random::GetString(const std::string& choices, intcs length)
    {
        if (choices.empty()) throw ArgumentException("Span may not be empty.", "choices");
        if (length < 0) throw ArgumentOutOfRangeException("length", "'length' must be a non-negative value.");
        if (length == 0) return {};

        std::string result(static_cast<std::size_t>(length), '\0');
        GetItems(ReadOnlySpan<char>(choices.data(), static_cast<intcs>(choices.size())),
                 Span<char>(result.data(), length));
        return result;
    }

    std::string Random::GetHexString(intcs stringLength, bool lowercase)
    {
        static constexpr const char* upper = "0123456789ABCDEF";
        static constexpr const char* lower = "0123456789abcdef";
        return GetString(lowercase ? lower : upper, stringLength);
    }

    void Random::GetHexString(Span<char> destination, bool lowercase)
    {
        static constexpr const char* upper = "0123456789ABCDEF";
        static constexpr const char* lower = "0123456789abcdef";
        const char* chars = lowercase ? lower : upper;
        GetItems(ReadOnlySpan<char>(chars, 16), destination);
    }

} // namespace System
