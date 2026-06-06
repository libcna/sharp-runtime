// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <complex>
#include <cmath>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Numerics {

    using SharpRuntime::Single;

    /**
     * @brief Represents a complex number.
     *
     * Wraps std::complex<double>. Partial C++ counterpart of .NET System.Numerics.Complex.
     *
     * @note Status: Implemented
     */
    struct Complex {
        std::complex<double> value;

        Complex() = default;
        Complex(double real, double imaginary) : value(real, imaginary) {}
        explicit Complex(std::complex<double> v) : value(v) {}

        [[nodiscard]] double getRealProperty()      const { return value.real(); }
        [[nodiscard]] double getImaginaryProperty() const { return value.imag(); }
        [[nodiscard]] double getMagnitudeProperty() const { return std::abs(value); }
        [[nodiscard]] double getPhaseProperty()     const { return std::arg(value); }

        Complex operator+(const Complex& o) const { return Complex(value + o.value); }
        Complex operator-(const Complex& o) const { return Complex(value - o.value); }
        Complex operator*(const Complex& o) const { return Complex(value * o.value); }
        Complex operator/(const Complex& o) const { return Complex(value / o.value); }
        Complex operator-()                 const { return Complex(-value); }
        bool operator==(const Complex& o)   const { return value == o.value; }
        bool operator!=(const Complex& o)   const { return value != o.value; }

        [[nodiscard]] Complex Conjugate() const { return Complex(std::conj(value)); }

        static Complex Abs(const Complex& c)   { return Complex(std::abs(c.value), 0); }
        static double  AbsD(const Complex& c)  { return std::abs(c.value); }
        static Complex Sqrt(const Complex& c)  { return Complex(std::sqrt(c.value)); }
        static Complex Exp(const Complex& c)   { return Complex(std::exp(c.value)); }
        static Complex Log(const Complex& c)   { return Complex(std::log(c.value)); }
        static Complex Pow(const Complex& b, const Complex& e) { return Complex(std::pow(b.value, e.value)); }
        static Complex Sin(const Complex& c)   { return Complex(std::sin(c.value)); }
        static Complex Cos(const Complex& c)   { return Complex(std::cos(c.value)); }
        static Complex Tan(const Complex& c)   { return Complex(std::tan(c.value)); }

        static const Complex Zero;
        static const Complex One;
        static const Complex ImaginaryOne;

        [[nodiscard]] std::string ToString() const {
            auto r = value.real(), i = value.imag();
            return std::string("<") + std::to_string(r) + "; " + std::to_string(i) + ">";
        }
    };

    inline const Complex Complex::Zero         { 0.0, 0.0 };
    inline const Complex Complex::One          { 1.0, 0.0 };
    inline const Complex Complex::ImaginaryOne { 0.0, 1.0 };

} // namespace System::Numerics
