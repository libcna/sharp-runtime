// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Numerics::Colors {

template<typename T> struct Rgba;

/// <summary>Represents a color in ARGB (Alpha, Red, Green, Blue) order.</summary>
template<typename T>
struct Argb {
    T A{};
    T R{};
    T G{};
    T B{};

    Argb() = default;
    Argb(T a, T r, T g, T b) : A(a), R(r), G(g), B(b) {}
    Argb(const std::vector<T>& values) {
        if (values.size() < 4) throw std::invalid_argument("values");
        A = values[0]; R = values[1]; G = values[2]; B = values[3];
    }

    void CopyTo(std::vector<T>& destination) const {
        if (destination.size() < 4) throw std::invalid_argument("destination");
        destination[0] = A; destination[1] = R; destination[2] = G; destination[3] = B;
    }

    bool Equals(const Argb<T>& other) const {
        return A == other.A && R == other.R && G == other.G && B == other.B;
    }
    bool operator==(const Argb<T>& other) const { return Equals(other); }
    bool operator!=(const Argb<T>& other) const { return !Equals(other); }

    std::string ToString() const {
        std::ostringstream ss;
        ss << "<" << +A << ", " << +R << ", " << +G << ", " << +B << ">";
        return ss.str();
    }

    inline Rgba<T> ToRgba() const;
};

/// <summary>Represents a color in RGBA (Red, Green, Blue, Alpha) order.</summary>
template<typename T>
struct Rgba {
    T R{};
    T G{};
    T B{};
    T A{};

    Rgba() = default;
    Rgba(T r, T g, T b, T a) : R(r), G(g), B(b), A(a) {}
    Rgba(const std::vector<T>& values) {
        if (values.size() < 4) throw std::invalid_argument("values");
        R = values[0]; G = values[1]; B = values[2]; A = values[3];
    }

    void CopyTo(std::vector<T>& destination) const {
        if (destination.size() < 4) throw std::invalid_argument("destination");
        destination[0] = R; destination[1] = G; destination[2] = B; destination[3] = A;
    }

    bool Equals(const Rgba<T>& other) const {
        return R == other.R && G == other.G && B == other.B && A == other.A;
    }
    bool operator==(const Rgba<T>& other) const { return Equals(other); }
    bool operator!=(const Rgba<T>& other) const { return !Equals(other); }

    std::string ToString() const {
        std::ostringstream ss;
        ss << "<" << +R << ", " << +G << ", " << +B << ", " << +A << ">";
        return ss.str();
    }

    Argb<T> ToArgb() const { return Argb<T>(A, R, G, B); }
};

template<typename T>
Rgba<T> Argb<T>::ToRgba() const { return Rgba<T>(R, G, B, A); }

} // namespace System::Numerics::Colors
