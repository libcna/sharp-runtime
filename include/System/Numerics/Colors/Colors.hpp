// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Numerics::Colors {

template<typename T> struct Rgba;

/** Represents a color in ARGB (Alpha, Red, Green, Blue) order. */
template<typename T>
struct Argb {
    T A{}; ///< Alpha channel.
    T R{}; ///< Red channel.
    T G{}; ///< Green channel.
    T B{}; ///< Blue channel.

    /** Default constructor — all channels zero-initialised. */
    Argb() = default;
    /**
     * @brief Constructs an Argb from individual channel values.
     * @param a Alpha value.
     * @param r Red value.
     * @param g Green value.
     * @param b Blue value.
     */
    Argb(T a, T r, T g, T b) : A(a), R(r), G(g), B(b) {}
    /**
     * @brief Constructs an Argb from a vector with at least 4 elements (A, R, G, B).
     * @param values Source vector; must have at least 4 elements.
     * @throws std::invalid_argument if @p values has fewer than 4 elements.
     */
    Argb(const std::vector<T>& values) {
        if (values.size() < 4) throw std::invalid_argument("values");
        A = values[0]; R = values[1]; G = values[2]; B = values[3];
    }

    /**
     * @brief Copies the four channels into @p destination (must have at least 4 elements).
     * @param destination Target vector; must have at least 4 elements.
     * @throws std::invalid_argument if @p destination has fewer than 4 elements.
     */
    void CopyTo(std::vector<T>& destination) const {
        if (destination.size() < 4) throw std::invalid_argument("destination");
        destination[0] = A; destination[1] = R; destination[2] = G; destination[3] = B;
    }

    /** Returns true if all four channels of @p other equal this instance's channels. */
    bool Equals(const Argb<T>& other) const {
        return A == other.A && R == other.R && G == other.G && B == other.B;
    }
    /** Returns true if all channels are equal to @p other. */
    bool operator==(const Argb<T>& other) const { return Equals(other); }
    /** Returns true if any channel differs from @p other. */
    bool operator!=(const Argb<T>& other) const { return !Equals(other); }

    /** Returns a string representation in the form "<A, R, G, B>". */
    std::string ToString() const {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << "<" << +A << ", " << +R << ", " << +G << ", " << +B << ">";
        return ss.str();
    }

    /** Converts this Argb to the equivalent Rgba representation. */
    inline Rgba<T> ToRgba() const;
};

/** Represents a color in RGBA (Red, Green, Blue, Alpha) order. */
template<typename T>
struct Rgba {
    T R{}; ///< Red channel.
    T G{}; ///< Green channel.
    T B{}; ///< Blue channel.
    T A{}; ///< Alpha channel.

    /** Default constructor — all channels zero-initialised. */
    Rgba() = default;
    /**
     * @brief Constructs an Rgba from individual channel values.
     * @param r Red value.
     * @param g Green value.
     * @param b Blue value.
     * @param a Alpha value.
     */
    Rgba(T r, T g, T b, T a) : R(r), G(g), B(b), A(a) {}
    /**
     * @brief Constructs an Rgba from a vector with at least 4 elements (R, G, B, A).
     * @param values Source vector; must have at least 4 elements.
     * @throws std::invalid_argument if @p values has fewer than 4 elements.
     */
    Rgba(const std::vector<T>& values) {
        if (values.size() < 4) throw std::invalid_argument("values");
        R = values[0]; G = values[1]; B = values[2]; A = values[3];
    }

    /**
     * @brief Copies the four channels into @p destination (must have at least 4 elements).
     * @param destination Target vector; must have at least 4 elements.
     * @throws std::invalid_argument if @p destination has fewer than 4 elements.
     */
    void CopyTo(std::vector<T>& destination) const {
        if (destination.size() < 4) throw std::invalid_argument("destination");
        destination[0] = R; destination[1] = G; destination[2] = B; destination[3] = A;
    }

    /** Returns true if all four channels of @p other equal this instance's channels. */
    bool Equals(const Rgba<T>& other) const {
        return R == other.R && G == other.G && B == other.B && A == other.A;
    }
    /** Returns true if all channels are equal to @p other. */
    bool operator==(const Rgba<T>& other) const { return Equals(other); }
    /** Returns true if any channel differs from @p other. */
    bool operator!=(const Rgba<T>& other) const { return !Equals(other); }

    /** Returns a string representation in the form "<R, G, B, A>". */
    std::string ToString() const {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << "<" << +R << ", " << +G << ", " << +B << ", " << +A << ">";
        return ss.str();
    }

    /** Converts this Rgba to the equivalent Argb representation. */
    Argb<T> ToArgb() const { return Argb<T>(A, R, G, B); }
};

template<typename T>
Rgba<T> Argb<T>::ToRgba() const { return Rgba<T>(R, G, B, A); }

} // namespace System::Numerics::Colors
