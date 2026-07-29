// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/MemoryStream.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/IOException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include <algorithm>
#include <vector>

namespace System::IO
{
    namespace
    {
        // Validates the raw-buffer constructor's arguments and produces the copy.
        //
        // Verified against MemoryStream.cs's MemoryStream(byte[] buffer, ...): real .NET
        // throws ArgumentNullException for a null buffer and ArgumentOutOfRangeException
        // for a negative count, and it does so BEFORE touching the source. This port
        // previously ran the std::vector range constructor first, so `buffer + size`
        // and the range copy happened ahead of any check at all:
        //   * (nullptr, 1) read through a null pointer -- an ASan-confirmed SEGV and a
        //     UBSan-confirmed "load of null pointer" (ticket #1805 / SR-AUD-341);
        //   * a negative size formed `buffer + size` (out-of-bounds pointer arithmetic,
        //     itself undefined) and then constructed a vector from a reversed range,
        //     which surfaced as a raw std::length_error, "cannot create std::vector
        //     larger than max_size()" -- a standard-library exception leaking through a
        //     public API whose whole purpose is to mirror .NET's argument diagnostics.
        //
        // A null pointer paired with a size of zero is deliberately NOT rejected. .NET's
        // parameter is a byte[] object, which has no "null but empty" spelling, but this
        // port's parameter is a pointer/length pair, where (nullptr, 0) is the ordinary
        // way to denote an empty range: `std::vector<bytecs>().data()` is permitted to
        // return null, and BinaryData::ToStream() reaches this constructor exactly that
        // way for empty content. That input is well defined today and produces a correct
        // empty stream, so rejecting it would be a regression rather than a repair. This
        // matches the rule ticket #1774 settled for the same pointer/length shape on
        // ICollection::CopyTo -- only a null pointer paired with a NONZERO size is an
        // error. UnmanagedMemoryStream diverges from both, rejecting null unconditionally,
        // because it RETAINS the caller's pointer and .NET's own UnmanagedMemoryStream
        // rejects it unconditionally too.
        std::vector<bytecs> validatedBufferCopy(const bytecs* buffer, intcs size)
        {
            // Null takes precedence over a bad size, matching .NET's own ordering, in
            // which ArgumentNullException.ThrowIfNull(buffer) precedes the count check.
            if (buffer == nullptr && size != 0)
                throw System::ArgumentNullException("buffer");
            if (size < 0)
                throw System::ArgumentOutOfRangeException("size", "Non-negative number required.");
            if (size == 0) return {};
            return std::vector<bytecs>(buffer, buffer + size);
        }
    }

    MemoryStream::MemoryStream()
        : position_(0), writable_(true) {}

    MemoryStream::MemoryStream(const bytecs* buffer, intcs size, bool writable)
        : data_(validatedBufferCopy(buffer, size)), position_(0), writable_(writable) {}

    void MemoryStream::ensureNotClosed() const
    {
        if (!isOpen_) throw System::ObjectDisposedException("Cannot access a closed Stream.");
    }

    // Verified against MemoryStream.cs's Read()/ValidateBufferArguments: real .NET throws
    // ArgumentNullException for a null buffer and ArgumentOutOfRangeException for a negative
    // offset/count, matching FileStream::Read's existing validation in this codebase. This
    // previously returned 0 (indistinguishable from "stream at EOF") for every one of these
    // cases instead of throwing -- a caller passing a negative count would silently get "0
    // bytes read" instead of an error.
    intcs MemoryStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        ensureNotClosed();
        if (buffer == nullptr) throw System::ArgumentNullException("buffer");
        if (offset < 0) throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
        if (count < 0) throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
        const intcs remaining = static_cast<intcs>(data_.size()) - position_;
        const intcs toRead = std::min(count, remaining);
        if (toRead <= 0) return 0;
        std::copy(data_.begin() + position_, data_.begin() + position_ + toRead, buffer + offset);
        position_ += toRead;
        return toRead;
    }

    void MemoryStream::Write(const bytecs buffer[], intcs offset, intcs count)
    {
        ensureNotClosed();
        // Verified against MemoryStream.cs's Write()/EnsureWriteable(): real .NET throws
        // NotSupportedException when !CanWrite, matching this class's own SetLength(). This
        // port previously just returned, silently dropping every byte the caller thought it
        // had written.
        if (!writable_) throw System::NotSupportedException("Stream does not support writing.");
        // Verified against MemoryStream.cs's Write()/ValidateBufferArguments: real .NET throws
        // ArgumentNullException for a null buffer and ArgumentOutOfRangeException for a
        // negative offset/count, matching Read()'s validation above. This previously only
        // checked `buffer == nullptr || count <= 0` and silently returned for everything else
        // -- a negative offset reached std::copy(buffer + offset, ...) unchecked, reading
        // before the caller's array (confirmed real out-of-bounds read via a standalone ASan
        // repro), not just a silent no-op.
        if (buffer == nullptr) throw System::ArgumentNullException("buffer");
        if (offset < 0) throw System::ArgumentOutOfRangeException("offset", "Non-negative number required.");
        if (count < 0) throw System::ArgumentOutOfRangeException("count", "Non-negative number required.");
        if (count == 0) return;
        // Position can legally be set arbitrarily far past the end (setPositionProperty only
        // rejects negative values, matching real .NET's own Position setter, which allows
        // seeking past Length -- the resize just happens lazily on the next Write). That means
        // position_+count (both intcs/int32) can genuinely signed-overflow -- confirmed real UB
        // via a standalone UBSan repro on the identical pattern in Span<T>::Slice (ticket
        // 265/1487) -- and worse than "just UB": computing it directly as intcs and wrapping to
        // a negative sum would silently compare as <= data_.size(), skipping the resize, so the
        // std::copy below would write through data_.begin()+position_ with position_ wildly
        // beyond the vector's actual allocation -- a real heap buffer overflow. Computed here in
        // int64_t (always wide enough to hold the sum of two int32 values without overflow) to
        // avoid the UB entirely, matching real .NET's own MemoryStream.Write, which computes
        // this exact sum and explicitly checks for the overflow, throwing
        // IOException(SR.IO_StreamTooLong).
        int64_t newLength64 = static_cast<int64_t>(position_) + static_cast<int64_t>(count);
        if (newLength64 > static_cast<int64_t>(SharpRuntime::INTCS_MAX))
            throw System::IO::IOException("Stream was too long.");
        intcs newLength = static_cast<intcs>(newLength64);
        if (newLength > static_cast<intcs>(data_.size()))
            data_.resize(static_cast<size_t>(newLength));
        std::copy(buffer + offset, buffer + offset + count, data_.begin() + position_);
        position_ = newLength;
    }

    void MemoryStream::WriteByte(bytecs value)
    {
        ensureNotClosed();
        if (!writable_) throw System::NotSupportedException("Stream does not support writing.");
        if (position_ >= static_cast<intcs>(data_.size())) data_.push_back(value);
        else data_[static_cast<size_t>(position_)] = value;
        ++position_;
    }

    // Verified against MemoryStream.cs's Dispose(bool): real .NET explicitly does NOT clear
    // the underlying buffer on Close/Dispose -- "Don't set buffer to null - allow TryGetBuffer,
    // GetBuffer & ToArray to work" -- nor does it reset the position. This previously cleared
    // both, contradicting this method's own doc comment ("no-op for MemoryStream") and
    // destroying data real .NET deliberately preserves after Close(). Marks the stream closed
    // (ticket 1724) so Read/Write/Seek/Length/Position now correctly throw
    // ObjectDisposedException afterward, matching real .NET's _isOpen guard -- while
    // GetBuffer()/ToArray() deliberately stay unguarded, matching real .NET's own
    // MemoryStream.cs (neither calls EnsureNotClosed()).
    void MemoryStream::Close()
    {
        isOpen_ = false;
    }

    intcs MemoryStream::getLengthProperty() const
    {
        ensureNotClosed();
        return static_cast<intcs>(data_.size());
    }

    intcs MemoryStream::getPositionProperty() const
    {
        ensureNotClosed();
        return position_;
    }

    void MemoryStream::setPositionProperty(intcs value)
    {
        ensureNotClosed();
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        position_ = value;
    }

    void MemoryStream::SetLength(intcs value)
    {
        ensureNotClosed();
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        if (!writable_)
            throw System::NotSupportedException("Stream does not support writing.");
        data_.resize(static_cast<size_t>(value), bytecs{0});
        if (position_ > value) position_ = value;
    }
}
