// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Numerics {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents an arbitrarily large signed integer.
     *
     * Self-contained implementation using sign + base-10^9 magnitude vector.
     * Partial C++ counterpart of .NET System.Numerics.BigInteger.
     *
     * @note Status: Partial — add, sub, mul, comparisons, ToString, Parse implemented;
     *   division, modulo, and bit operations are not yet implemented.
     */
    class BigInteger {
        bool                   negative_ = false;
        std::vector<uint32_t>  mag_;       // base 10^9, least significant first

        static constexpr uint32_t BASE = 1000000000u;

        void trim() {
            while (mag_.size() > 1 && mag_.back() == 0) mag_.pop_back();
            if (mag_.size() == 1 && mag_[0] == 0) negative_ = false;
        }

        static int cmpMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
            if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
            for (int i = static_cast<int>(a.size())-1; i >= 0; --i)
                if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
            return 0;
        }

        static std::vector<uint32_t> addMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
            std::vector<uint32_t> r;
            uint64_t carry = 0;
            for (size_t i = 0; i < std::max(a.size(), b.size()) || carry; ++i) {
                uint64_t sum = carry;
                if (i < a.size()) sum += a[i];
                if (i < b.size()) sum += b[i];
                r.push_back(static_cast<uint32_t>(sum % BASE));
                carry = sum / BASE;
            }
            return r;
        }

        static std::vector<uint32_t> subMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
            // assumes |a| >= |b|
            std::vector<uint32_t> r;
            int64_t borrow = 0;
            for (size_t i = 0; i < a.size(); ++i) {
                int64_t d = static_cast<int64_t>(a[i]) - borrow - (i < b.size() ? b[i] : 0);
                if (d < 0) { d += BASE; borrow = 1; } else { borrow = 0; }
                r.push_back(static_cast<uint32_t>(d));
            }
            while (r.size() > 1 && r.back() == 0) r.pop_back();
            return r;
        }

        static std::vector<uint32_t> mulMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
            std::vector<uint32_t> r(a.size() + b.size(), 0);
            for (size_t i = 0; i < a.size(); ++i) {
                uint64_t carry = 0;
                for (size_t j = 0; j < b.size() || carry; ++j) {
                    uint64_t cur = static_cast<uint64_t>(r[i+j]) + carry;
                    if (j < b.size()) cur += static_cast<uint64_t>(a[i]) * b[j];
                    r[i+j] = static_cast<uint32_t>(cur % BASE);
                    carry = cur / BASE;
                }
            }
            while (r.size() > 1 && r.back() == 0) r.pop_back();
            return r;
        }

        static std::vector<uint32_t> fromLong(uint64_t v) {
            std::vector<uint32_t> r;
            if (v == 0) { r.push_back(0); return r; }
            while (v) { r.push_back(static_cast<uint32_t>(v % BASE)); v /= BASE; }
            return r;
        }

    public:
        BigInteger() : mag_({0}) {}

        BigInteger(intcs v) {
            negative_ = v < 0;
            mag_ = fromLong(negative_ ? static_cast<uint64_t>(-(int64_t)v) : static_cast<uint64_t>(v));
        }

        BigInteger(longcs v) {
            negative_ = v < 0;
            mag_ = fromLong(negative_ ? static_cast<uint64_t>(-v) : static_cast<uint64_t>(v));
        }

        [[nodiscard]] bool getIsZeroProperty()     const { return mag_.size() == 1 && mag_[0] == 0; }
        [[nodiscard]] bool getIsOneProperty()      const { return !negative_ && mag_.size() == 1 && mag_[0] == 1; }
        [[nodiscard]] bool getIsNegativeProperty() const { return negative_; }

        [[nodiscard]] int Sign() const { return getIsZeroProperty() ? 0 : (negative_ ? -1 : 1); }

        static BigInteger Parse(const std::string& s) {
            BigInteger r;
            bool neg = (!s.empty() && s[0] == '-');
            size_t start = (neg || (!s.empty() && s[0] == '+')) ? 1 : 0;
            std::vector<uint32_t> m;
            // parse right-to-left in chunks of 9 digits
            int i = static_cast<int>(s.size());
            while (i > static_cast<int>(start)) {
                int from = std::max(static_cast<int>(start), i-9);
                std::string chunk = s.substr(from, i-from);
                m.push_back(static_cast<uint32_t>(std::stoul(chunk)));
                i = from;
            }
            if (m.empty()) m.push_back(0);
            r.mag_ = m;
            r.negative_ = neg;
            r.trim();
            return r;
        }

        [[nodiscard]] std::string ToString() const {
            if (getIsZeroProperty()) return "0";
            std::string s;
            for (int i = static_cast<int>(mag_.size())-1; i >= 0; --i) {
                if (s.empty()) s += std::to_string(mag_[i]);
                else {
                    std::string chunk = std::to_string(mag_[i]);
                    s += std::string(9 - chunk.size(), '0') + chunk;
                }
            }
            return negative_ ? "-" + s : s;
        }

        BigInteger operator-() const { BigInteger r(*this); if (!getIsZeroProperty()) r.negative_ = !negative_; return r; }
        BigInteger Abs() const { BigInteger r(*this); r.negative_ = false; return r; }

        BigInteger operator+(const BigInteger& o) const {
            if (negative_ == o.negative_) { BigInteger r; r.negative_ = negative_; r.mag_ = addMag(mag_, o.mag_); r.trim(); return r; }
            int c = cmpMag(mag_, o.mag_);
            if (c == 0) return BigInteger(0);
            BigInteger r;
            if (c > 0) { r.negative_ = negative_; r.mag_ = subMag(mag_, o.mag_); }
            else        { r.negative_ = o.negative_; r.mag_ = subMag(o.mag_, mag_); }
            r.trim(); return r;
        }

        BigInteger operator-(const BigInteger& o) const { return *this + (-o); }

        BigInteger operator*(const BigInteger& o) const {
            BigInteger r;
            r.negative_ = negative_ != o.negative_;
            r.mag_ = mulMag(mag_, o.mag_);
            r.trim();
            return r;
        }

        bool operator==(const BigInteger& o) const { return negative_ == o.negative_ && mag_ == o.mag_; }
        bool operator!=(const BigInteger& o) const { return !(*this == o); }
        bool operator< (const BigInteger& o) const {
            if (negative_ != o.negative_) return negative_;
            int c = cmpMag(mag_, o.mag_);
            return negative_ ? c > 0 : c < 0;
        }
        bool operator<=(const BigInteger& o) const { return !(o < *this); }
        bool operator> (const BigInteger& o) const { return o < *this; }
        bool operator>=(const BigInteger& o) const { return !(*this < o); }

        static const BigInteger Zero;
        static const BigInteger One;
        static const BigInteger MinusOne;
    };

    inline const BigInteger BigInteger::Zero    {  0 };
    inline const BigInteger BigInteger::One     {  1 };
    inline const BigInteger BigInteger::MinusOne{ -1 };

} // namespace System::Numerics
