// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Portable std::sscanf. MSVC's secure CRT deprecates std::sscanf (C4996) and this project
// compiles /W4 /WX, so every call to it is a hard error there. sscanf_s is the same parser --
// the C runtime's own documentation covers both under one grammar, and the only difference is
// that a c/C/s/S/[ conversion takes a buffer size immediately after its argument, while numeric
// and %n conversions take nothing extra. Routing MSVC through it therefore keeps the accepted
// grammar byte-for-byte identical on every compiler, which neither disabling the diagnostic nor
// hand-rolling a stricter scanner would do: a hand-written scanner also rejects input sscanf
// accepts, and the callers here are written against sscanf's own leniencies.
//
// Macros rather than a variadic wrapper function, for two reasons a function cannot satisfy:
// the size argument has to be injected *between* existing arguments, and keeping the format
// string a literal at the call site preserves GCC's and Clang's format/argument checking.
#pragma once

#include <cstdio>

/**
 * @brief `std::sscanf`, spelled as the secure CRT variant on MSVC.
 *
 * Every character-array argument must be wrapped in SHARP_RUNTIME_SCANF_BUFFER; numeric
 * arguments are passed unchanged.
 */
#if defined(_MSC_VER)
#  define SHARP_RUNTIME_SSCANF ::sscanf_s
#else
#  define SHARP_RUNTIME_SSCANF std::sscanf
#endif

/**
 * @brief Passes a character array to SHARP_RUNTIME_SSCANF with the size `sscanf_s` requires.
 *
 * The argument must be a real array, not a decayed pointer, because the size comes from
 * `sizeof`. The size is narrowed to `unsigned`, which is the type the secure CRT reads back out
 * of the variadic list -- passing `std::size_t` is what MSVC reports as C4477. Every buffer here
 * is a small fixed-size local, so the conversion cannot lose anything.
 */
#if defined(_MSC_VER)
#  define SHARP_RUNTIME_SCANF_BUFFER(buffer) (buffer), static_cast<unsigned>(sizeof(buffer))
#else
#  define SHARP_RUNTIME_SCANF_BUFFER(buffer) (buffer)
#endif
