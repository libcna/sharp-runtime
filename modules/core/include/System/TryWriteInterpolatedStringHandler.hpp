// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <concepts>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include "System/ArgumentNullException.hpp"
#include "System/Boolean.hpp"
#include "System/Byte.hpp"
#include "System/Double.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/SByte.hpp"
#include "System/Single.hpp"
#include "System/Span.hpp"
#include "System/UInt16.hpp"
#include "System/UInt32.hpp"
#include "System/UInt64.hpp"

namespace System::detail {

    /**
     * @brief The `IFormattable` shape: a `ToString` that accepts a format string.
     *
     * `System::IFormattable` implementers satisfy this by construction — its sole
     * pure virtual is `ToString(const std::string&)` — and so does any type that
     * simply offers the same member. Matching on the shape rather than on
     * `is_base_of_v<IFormattable, T>` keeps this header from depending on the
     * interface, while still covering every implementer of it.
     */
    template <typename T>
    concept InterpolatedFormattableWithSpec = requires(const T& v, const std::string& f) {
        { v.ToString(f) } -> std::convertible_to<std::string>;
    };

    /**
     * @brief The `System::Object` shape: a no-argument `ToString`.
     *
     * Every `System::Object` descendant satisfies this, as does any plain struct
     * that offers `ToString()`. There is nowhere for a format specifier to go, so
     * one supplied alongside such a value is ignored.
     */
    template <typename T>
    concept InterpolatedHasToString = requires(const T& v) {
        { v.ToString() } -> std::convertible_to<std::string>;
    };

} // namespace System::detail

namespace System {

    /**
     * @brief Provides a handler for formatting interpolated strings into a character span.
     *
     * C++ counterpart of .NET System.MemoryExtensions.TryWriteInterpolatedStringHandler.
     * In .NET this is a compiler-generated ref struct; here it is a regular class used
     * manually to build formatted output into a fixed-size char buffer.
     *
     * Usage:
     * @code
     *   char buf[64]; std::size_t written = 0;
     *   TryWriteInterpolatedStringHandler h(buf, sizeof(buf));
     *   h.AppendLiteral("x=");
     *   h.AppendFormatted(42);
     *   written = h.getCharsWrittenProperty();  // "x=42" in buf
     * @endcode
     */
    class TryWriteInterpolatedStringHandler {
    public:
        /**
         * @brief Constructs a handler that writes into the given character buffer.
         *
         * A null @p destination is accepted only when @p destLen is zero, which is
         * this port's spelling of an empty destination; a null paired with a claimed
         * positive capacity is a programming error and is rejected here rather than
         * carried to the first write. See requireUsableDestination().
         *
         * @param destination Pointer to the output buffer.
         * @param destLen     Maximum number of characters the buffer can hold (excluding NUL).
         * @param literalLength Hint: total characters expected from literals (used to pre-check).
         * @param shouldAppend On return, true if the buffer is large enough for the literal hint.
         * @throws System::ArgumentNullException if @p destination is null and @p destLen
         *         is not zero. Note this is deliberately an exception rather than
         *         `shouldAppend = false`: that output reports insufficient *capacity*,
         *         and a destination that does not exist is a different failure.
         */
        TryWriteInterpolatedStringHandler(char* destination, std::size_t destLen,
                                          std::size_t literalLength, bool& shouldAppend)
            : dest_(requireUsableDestination(destination, destLen)),
              destLen_(destLen), pos_(0), success_(true) {
            shouldAppend = success_ = (destLen >= literalLength);
        }

        /**
         * @brief Constructs a handler without a literal-length pre-check.
         * @param destination Pointer to the output buffer.
         * @param destLen     Maximum number of characters the buffer can hold.
         * @throws System::ArgumentNullException if @p destination is null and @p destLen
         *         is not zero.
         */
        TryWriteInterpolatedStringHandler(char* destination, std::size_t destLen)
            : dest_(requireUsableDestination(destination, destLen)),
              destLen_(destLen), pos_(0), success_(true) {}

        // ---------------------------------------------------------------

        /**
         * @brief Appends a string literal into the buffer.
         * @param value The literal to append.
         * @return true if the value fit; false if the buffer is full.
         */
        bool AppendLiteral(const std::string& value) {
            return appendRaw(value.data(), value.size());
        }

        /**
         * @brief C-string overload; see the std::string overload above.
         *
         * A null @p value is rejected rather than treated as an empty literal. In .NET
         * this handler is compiler-generated and `AppendLiteral` receives only literal
         * text, so there is no .NET behaviour to copy and the policy is decided here:
         * the `std::string` overload cannot be null, so `""` is already the way to
         * spell an empty literal, and the `bool` result already means "did it fit".
         * Silently succeeding on null would give that result a second meaning and hide
         * the caller's bug. Ticket #1810 / SR-AUD-132, whose closing sentence asked for
         * exactly this policy to be defined.
         *
         * @param value The literal to append. Must not be null.
         * @return true if the value fit; false if the buffer is full.
         * @throws System::ArgumentNullException if @p value is null.
         */
        bool AppendLiteral(const char* value) {
            if (value == nullptr) throw System::ArgumentNullException("value");
            return appendRaw(value, std::strlen(value));
        }

        /**
         * @brief Appends a value into the buffer using this port's default format.
         *
         * The text is whatever this repository's own formatter for @p value's
         * category already answers — `Boolean::ToString` for `bool`, the matching
         * fixed-width integer wrapper for an integral type, `Single`/`Double`
         * `ToString` for the binary floats, the value itself for text, and the
         * value's own `ToString` for a type that has one. No spelling is invented
         * here; see formatValue() for the complete map and its two deliberate
         * boundaries.
         *
         * @param value The value to append.
         * @return true if the formatted text fit; false if the buffer is full.
         * @throws System::ArgumentNullException if @p value is a null `char` pointer.
         */
        template<typename T>
        bool AppendFormatted(const T& value) {
            return AppendLiteral(formatValue(value, std::string_view{}));
        }

        /**
         * @brief Appends a value into the buffer using an explicit format string.
         *
         * @p format is passed to the same sibling formatter the unformatted
         * overload uses, so `AppendFormatted(255, "X2")` yields exactly what
         * `Int32::ToString(255, "X2")` yields. An empty @p format is the default
         * format and is routed to the single-argument formatter, so it cannot
         * differ from the unformatted overload.
         *
         * Three categories have no formatter that accepts a specifier — text
         * (`std::string`, `std::string_view`, `char` pointers and arrays, `char`),
         * `bool`, and `long double` — and one more, a type whose only `ToString`
         * takes no argument. For those @p format is ignored. That is not a silent
         * widening: it is the pre-existing behaviour of those categories, it
         * matches .NET (where `string` is not `IFormattable`, so a specifier on a
         * string is discarded there too), and each is pinned by a test.
         *
         * @param value  The value to append.
         * @param format A format specifier accepted by @p value's sibling formatter.
         * @return true if the formatted text fit; false if the buffer is full.
         * @throws System::FormatException if @p format is not one the sibling
         *         formatter accepts. The throw happens before anything is written,
         *         so a rejected specifier leaves the handler exactly as it was.
         *         This is inherited, not invented: raising `FormatException` for an
         *         unrecognised specifier is the policy CCF-006 settled across all
         *         twelve numeric wrappers (tickets #1847/#1849), and delegating to
         *         them necessarily delegates their validation too.
         * @throws System::ArgumentNullException if @p value is a null `char` pointer.
         */
        template<typename T>
        bool AppendFormatted(const T& value, const std::string& format) {
            return AppendLiteral(formatValue(value, std::string_view(format)));
        }

        // ---------------------------------------------------------------

        /** @brief Returns the number of characters written so far. */
        [[nodiscard]] std::size_t getCharsWrittenProperty() const { return pos_; }

        /** @brief Returns true if all append operations succeeded. */
        [[nodiscard]] bool getSuccessProperty() const { return success_; }

        /** @brief Returns a std::string containing the characters written so far. */
        [[nodiscard]] std::string getString() const {
            // dest_ can still be null, but only for a zero-capacity handler, where pos_
            // is necessarily 0. std::string(const char*, size_type) requires a valid
            // range, and [nullptr, nullptr) is not one even though the count is zero.
            if (!success_ || dest_ == nullptr) return std::string{};
            return std::string(dest_, pos_);
        }

    private:
        char*       dest_;
        std::size_t destLen_;
        std::size_t pos_;
        bool        success_;

        /**
         * @brief Rejects a null destination that claims a nonzero capacity.
         *
         * The .NET counterpart takes a `Span<char>`, which cannot represent a nonempty
         * null destination at all, so this check restores by validation what the .NET
         * type gets from its parameter type. Before ticket #1810 the pointer and length
         * were stored unchecked: `TryWriteInterpolatedStringHandler(nullptr, 1)` passed
         * the capacity check in appendRaw() and reached
         * `std::memcpy(dest_ + pos_, ...)`, an AddressSanitizer-confirmed **write** to
         * address zero, plus a UBSan "null pointer passed as argument 1, which is
         * declared to never be null" (build-probe/1810_prefix_defects.log cases 1-2).
         *
         * A null paired with a capacity of ZERO stays valid: it is this port's spelling
         * of an empty destination, it already behaved correctly (case 3 refuses every
         * append and reports success=0), and it is the rule tickets #1774 and #1805
         * settled for the same pointer/length shape elsewhere in this repository.
         */
        static char* requireUsableDestination(char* destination, std::size_t destLen) {
            if (destination == nullptr && destLen != 0)
                throw System::ArgumentNullException("destination");
            return destination;
        }

        bool appendRaw(const char* data, std::size_t len) {
            if (!success_) return false;
            // Written as a subtraction, not as `pos_ + len > destLen_`: pos_ and len are
            // both size_t, so the sum can wrap and let an oversized append pass the very
            // check meant to stop it. pos_ <= destLen_ is an invariant of this class --
            // pos_ only ever advances by a len this test has already accepted -- so the
            // right-hand side cannot underflow.
            if (len > destLen_ - pos_) { success_ = false; return false; }
            // memcpy is undefined for a null pointer even with a length of zero, and both
            // ends can legitimately be null here: dest_ for a zero-capacity handler, data
            // for an empty std::string.
            if (len == 0) return true;
            std::memcpy(dest_ + pos_, data, len);
            pos_ += len;
            return true;
        }

        /**
         * @brief Calls one sibling formatter, choosing its overload by @p spec.
         *
         * An empty specifier is routed to the single-argument `ToString`, rather
         * than to `ToString(value, "")`, so the unformatted overload's text is by
         * construction the sibling's own unformatted text and cannot drift if a
         * wrapper ever changes how it reads an empty format string.
         */
        template<class Wrapper, class V>
        static std::string viaWrapper(V v, std::string_view spec) {
            if (spec.empty()) return Wrapper::ToString(v);
            return Wrapper::ToString(v, std::string(spec));
        }

        /**
         * @brief Maps one value to text, delegating to this repository's formatters.
         *
         * Before ticket #2304 (SR-AUD-133) this was a hand-written type map that
         * discarded the format specifier outright and answered with C++ default
         * spellings: `true` was `"1"`, `3.14` was `"3.140000"`, `255` with `"X2"`
         * was `"255"`, and every type that was neither arithmetic, `std::string`,
         * `const char*` nor `char` — **including a string literal**, whose deduced
         * type is `char[N]` and never matched the pointer arm — collapsed to the
         * placeholder `"[?]"`, losing the value entirely. The class doc-comment
         * claimed an `IFormattable` fallback that did not exist: a real
         * `System::IFormattable` implementer also produced `"[?]"`.
         *
         * The map now delegates, so no format grammar is written here and no text
         * is invented. Each arm answers with exactly what an existing, tested,
         * already-remediated sibling API answers for the same input:
         *
         * | Category | Authority |
         * |---|---|
         * | `bool` | `Boolean::ToString` |
         * | signed integral, by width | `SByte`/`Int16`/`Int32`/`Int64::ToString` |
         * | unsigned integral, by width | `Byte`/`UInt16`/`UInt32`/`UInt64::ToString` |
         * | `float` / `double` | `Single`/`Double::ToString` |
         * | text and `char` | the characters themselves |
         * | a type with `ToString(format)` | the value itself (covers `IFormattable`) |
         * | a type with `ToString()` | the value itself |
         *
         * Two boundaries are deliberate and unchanged rather than guessed:
         *
         * - **`long double`** keeps `std::to_string`. This repository has no
         *   extended-precision formatter, and narrowing to `double` to reach
         *   `Double::ToString` would silently lose precision. It is the one
         *   arithmetic arm whose text did not change.
         * - **`char16_t`** stays on the integral path and keeps emitting a number,
         *   even though `SharpRuntime::charcs` is `char16_t`. Whether that door
         *   means "a character" or "a 16-bit integer" is not answerable from this
         *   type's own contract, and changing it would be a guess.
         *
         * An integral type of some other width — `__int128` is the only one GCC
         * offers — reaches the same `std::to_string` fallback it reached before,
         * which is ill-formed for it now exactly as it was then; the compile-domain
         * behaviour of this template is unchanged.
         *
         * A type with no `ToString` at all still yields `"[?]"`.
         *
         * @param v    The value to render.
         * @param spec The requested format, or empty for the default format.
         */
        template<typename T>
        static std::string formatValue(const T& v, std::string_view spec) {
            // --- Text. .NET's String is not IFormattable, so a specifier does not
            // apply to it there either; the same holds for this port's char types.
            if constexpr (std::is_same_v<T, std::string>) {
                (void)spec;
                return v;
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                (void)spec;
                return std::string(v);
            } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
                (void)spec;
                // std::string(const char*) requires a NUL-terminated array. Before
                // ticket #2305 a null reached it and libstdc++ threw std::logic_error
                // out of a System-shaped API with nothing to catch it, aborting the
                // process (exit 134). Rejected here on exactly the terms #1810 settled
                // for AppendLiteral(const char*).
                if (v == nullptr) throw System::ArgumentNullException("value");
                return std::string(v);
            } else if constexpr (std::is_array_v<T> &&
                                 std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>, char>) {
                // A string literal deduces to char[N], which never matched the pointer
                // arm and so used to collapse to "[?]". Read within the array bounds
                // and stop at the first NUL, so an array that happens not to be
                // NUL-terminated is still safe -- unlike std::string(v), which would
                // run off the end of it.
                const std::string_view all(static_cast<const char*>(v), std::extent_v<T>);
                const std::size_t z = all.find('\0');
                (void)spec;
                return std::string(z == std::string_view::npos ? all : all.substr(0, z));
            } else if constexpr (std::is_same_v<T, char>) {
                (void)spec;
                return std::string(1, v);
            }
            // --- bool, before the integral arm: bool is an integral type, and
            // Boolean has no format overload, so a specifier is ignored for it.
            else if constexpr (std::is_same_v<T, bool>) {
                (void)spec;
                return System::Boolean::ToString(v);
            }
            // --- Integral, routed to the wrapper of matching width and signedness.
            else if constexpr (std::is_integral_v<T>) {
                if constexpr (std::is_signed_v<T>) {
                    if constexpr (sizeof(T) == 1)
                        return viaWrapper<System::SByte>(static_cast<SharpRuntime::sbytecs>(v), spec);
                    else if constexpr (sizeof(T) == 2)
                        return viaWrapper<System::Int16>(static_cast<SharpRuntime::shortcs>(v), spec);
                    else if constexpr (sizeof(T) == 4)
                        return viaWrapper<System::Int32>(static_cast<SharpRuntime::intcs>(v), spec);
                    else if constexpr (sizeof(T) == 8)
                        return viaWrapper<System::Int64>(static_cast<SharpRuntime::longcs>(v), spec);
                    else { (void)spec; return std::to_string(v); }
                } else {
                    if constexpr (sizeof(T) == 1)
                        return viaWrapper<System::Byte>(static_cast<SharpRuntime::bytecs>(v), spec);
                    else if constexpr (sizeof(T) == 2)
                        return viaWrapper<System::UInt16>(static_cast<SharpRuntime::ushortcs>(v), spec);
                    else if constexpr (sizeof(T) == 4)
                        return viaWrapper<System::UInt32>(static_cast<SharpRuntime::uintcs>(v), spec);
                    else if constexpr (sizeof(T) == 8)
                        return viaWrapper<System::UInt64>(static_cast<SharpRuntime::ulongcs>(v), spec);
                    else { (void)spec; return std::to_string(v); }
                }
            }
            // --- Binary floats.
            else if constexpr (std::is_same_v<T, float>) {
                return viaWrapper<System::Single>(v, spec);
            } else if constexpr (std::is_same_v<T, double>) {
                return viaWrapper<System::Double>(v, spec);
            }
            // --- The one arithmetic category this repository has no formatter for.
            else if constexpr (std::is_arithmetic_v<T>) {
                (void)spec;
                return std::to_string(v);
            }
            // --- The value's own contract. This is the fallback the class
            // doc-comment has always advertised and never performed.
            else if constexpr (detail::InterpolatedFormattableWithSpec<T>) {
                return v.ToString(std::string(spec));
            } else if constexpr (detail::InterpolatedHasToString<T>) {
                (void)spec;
                return v.ToString();
            } else {
                (void)spec;
                return std::string("[?]");
            }
        }
    };

} // namespace System
