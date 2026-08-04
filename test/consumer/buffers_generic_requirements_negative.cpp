// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2054 (family B-C of the System::Buffers
// namespace review, findings SR-AUD-070 and SR-AUD-077).
//
// #2054 did NOT change what compiles. Six public System::Buffers generic
// surfaces already required more of `T` than their documentation stated --
// default-constructibility through `std::vector<T>`, copy-assignability through
// `assign`/`fill`, and a usable `std::hash<T>` through `std::unordered_set<T>`
// -- and every one of those requirements was enforced only by an error deep
// inside libstdc++ ("no matching function for call to 'construct_at'",
// "'std::__hash_enum<...>::~__hash_enum()' is private within this context").
// The ticket states each requirement in the header and adds a `static_assert`
// AT THE POINT WHERE IT WAS ALREADY ENFORCED.
//
// That makes this fixture's job unusual and worth stating plainly: the sites
// below were ALREADY rejected before #2054. What the fixture pins is the pair
// of claims that make the change safe --
//
//   1. every one of them is STILL rejected (the assert did not accidentally
//      relax a requirement, e.g. by being placed in a body that is never
//      instantiated), and
//   2. it is rejected BY THE NEW MESSAGE, not by the old libstdc++ noise, which
//      is the only observable difference the ticket claims.
//
// The `#else` branches carry the accepted spelling: the same call with a `T`
// that meets the requirement. Nothing here is a migration -- no caller has to
// change -- so the `#else` branches are what a caller writes today and will
// keep writing.
//
// The converse claim -- that naming `ArrayBufferWriter<NoDefault>`,
// `ArrayPool<NoDefault>`, `MemoryPool<NoDefault>`, `SearchValues<EqualityOnly>`
// and `SequenceReader<NoDefault>`, and taking their `sizeof`, all remain LEGAL
// -- is a positive claim and cannot live in a negative fixture. It is pinned by
// the baseline below (which the checker compiles with zero diagnostics) and by
// BuffersGenericRequirementsTests.cpp in the Buffers suite. The measured
// before/after table is docs/BuffersNamespaceReviewPlan.md §23.4.
//
// NEGATIVE-FIXTURE: component=Buffers
#include <cstddef>
#include <vector>

#include "System/Buffers/ArrayBufferWriter.hpp"
#include "System/Buffers/ArrayPool.hpp"
#include "System/Buffers/MemoryPool.hpp"
#include "System/Buffers/ReadOnlySequence.hpp"
#include "System/Buffers/SearchValues.hpp"
#include "System/Buffers/SequenceReader.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Buffers::ArrayBufferWriter;
using System::Buffers::ArrayPool;
using System::Buffers::MemoryPool;
using System::Buffers::ReadOnlySequence;
using System::Buffers::SearchValues;
using System::Buffers::SequenceReader;

/** Copyable and equality-comparable, but NOT default-constructible. */
struct NoDefault {
    int value;
    explicit NoDefault(int initial) : value(initial) {}
    NoDefault(const NoDefault&) = default;
    NoDefault& operator=(const NoDefault&) = default;
    bool operator==(const NoDefault& other) const { return value == other.value; }
};

/** Default-constructible and copy-constructible, but NOT copy-assignable. */
struct NoCopyAssign {
    int value = 0;
    NoCopyAssign() = default;
    NoCopyAssign(const NoCopyAssign&) = default;
    NoCopyAssign& operator=(const NoCopyAssign&) = delete;
    NoCopyAssign& operator=(NoCopyAssign&&) = delete;
};

/**
 * Default-constructible and equality-comparable, with NO std::hash
 * specialization -- exactly the shape .NET's `IEquatable<T>` constraint admits
 * and this port cannot store.
 */
struct EqualityOnly {
    int value = 0;
    EqualityOnly() = default;
    explicit EqualityOnly(int initial) : value(initial) {}
    bool operator==(const EqualityOnly& other) const { return value == other.value; }
};

/** The same shape, plus the std::hash specialization this port needs. */
struct EqualityAndHash {
    int value = 0;
    EqualityAndHash() = default;
    explicit EqualityAndHash(int initial) : value(initial) {}
    bool operator==(const EqualityAndHash& other) const { return value == other.value; }
};

template<>
struct std::hash<EqualityAndHash> {
    std::size_t operator()(const EqualityAndHash& key) const noexcept {
        return static_cast<std::size_t>(key.value);
    }
};

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(arraybufferwriter-capacity-ctor-nondefault): ArrayBufferWriter<T>(intcs) requires T to be default-constructible
    ArrayBufferWriter<NoDefault> writer(4);
    (void)writer;
#else
    ArrayBufferWriter<int> writer(4);
    (void)writer;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(arraybufferwriter-getspan-nondefault): ArrayBufferWriter<T>::GetSpan/GetMemory require T to be default-constructible
    ArrayBufferWriter<NoDefault> spanWriter;
    (void)spanWriter.GetSpan(4);
#else
    ArrayBufferWriter<int> spanWriter;
    (void)spanWriter.GetSpan(4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(arraybufferwriter-getmemory-nondefault): ArrayBufferWriter<T>::GetSpan/GetMemory require T to be default-constructible
    ArrayBufferWriter<NoDefault> memoryWriter;
    (void)memoryWriter.GetMemory(4);
#else
    ArrayBufferWriter<int> memoryWriter;
    (void)memoryWriter.GetMemory(4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(arraybufferwriter-clear-nondefault): ArrayBufferWriter<T>::Clear() requires T to be default-constructible
    ArrayBufferWriter<NoDefault> clearWriter;
    clearWriter.Clear();
#else
    ArrayBufferWriter<int> clearWriter;
    clearWriter.Clear();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(arraybufferwriter-clear-noncopyassignable): ArrayBufferWriter<T>::Clear() requires T to be copy-assignable
    ArrayBufferWriter<NoCopyAssign> assignWriter(4);
    assignWriter.Clear();
#else
    // ResetWrittenCount() is the member that needs neither requirement.
    ArrayBufferWriter<NoCopyAssign> assignWriter(4);
    assignWriter.ResetWrittenCount();
#endif

    // Sites 6 and 7 do NOT spell `MemoryPool<T>::Shared().Rent(n)` /
    // `ArrayPool<T>::Shared().Rent(n)`, which is what a caller writes and what
    // the accepted (#else) branches use. That spelling IS rejected -- measured,
    // before and after #2054 -- but it cannot be PROVEN rejected here: the
    // failing member is reached from inside `Shared()`'s own body, so GCC roots
    // the instantiation chain at a header line and no diagnostic ever names a
    // line of this fixture. The checker then reports "the fixture may no longer
    // be compiled at all", which is the right answer to an unattributable
    // result and a real limit of a per-site fixture over VIRTUAL members.
    //
    // The sites therefore name the concrete pool type and declare it `extern`,
    // so that the call -- not the vtable -- is the first instantiation, and the
    // chain roots on the line below. Same requirement, same assert, attributable
    // proof. docs/NegativeConsumerFixtureValidation.md records the limit.
#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(memorypool-rent-nondefault): MemoryPool<T>::Shared().Rent() requires T to be default-constructible
    extern System::Buffers::DefaultMemoryPool_<NoDefault> heapPool;
    (void)heapPool.Rent(4);
#else
    auto owner = MemoryPool<int>::Shared().Rent(4);
    (void)owner;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(arraypool-rent-nondefault): ArrayPool<T>::Shared()/Create() require T to be default-constructible
    extern System::Buffers::SharedArrayPool<NoDefault> heapArrayPool;
    (void)heapArrayPool.Rent(4);
#else
    auto rented = ArrayPool<int>::Shared().Rent(4);
    (void)rented;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // Rent and Return are VIRTUAL, so instantiating the pool at all -- even to
    // call Return with clearArray = false, which needs nothing -- already
    // required a default-constructible T. That is what this site pins.
    // NEGATIVE(arraypool-return-noclear-nondefault): ArrayPool<T> requires T to be default-constructible
    std::vector<NoDefault> returned;
    returned.emplace_back(1);
    ArrayPool<NoDefault>::Shared().Return(returned, false);
#else
    std::vector<int> returned(1, 1);
    ArrayPool<int>::Shared().Return(returned, false);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 9
    // NEGATIVE(arraypool-noncopyassignable): ArrayPool<T> requires T to be copy-assignable
    std::vector<NoCopyAssign> cleared(2);
    ArrayPool<NoCopyAssign>::Shared().Return(cleared, true);
#else
    std::vector<int> cleared(2);
    ArrayPool<int>::Shared().Return(cleared, true);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 10
    // NEGATIVE(searchvalues-initializer-list-equality-only): SearchValues<T> requires a usable std::hash<T>
    SearchValues<EqualityOnly> listValues{EqualityOnly(1), EqualityOnly(2)};
    (void)listValues.Contains(EqualityOnly(1));
#else
    SearchValues<EqualityAndHash> listValues{EqualityAndHash(1), EqualityAndHash(2)};
    (void)listValues.Contains(EqualityAndHash(1));
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 11
    // NEGATIVE(searchvalues-vector-equality-only): SearchValues<T> requires a usable std::hash<T>
    std::vector<EqualityOnly> source{EqualityOnly(1)};
    SearchValues<EqualityOnly> vectorValues(source);
    (void)vectorValues.Contains(EqualityOnly(1));
#else
    std::vector<EqualityAndHash> source{EqualityAndHash(1)};
    SearchValues<EqualityAndHash> vectorValues(source);
    (void)vectorValues.Contains(EqualityAndHash(1));
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 12
    // NEGATIVE(sequencereader-tryread-nondefault): SequenceReader<T>::TryRead requires T to be default-constructible
    ReadOnlySequence<NoDefault> readSequence;
    SequenceReader<NoDefault> reader(readSequence);
    NoDefault read(0);
    (void)reader.TryRead(read);
#else
    ReadOnlySequence<int> readSequence;
    SequenceReader<int> reader(readSequence);
    int read = 0;
    (void)reader.TryRead(read);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 13
    // NEGATIVE(sequencereader-trypeek-nondefault): SequenceReader<T>::TryPeek requires T to be default-constructible
    ReadOnlySequence<NoDefault> peekSequence;
    SequenceReader<NoDefault> peeker(peekSequence);
    NoDefault peeked(0);
    (void)peeker.TryPeek(peeked);
#else
    ReadOnlySequence<int> peekSequence;
    SequenceReader<int> peeker(peekSequence);
    int peeked = 0;
    (void)peeker.TryPeek(peeked);
#endif

    return 0;
}
