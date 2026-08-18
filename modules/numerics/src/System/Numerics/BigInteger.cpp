// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Numerics/BigInteger.hpp"
#include "System/DivideByZeroException.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"
#include <algorithm>
#include <limits>

namespace System::Numerics {

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void BigInteger::trim() {
    while (mag_.size() > 1 && mag_.back() == 0) mag_.pop_back();
    if (mag_.size() == 1 && mag_[0] == 0) negative_ = false;
}

int BigInteger::cmpMag(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

std::vector<uint32_t> BigInteger::addMag(const std::vector<uint32_t>& a,
                                          const std::vector<uint32_t>& b) {
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

std::vector<uint32_t> BigInteger::subMag(const std::vector<uint32_t>& a,
                                          const std::vector<uint32_t>& b) {
    // Precondition: |a| >= |b|
    std::vector<uint32_t> r;
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t d = static_cast<int64_t>(a[i]) - borrow
                    - (i < b.size() ? b[i] : 0);
        if (d < 0) { d += BASE; borrow = 1; } else { borrow = 0; }
        r.push_back(static_cast<uint32_t>(d));
    }
    while (r.size() > 1 && r.back() == 0) r.pop_back();
    return r;
}

std::vector<uint32_t> BigInteger::mulMag(const std::vector<uint32_t>& a,
                                          const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size() || carry; ++j) {
            uint64_t cur = static_cast<uint64_t>(r[i + j]) + carry;
            if (j < b.size()) cur += static_cast<uint64_t>(a[i]) * b[j];
            r[i + j] = static_cast<uint32_t>(cur % BASE);
            carry    = cur / BASE;
        }
    }
    while (r.size() > 1 && r.back() == 0) r.pop_back();
    return r;
}

// Single-limb divisor — simple long division in base BASE.
std::vector<uint32_t> BigInteger::divMagByLimb(const std::vector<uint32_t>& a,
                                                uint32_t b, uint32_t& rem) {
    std::vector<uint32_t> q(a.size());
    uint64_t r = 0;
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        uint64_t cur = r * BASE + a[i];
        q[i] = static_cast<uint32_t>(cur / b);
        r    = cur % b;
    }
    rem = static_cast<uint32_t>(r);
    while (q.size() > 1 && q.back() == 0) q.pop_back();
    return q;
}

// Multi-limb division: Knuth Algorithm D (base BASE = 10^9).
// Returns {quotient magnitude, remainder magnitude}, both least-significant first.
std::pair<std::vector<uint32_t>, std::vector<uint32_t>>
BigInteger::divmodMag(std::vector<uint32_t> u, std::vector<uint32_t> v) {
    int n = static_cast<int>(v.size());
    int m = static_cast<int>(u.size()) - n; // quotient has m+1 digits

    // Step D1: normalise — multiply both by d = BASE / (v[n-1] + 1)
    uint64_t d = static_cast<uint64_t>(BASE) / (static_cast<uint64_t>(v[n-1]) + 1);

    // Extend u by one leading zero limb after normalisation
    u.push_back(0);
    if (d > 1) {
        uint64_t carry = 0;
        for (auto& x : u)  { uint64_t p = static_cast<uint64_t>(x)*d + carry; x = static_cast<uint32_t>(p % BASE); carry = p / BASE; }
        carry = 0;
        for (auto& x : v)  { uint64_t p = static_cast<uint64_t>(x)*d + carry; x = static_cast<uint32_t>(p % BASE); carry = p / BASE; }
    }

    std::vector<uint32_t> q(static_cast<size_t>(m + 1), 0);

    for (int j = m; j >= 0; --j) {
        // Step D3: estimate quotient digit
        uint64_t top2 = static_cast<uint64_t>(u[j + n]) * BASE
                      + static_cast<uint64_t>(u[j + n - 1]);
        uint64_t vn1  = static_cast<uint64_t>(v[n - 1]);
        uint64_t qhat = top2 / vn1;
        uint64_t rhat = top2 % vn1;

        // Step D3 refinement: adjust qhat down at most twice
        uint64_t vn2 = (n >= 2) ? static_cast<uint64_t>(v[n - 2]) : 0;
        while (qhat >= static_cast<uint64_t>(BASE) ||
               (n >= 2 && qhat * vn2 > rhat * static_cast<uint64_t>(BASE)
                                      + static_cast<uint64_t>(u[j + n - 2]))) {
            --qhat;
            rhat += vn1;
            if (rhat >= static_cast<uint64_t>(BASE)) break;
        }

        // Step D4: u[j..j+n] -= qhat * v
        int64_t  borrow = 0;
        uint64_t carry  = 0;
        for (int i = 0; i <= n; ++i) {
            uint64_t prod = (i < n) ? qhat * static_cast<uint64_t>(v[i]) + carry : carry;
            carry = prod / BASE;
            int64_t diff = static_cast<int64_t>(u[j + i])
                         - static_cast<int64_t>(prod % BASE) - borrow;
            if (diff < 0) { diff += static_cast<int64_t>(BASE); borrow = 1; }
            else            borrow = 0;
            u[j + i] = static_cast<uint32_t>(diff);
        }

        // Step D5: if underflow, add back
        if (borrow) {
            --qhat;
            uint64_t c = 0;
            for (int i = 0; i <= n; ++i) {
                uint64_t s = static_cast<uint64_t>(u[j + i])
                           + (i < n ? static_cast<uint64_t>(v[i]) : 0) + c;
                u[j + i] = static_cast<uint32_t>(s % BASE);
                c = s / BASE;
            }
        }

        q[j] = static_cast<uint32_t>(qhat);
    }

    while (q.size() > 1 && q.back() == 0) q.pop_back();

    // Step D8: unnormalise remainder
    std::vector<uint32_t> rem(u.begin(), u.begin() + n);
    if (d > 1) {
        uint64_t r = 0;
        for (int i = n - 1; i >= 0; --i) {
            uint64_t cur = r * BASE + rem[i];
            rem[i] = static_cast<uint32_t>(cur / d);
            r      = cur % d;
        }
    }
    while (rem.size() > 1 && rem.back() == 0) rem.pop_back();

    return {q, rem};
}

std::vector<uint32_t> BigInteger::fromUInt64(uint64_t v) {
    std::vector<uint32_t> r;
    if (v == 0) { r.push_back(0); return r; }
    while (v) { r.push_back(static_cast<uint32_t>(v % BASE)); v /= BASE; }
    return r;
}

std::vector<uint16_t> BigInteger::magnitudeToBinary16(const std::vector<uint32_t>& magnitude) {
    std::vector<uint32_t> remaining = magnitude;
    std::vector<uint16_t> words;
    while (!(remaining.size() == 1 && remaining[0] == 0)) {
        uint32_t remainder = 0;
        remaining = divMagByLimb(remaining, 65536u, remainder);
        words.push_back(static_cast<uint16_t>(remainder));
    }
    if (words.empty()) {
        words.push_back(0);
    }
    return words;
}

std::vector<uint32_t> BigInteger::binary16ToMagnitude(const std::vector<uint16_t>& words) {
    std::vector<uint32_t> magnitude = {0};
    for (auto it = words.rbegin(); it != words.rend(); ++it) {
        magnitude = mulMag(magnitude, {65536u});
        if (*it != 0) {
            magnitude = addMag(magnitude, {static_cast<uint32_t>(*it)});
        }
    }
    return magnitude;
}

std::vector<uint16_t> BigInteger::toTwosComplement16(const BigInteger& value, size_t wordCount) {
    std::vector<uint16_t> words = magnitudeToBinary16(value.mag_);
    words.resize(wordCount, 0);
    if (!value.negative_) {
        return words;
    }

    for (auto& word : words) {
        word = static_cast<uint16_t>(~word);
    }
    uint32_t carry = 1;
    for (auto& word : words) {
        const uint32_t sum = static_cast<uint32_t>(word) + carry;
        word = static_cast<uint16_t>(sum);
        carry = sum >> 16;
        if (carry == 0) {
            break;
        }
    }
    return words;
}

BigInteger BigInteger::fromTwosComplement16(std::vector<uint16_t> words) {
    const bool negative = (words.back() & 0x8000u) != 0;
    if (negative) {
        for (auto& word : words) {
            word = static_cast<uint16_t>(~word);
        }
        uint32_t carry = 1;
        for (auto& word : words) {
            const uint32_t sum = static_cast<uint32_t>(word) + carry;
            word = static_cast<uint16_t>(sum);
            carry = sum >> 16;
            if (carry == 0) {
                break;
            }
        }
    }

    BigInteger result;
    result.mag_ = binary16ToMagnitude(words);
    result.negative_ = negative;
    result.trim();
    return result;
}

BigInteger BigInteger::bitwise(const BigInteger& other, char operation) const {
    const size_t wordCount = std::max(magnitudeToBinary16(mag_).size(),
                                      magnitudeToBinary16(other.mag_).size()) + 1;
    const std::vector<uint16_t> left = toTwosComplement16(*this, wordCount);
    const std::vector<uint16_t> right = toTwosComplement16(other, wordCount);
    std::vector<uint16_t> result(wordCount);
    for (size_t index = 0; index < wordCount; ++index) {
        switch (operation) {
        case '&': result[index] = static_cast<uint16_t>(left[index] & right[index]); break;
        case '|': result[index] = static_cast<uint16_t>(left[index] | right[index]); break;
        default:  result[index] = static_cast<uint16_t>(left[index] ^ right[index]); break;
        }
    }
    return fromTwosComplement16(std::move(result));
}

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

BigInteger::BigInteger() : mag_({0}) {}

BigInteger::BigInteger(intcs v) {
    negative_ = v < 0;
    mag_ = fromUInt64(negative_ ? static_cast<uint64_t>(-(int64_t)v)
                                : static_cast<uint64_t>(v));
}

BigInteger::BigInteger(longcs v) {
    negative_ = v < 0;
    // Negating v directly (`-v`) is undefined behavior in C++ for v == LONGCS_MIN (confirmed
    // via UBSan: "negation of -9223372036854775808 cannot be represented in type 'long int'")
    // -- unlike the intcs constructor above, which safely widens to int64_t before negating
    // (int64_t can represent -INT32_MIN exactly), there is no wider standard integer type to
    // widen into here. Compute the magnitude via well-defined unsigned subtraction instead:
    // `0 - (uint64_t)v` reinterprets v's bit pattern as unsigned first (well-defined in C++20)
    // and performs the two's-complement negation via unsigned wraparound (also well-defined),
    // producing the identical, correct magnitude without ever negating a signed value.
    mag_ = fromUInt64(negative_ ? (0ULL - static_cast<uint64_t>(v))
                                : static_cast<uint64_t>(v));
}

BigInteger::BigInteger(const std::vector<uint8_t>& value, bool isUnsigned, bool isBigEndian) {
    if (value.empty()) {
        mag_ = {0};
        return;
    }

    std::vector<uint8_t> littleEndian = value;
    if (isBigEndian) {
        std::reverse(littleEndian.begin(), littleEndian.end());
    }

    const bool negative = !isUnsigned && (littleEndian.back() & 0x80u) != 0;
    std::vector<uint16_t> words((littleEndian.size() + 1) / 2, 0);
    for (size_t index = 0; index < littleEndian.size(); ++index) {
        words[index / 2] |= static_cast<uint16_t>(littleEndian[index]) << ((index % 2) * 8);
    }
    if (negative && (littleEndian.size() % 2) != 0) {
        words.back() |= 0xFF00u;
    }

    if (negative) {
        *this = fromTwosComplement16(std::move(words));
    } else {
        mag_ = binary16ToMagnitude(words);
        negative_ = false;
        trim();
    }
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

bool BigInteger::getIsZeroProperty()     const { return mag_.size() == 1 && mag_[0] == 0; }
bool BigInteger::getIsOneProperty()      const { return !negative_ && mag_.size() == 1 && mag_[0] == 1; }
bool BigInteger::getIsNegativeProperty() const { return negative_; }
int  BigInteger::Sign()                  const { return getIsZeroProperty() ? 0 : (negative_ ? -1 : 1); }

// ---------------------------------------------------------------------------
// Parse / ToString
// ---------------------------------------------------------------------------

namespace {
    // .NET's own whitespace predicate for number parsing: a space, or 0x09..0x0D
    // (`Number.Parsing.Common.cs:309`). Deliberately NOT std::isspace.
    //
    // HONEST NOTE ON THE EVIDENCE: in the "C" locale those two sets are IDENTICAL, so a mutation
    // replacing this body with `std::isspace(c) != 0` is NOT caught, and was measured not to be.
    // They diverge only after a `std::setlocale` call, and a test that changed the global locale
    // inside a shared test binary would leak that change into every other suite in it. The
    // explicit set is chosen for locale-independence, not because a test distinguishes it.
    bool isParseWhite(unsigned char c) {
        return c == 0x20 || (c >= 0x09 && c <= 0x0D);
    }
}

BigInteger BigInteger::Parse(const std::string& s) {
    // Ticket #2174 (2026-08-18). `Parse(" 1")` and `Parse("1 ")` both threw, and .NET accepts
    // both: `BigInteger.Parse(string)` is `Parse(value, NumberStyles.Integer)`
    // (`BigInteger.cs:798-801`), and `NumberStyles.Integer` is
    // `AllowLeadingWhite | AllowTrailingWhite | AllowLeadingSign`. So the whitespace is not a
    // leniency this port declined -- it is the DEFAULT style's own contract, and rejecting it
    // made `Parse` stricter than the style it claims to implement.
    //
    // The trimmed set is .NET's `IsWhite`, not `std::isspace`: 0x20 and 0x09..0x0D exactly.
    // A vertical tab is inside that range and IS trimmed; a non-breaking space is not.
    std::size_t first = 0, last = s.size();
    while (first < last && isParseWhite(static_cast<unsigned char>(s[first]))) ++first;
    while (last > first && isParseWhite(static_cast<unsigned char>(s[last - 1]))) --last;
    const std::string trimmed = s.substr(first, last - first);

    if (trimmed.empty()) throw System::FormatException("The value could not be parsed.");
    BigInteger r;
    bool neg = (trimmed[0] == '-');
    size_t start = (neg || trimmed[0] == '+') ? 1 : 0;
    if (start >= trimmed.size()) throw System::FormatException("The value could not be parsed.");
    for (size_t k = start; k < trimmed.size(); ++k)
        if (trimmed[k] < '0' || trimmed[k] > '9')
            throw System::FormatException("The value could not be parsed.");

    std::vector<uint32_t> m;
    int i = static_cast<int>(trimmed.size());
    while (i > static_cast<int>(start)) {
        int from = std::max(static_cast<int>(start), i - 9);
        m.push_back(static_cast<uint32_t>(std::stoul(trimmed.substr(from, i - from))));
        i = from;
    }
    if (m.empty()) m.push_back(0);
    r.mag_      = m;
    r.negative_ = neg;
    r.trim();
    return r;
}

bool BigInteger::TryParse(const std::string& s, BigInteger& result) {
    try { result = Parse(s); return true; }
    catch (...) { result = BigInteger(0); return false; }
}

std::string BigInteger::ToString() const {
    if (getIsZeroProperty()) return "0";
    std::string s;
    for (int i = static_cast<int>(mag_.size()) - 1; i >= 0; --i) {
        if (s.empty()) {
            s += std::to_string(mag_[i]);
        } else {
            std::string chunk = std::to_string(mag_[i]);
            s += std::string(9 - chunk.size(), '0') + chunk;
        }
    }
    return negative_ ? "-" + s : s;
}

std::vector<uint8_t> BigInteger::ToByteArray(bool isUnsigned, bool isBigEndian) const {
    if (isUnsigned && negative_) {
        throw System::OverflowException("A negative value cannot be represented as unsigned bytes.");
    }

    std::vector<uint16_t> words;
    if (negative_) {
        words = toTwosComplement16(*this, magnitudeToBinary16(mag_).size() + 1);
    } else {
        words = magnitudeToBinary16(mag_);
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(words.size() * 2);
    for (const uint16_t word : words) {
        bytes.push_back(static_cast<uint8_t>(word));
        bytes.push_back(static_cast<uint8_t>(word >> 8));
    }

    if (negative_) {
        while (bytes.size() > 1 && bytes.back() == 0xFFu &&
               (bytes[bytes.size() - 2] & 0x80u) != 0) {
            bytes.pop_back();
        }
    } else {
        while (bytes.size() > 1 && bytes.back() == 0) {
            bytes.pop_back();
        }
        if (!isUnsigned && (bytes.back() & 0x80u) != 0) {
            bytes.push_back(0);
        }
    }

    if (isBigEndian) {
        std::reverse(bytes.begin(), bytes.end());
    }
    return bytes;
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

BigInteger BigInteger::operator-() const {
    BigInteger r(*this);
    if (!getIsZeroProperty()) r.negative_ = !negative_;
    return r;
}

BigInteger BigInteger::Abs() const {
    BigInteger r(*this); r.negative_ = false; return r;
}

BigInteger BigInteger::operator+(const BigInteger& o) const {
    if (negative_ == o.negative_) {
        BigInteger r; r.negative_ = negative_; r.mag_ = addMag(mag_, o.mag_); r.trim();
        return r;
    }
    int c = cmpMag(mag_, o.mag_);
    if (c == 0) return BigInteger(0);
    BigInteger r;
    if (c > 0) { r.negative_ = negative_;   r.mag_ = subMag(mag_, o.mag_); }
    else       { r.negative_ = o.negative_; r.mag_ = subMag(o.mag_, mag_); }
    r.trim();
    return r;
}

BigInteger BigInteger::operator-(const BigInteger& o) const { return *this + (-o); }

BigInteger BigInteger::operator*(const BigInteger& o) const {
    BigInteger r;
    r.negative_ = negative_ != o.negative_;
    r.mag_ = mulMag(mag_, o.mag_);
    r.trim();
    return r;
}

BigInteger BigInteger::operator/(const BigInteger& o) const {
    if (o.getIsZeroProperty()) throw System::DivideByZeroException("Attempted to divide by zero.");
    if (getIsZeroProperty())   return BigInteger(0);
    int c = cmpMag(mag_, o.mag_);
    if (c < 0) return BigInteger(0);
    if (c == 0) { BigInteger r(1); r.negative_ = negative_ != o.negative_; return r; }

    std::vector<uint32_t> q;
    if (o.mag_.size() == 1) {
        uint32_t rem;
        q = divMagByLimb(mag_, o.mag_[0], rem);
    } else {
        q = divmodMag(mag_, o.mag_).first;
    }
    BigInteger r;
    r.mag_      = q;
    r.negative_ = negative_ != o.negative_;
    r.trim();
    return r;
}

BigInteger BigInteger::operator%(const BigInteger& o) const {
    if (o.getIsZeroProperty()) throw System::DivideByZeroException("Attempted to divide by zero.");
    if (getIsZeroProperty())   return BigInteger(0);
    int c = cmpMag(mag_, o.mag_);
    if (c < 0) return *this;  // |dividend| < |divisor| → remainder = dividend
    if (c == 0) return BigInteger(0);

    std::vector<uint32_t> rem;
    if (o.mag_.size() == 1) {
        uint32_t r;
        divMagByLimb(mag_, o.mag_[0], r);
        rem = {r};
    } else {
        rem = divmodMag(mag_, o.mag_).second;
    }
    BigInteger r;
    r.mag_      = rem;
    r.negative_ = negative_; // remainder has the sign of the dividend (.NET semantics)
    r.trim();
    return r;
}

BigInteger BigInteger::operator&(const BigInteger& o) const { return bitwise(o, '&'); }
BigInteger BigInteger::operator|(const BigInteger& o) const { return bitwise(o, '|'); }
BigInteger BigInteger::operator^(const BigInteger& o) const { return bitwise(o, '^'); }

BigInteger BigInteger::operator~() const {
    const size_t wordCount = magnitudeToBinary16(mag_).size() + 1;
    std::vector<uint16_t> words = toTwosComplement16(*this, wordCount);
    for (auto& word : words) {
        word = static_cast<uint16_t>(~word);
    }
    return fromTwosComplement16(std::move(words));
}

BigInteger BigInteger::operator<<(intcs shift) const {
    if (shift == 0 || getIsZeroProperty()) {
        return *this;
    }
    if (shift == std::numeric_limits<intcs>::min()) {
        return (*this >> std::numeric_limits<intcs>::max()) >> 1;
    }
    if (shift < 0) {
        return *this >> -shift;
    }

    const size_t wordShift = static_cast<size_t>(shift / 16);
    const uint32_t bitShift = static_cast<uint32_t>(shift % 16);
    const std::vector<uint16_t> source = magnitudeToBinary16(mag_);
    std::vector<uint16_t> words(wordShift, 0);
    uint32_t carry = 0;
    for (const uint16_t word : source) {
        const uint32_t shifted = (static_cast<uint32_t>(word) << bitShift) | carry;
        words.push_back(static_cast<uint16_t>(shifted));
        carry = shifted >> 16;
    }
    if (carry != 0) {
        words.push_back(static_cast<uint16_t>(carry));
    }

    BigInteger result;
    result.mag_ = binary16ToMagnitude(words);
    result.negative_ = negative_;
    result.trim();
    return result;
}

BigInteger BigInteger::operator>>(intcs shift) const {
    if (shift == 0 || getIsZeroProperty()) {
        return *this;
    }
    if (shift == std::numeric_limits<intcs>::min()) {
        return (*this << 1) << std::numeric_limits<intcs>::max();
    }
    if (shift < 0) {
        return *this << -shift;
    }

    const size_t wordShift = static_cast<size_t>(shift / 16);
    const uint32_t bitShift = static_cast<uint32_t>(shift % 16);
    const std::vector<uint16_t> source = magnitudeToBinary16(mag_);
    if (wordShift >= source.size()) {
        return negative_ ? BigInteger(-1) : BigInteger::Zero;
    }

    bool remainder = false;
    for (size_t index = 0; index < wordShift; ++index) {
        remainder = remainder || source[index] != 0;
    }

    std::vector<uint16_t> words(source.begin() + wordShift, source.end());
    if (bitShift != 0) {
        const uint32_t lowerMask = (1u << bitShift) - 1u;
        remainder = remainder || (words.front() & lowerMask) != 0;
        for (size_t index = 0; index < words.size(); ++index) {
            const uint32_t next = index + 1 < words.size() ? words[index + 1] : 0;
            words[index] = static_cast<uint16_t>((words[index] >> bitShift) |
                                                 (next << (16 - bitShift)));
        }
    }

    BigInteger result;
    result.mag_ = binary16ToMagnitude(words);
    result.negative_ = negative_;
    if (negative_ && remainder) {
        result.mag_ = addMag(result.mag_, {1});
    }
    result.trim();
    return result;
}

BigInteger& BigInteger::operator+=(const BigInteger& o) { *this = *this + o; return *this; }
BigInteger& BigInteger::operator-=(const BigInteger& o) { *this = *this - o; return *this; }
BigInteger& BigInteger::operator*=(const BigInteger& o) { *this = *this * o; return *this; }
BigInteger& BigInteger::operator/=(const BigInteger& o) { *this = *this / o; return *this; }
BigInteger& BigInteger::operator%=(const BigInteger& o) { *this = *this % o; return *this; }
BigInteger& BigInteger::operator&=(const BigInteger& o) { *this = *this & o; return *this; }
BigInteger& BigInteger::operator|=(const BigInteger& o) { *this = *this | o; return *this; }
BigInteger& BigInteger::operator^=(const BigInteger& o) { *this = *this ^ o; return *this; }
BigInteger& BigInteger::operator<<=(intcs shift) { *this = *this << shift; return *this; }
BigInteger& BigInteger::operator>>=(intcs shift) { *this = *this >> shift; return *this; }

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

bool BigInteger::operator==(const BigInteger& o) const {
    return negative_ == o.negative_ && mag_ == o.mag_;
}
bool BigInteger::operator!=(const BigInteger& o) const { return !(*this == o); }
bool BigInteger::operator<(const BigInteger& o) const {
    if (negative_ != o.negative_) return negative_;
    int c = cmpMag(mag_, o.mag_);
    return negative_ ? c > 0 : c < 0;
}
bool BigInteger::operator<=(const BigInteger& o) const { return !(o < *this); }
bool BigInteger::operator> (const BigInteger& o) const { return   o < *this;  }
bool BigInteger::operator>=(const BigInteger& o) const { return !(*this < o); }

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const BigInteger BigInteger::Zero    {  0 };
const BigInteger BigInteger::One     {  1 };
const BigInteger BigInteger::MinusOne{ -1 };

} // namespace System::Numerics
