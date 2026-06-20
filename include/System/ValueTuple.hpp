// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <sstream>
#include <string>
#include <tuple>

namespace System {

namespace detail {

    inline size_t vtHashCombine(size_t seed, size_t h) noexcept {
        return seed ^ (h + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    }

    template<typename T>
    std::string vtItemStr(const T& v) {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }

    template<typename T>
    size_t vtHash(const T& v) noexcept {
        return std::hash<T>{}(v);
    }

} // namespace detail

// ===========================================================================
// ValueTuple1<T1>  —  System.ValueTuple<T1>
// ===========================================================================

/**
 * @brief Value-semantic single-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1>.
 */
template<typename T1>
struct ValueTuple1 {
    T1 Item1; ///< The first (and only) element.

    /** @brief Default-constructs all elements. */
    ValueTuple1() = default;

    /** @brief Constructs a ValueTuple1 with the specified element. */
    explicit ValueTuple1(T1 item1) : Item1(std::move(item1)) {}

    /** @brief Determines whether this instance equals another. */
    [[nodiscard]] bool Equals(const ValueTuple1& other) const {
        return Item1 == other.Item1;
    }

    /**
     * @brief Returns a hash code for this instance.
     *
     * C++ counterpart of .NET ValueTuple<T1>.GetHashCode().
     */
    [[nodiscard]] int GetHashCode() const noexcept {
        return static_cast<int>(detail::vtHash(Item1));
    }

    /**
     * @brief Compares this instance to another ValueTuple1.
     * @return -1 if less, 0 if equal, 1 if greater.
     */
    [[nodiscard]] int CompareTo(const ValueTuple1& other) const {
        if (Item1 < other.Item1) return -1;
        if (other.Item1 < Item1) return  1;
        return 0;
    }

    /**
     * @brief Returns a string that represents the value of this instance.
     *
     * C++ counterpart of .NET ValueTuple<T1>.ToString() — formats as "(Item1)".
     */
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ")";
    }

    bool operator==(const ValueTuple1& o) const { return Item1 == o.Item1; }
    bool operator!=(const ValueTuple1& o) const { return !(*this == o); }
    bool operator< (const ValueTuple1& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple1& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple1& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple1& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple2<T1,T2>  —  System.ValueTuple<T1,T2>
// ===========================================================================

/**
 * @brief Value-semantic two-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2>.
 */
template<typename T1, typename T2>
struct ValueTuple2 {
    T1 Item1; ///< The first element.
    T2 Item2; ///< The second element.

    /** @brief Default-constructs all elements. */
    ValueTuple2() = default;

    /** @brief Constructs a ValueTuple2 with the specified elements. */
    ValueTuple2(T1 i1, T2 i2) : Item1(std::move(i1)), Item2(std::move(i2)) {}

    /** @brief Determines whether this instance equals another. */
    [[nodiscard]] bool Equals(const ValueTuple2& other) const {
        return Item1 == other.Item1 && Item2 == other.Item2;
    }

    /**
     * @brief Returns a hash code for this instance.
     *
     * C++ counterpart of .NET ValueTuple<T1,T2>.GetHashCode().
     */
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        return static_cast<int>(h);
    }

    /**
     * @brief Compares this instance to another ValueTuple2 lexicographically.
     * @return -1 if less, 0 if equal, 1 if greater.
     */
    [[nodiscard]] int CompareTo(const ValueTuple2& other) const {
        if (Item1 < other.Item1) return -1;
        if (other.Item1 < Item1) return  1;
        if (Item2 < other.Item2) return -1;
        if (other.Item2 < Item2) return  1;
        return 0;
    }

    /**
     * @brief Returns a string that represents the value of this instance.
     *
     * C++ counterpart of .NET ValueTuple<T1,T2>.ToString() — formats as "(Item1, Item2)".
     */
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " + detail::vtItemStr(Item2) + ")";
    }

    bool operator==(const ValueTuple2& o) const { return Item1 == o.Item1 && Item2 == o.Item2; }
    bool operator!=(const ValueTuple2& o) const { return !(*this == o); }
    bool operator< (const ValueTuple2& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple2& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple2& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple2& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple3<T1,T2,T3>  —  System.ValueTuple<T1,T2,T3>
// ===========================================================================

/**
 * @brief Value-semantic three-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2, T3>.
 */
template<typename T1, typename T2, typename T3>
struct ValueTuple3 {
    T1 Item1; ///< The first element.
    T2 Item2; ///< The second element.
    T3 Item3; ///< The third element.

    /** @brief Default-constructs all elements. */
    ValueTuple3() = default;

    /** @brief Constructs a ValueTuple3 with the specified elements. */
    ValueTuple3(T1 i1, T2 i2, T3 i3)
        : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)) {}

    /** @brief Determines whether this instance equals another. */
    [[nodiscard]] bool Equals(const ValueTuple3& other) const {
        return Item1 == other.Item1 && Item2 == other.Item2 && Item3 == other.Item3;
    }

    /** @brief Returns a hash code for this instance. */
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        h = detail::vtHashCombine(h, detail::vtHash(Item3));
        return static_cast<int>(h);
    }

    /**
     * @brief Compares this instance to another ValueTuple3 lexicographically.
     * @return -1 if less, 0 if equal, 1 if greater.
     */
    [[nodiscard]] int CompareTo(const ValueTuple3& other) const {
        if (Item1 < other.Item1) return -1;
        if (other.Item1 < Item1) return  1;
        if (Item2 < other.Item2) return -1;
        if (other.Item2 < Item2) return  1;
        if (Item3 < other.Item3) return -1;
        if (other.Item3 < Item3) return  1;
        return 0;
    }

    /** @brief Returns a string that represents the value of this instance. */
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " +
                     detail::vtItemStr(Item2) + ", " +
                     detail::vtItemStr(Item3) + ")";
    }

    bool operator==(const ValueTuple3& o) const {
        return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3;
    }
    bool operator!=(const ValueTuple3& o) const { return !(*this == o); }
    bool operator< (const ValueTuple3& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple3& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple3& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple3& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple4<T1,T2,T3,T4>  —  System.ValueTuple<T1,T2,T3,T4>
// ===========================================================================

/**
 * @brief Value-semantic four-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2, T3, T4>.
 */
template<typename T1, typename T2, typename T3, typename T4>
struct ValueTuple4 {
    T1 Item1; ///< The first element.
    T2 Item2; ///< The second element.
    T3 Item3; ///< The third element.
    T4 Item4; ///< The fourth element.

    /** @brief Default-constructs all elements. */
    ValueTuple4() = default;

    /** @brief Constructs a ValueTuple4 with the specified elements. */
    ValueTuple4(T1 i1, T2 i2, T3 i3, T4 i4)
        : Item1(std::move(i1)), Item2(std::move(i2)),
          Item3(std::move(i3)), Item4(std::move(i4)) {}

    /** @brief Determines whether this instance equals another. */
    [[nodiscard]] bool Equals(const ValueTuple4& other) const {
        return Item1==other.Item1 && Item2==other.Item2 &&
               Item3==other.Item3 && Item4==other.Item4;
    }

    /** @brief Returns a hash code for this instance. */
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        h = detail::vtHashCombine(h, detail::vtHash(Item3));
        h = detail::vtHashCombine(h, detail::vtHash(Item4));
        return static_cast<int>(h);
    }

    /**
     * @brief Compares this instance to another ValueTuple4 lexicographically.
     * @return -1 if less, 0 if equal, 1 if greater.
     */
    [[nodiscard]] int CompareTo(const ValueTuple4& other) const {
        if (Item1 < other.Item1) return -1; if (other.Item1 < Item1) return 1;
        if (Item2 < other.Item2) return -1; if (other.Item2 < Item2) return 1;
        if (Item3 < other.Item3) return -1; if (other.Item3 < Item3) return 1;
        if (Item4 < other.Item4) return -1; if (other.Item4 < Item4) return 1;
        return 0;
    }

    /** @brief Returns a string that represents the value of this instance. */
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " +
                     detail::vtItemStr(Item2) + ", " +
                     detail::vtItemStr(Item3) + ", " +
                     detail::vtItemStr(Item4) + ")";
    }

    bool operator==(const ValueTuple4& o) const {
        return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3 && Item4==o.Item4;
    }
    bool operator!=(const ValueTuple4& o) const { return !(*this == o); }
    bool operator< (const ValueTuple4& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple4& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple4& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple4& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple5<T1..T5>  —  System.ValueTuple<T1,T2,T3,T4,T5>
// ===========================================================================

/**
 * @brief Value-semantic five-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2, T3, T4, T5>.
 */
template<typename T1, typename T2, typename T3, typename T4, typename T5>
struct ValueTuple5 {
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5;

    ValueTuple5() = default;
    ValueTuple5(T1 i1, T2 i2, T3 i3, T4 i4, T5 i5)
        : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)),
          Item4(std::move(i4)), Item5(std::move(i5)) {}

    [[nodiscard]] bool Equals(const ValueTuple5& o) const {
        return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3 &&
               Item4==o.Item4 && Item5==o.Item5;
    }
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        h = detail::vtHashCombine(h, detail::vtHash(Item3));
        h = detail::vtHashCombine(h, detail::vtHash(Item4));
        h = detail::vtHashCombine(h, detail::vtHash(Item5));
        return static_cast<int>(h);
    }
    [[nodiscard]] int CompareTo(const ValueTuple5& o) const {
        if (Item1 < o.Item1) return -1; if (o.Item1 < Item1) return 1;
        if (Item2 < o.Item2) return -1; if (o.Item2 < Item2) return 1;
        if (Item3 < o.Item3) return -1; if (o.Item3 < Item3) return 1;
        if (Item4 < o.Item4) return -1; if (o.Item4 < Item4) return 1;
        if (Item5 < o.Item5) return -1; if (o.Item5 < Item5) return 1;
        return 0;
    }
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " + detail::vtItemStr(Item2) + ", " +
                     detail::vtItemStr(Item3) + ", " + detail::vtItemStr(Item4) + ", " +
                     detail::vtItemStr(Item5) + ")";
    }
    bool operator==(const ValueTuple5& o) const { return Equals(o); }
    bool operator!=(const ValueTuple5& o) const { return !Equals(o); }
    bool operator< (const ValueTuple5& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple5& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple5& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple5& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple6<T1..T6>  —  System.ValueTuple<T1,T2,T3,T4,T5,T6>
// ===========================================================================

/**
 * @brief Value-semantic six-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2, T3, T4, T5, T6>.
 */
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct ValueTuple6 {
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5; T6 Item6;

    ValueTuple6() = default;
    ValueTuple6(T1 i1, T2 i2, T3 i3, T4 i4, T5 i5, T6 i6)
        : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)),
          Item4(std::move(i4)), Item5(std::move(i5)), Item6(std::move(i6)) {}

    [[nodiscard]] bool Equals(const ValueTuple6& o) const {
        return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3 &&
               Item4==o.Item4 && Item5==o.Item5 && Item6==o.Item6;
    }
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        h = detail::vtHashCombine(h, detail::vtHash(Item3));
        h = detail::vtHashCombine(h, detail::vtHash(Item4));
        h = detail::vtHashCombine(h, detail::vtHash(Item5));
        h = detail::vtHashCombine(h, detail::vtHash(Item6));
        return static_cast<int>(h);
    }
    [[nodiscard]] int CompareTo(const ValueTuple6& o) const {
        if (Item1 < o.Item1) return -1; if (o.Item1 < Item1) return 1;
        if (Item2 < o.Item2) return -1; if (o.Item2 < Item2) return 1;
        if (Item3 < o.Item3) return -1; if (o.Item3 < Item3) return 1;
        if (Item4 < o.Item4) return -1; if (o.Item4 < Item4) return 1;
        if (Item5 < o.Item5) return -1; if (o.Item5 < Item5) return 1;
        if (Item6 < o.Item6) return -1; if (o.Item6 < Item6) return 1;
        return 0;
    }
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " + detail::vtItemStr(Item2) + ", " +
                     detail::vtItemStr(Item3) + ", " + detail::vtItemStr(Item4) + ", " +
                     detail::vtItemStr(Item5) + ", " + detail::vtItemStr(Item6) + ")";
    }
    bool operator==(const ValueTuple6& o) const { return Equals(o); }
    bool operator!=(const ValueTuple6& o) const { return !Equals(o); }
    bool operator< (const ValueTuple6& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple6& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple6& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple6& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// ValueTuple7<T1..T7>  —  System.ValueTuple<T1,T2,T3,T4,T5,T6,T7>
// ===========================================================================

/**
 * @brief Value-semantic seven-element tuple.
 *
 * C++ counterpart of .NET System.ValueTuple<T1, T2, T3, T4, T5, T6, T7>.
 */
template<typename T1, typename T2, typename T3, typename T4,
         typename T5, typename T6, typename T7>
struct ValueTuple7 {
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5; T6 Item6; T7 Item7;

    ValueTuple7() = default;
    ValueTuple7(T1 i1, T2 i2, T3 i3, T4 i4, T5 i5, T6 i6, T7 i7)
        : Item1(std::move(i1)), Item2(std::move(i2)), Item3(std::move(i3)),
          Item4(std::move(i4)), Item5(std::move(i5)), Item6(std::move(i6)),
          Item7(std::move(i7)) {}

    [[nodiscard]] bool Equals(const ValueTuple7& o) const {
        return Item1==o.Item1 && Item2==o.Item2 && Item3==o.Item3 && Item4==o.Item4 &&
               Item5==o.Item5 && Item6==o.Item6 && Item7==o.Item7;
    }
    [[nodiscard]] int GetHashCode() const noexcept {
        size_t h = detail::vtHash(Item1);
        h = detail::vtHashCombine(h, detail::vtHash(Item2));
        h = detail::vtHashCombine(h, detail::vtHash(Item3));
        h = detail::vtHashCombine(h, detail::vtHash(Item4));
        h = detail::vtHashCombine(h, detail::vtHash(Item5));
        h = detail::vtHashCombine(h, detail::vtHash(Item6));
        h = detail::vtHashCombine(h, detail::vtHash(Item7));
        return static_cast<int>(h);
    }
    [[nodiscard]] int CompareTo(const ValueTuple7& o) const {
        if (Item1 < o.Item1) return -1; if (o.Item1 < Item1) return 1;
        if (Item2 < o.Item2) return -1; if (o.Item2 < Item2) return 1;
        if (Item3 < o.Item3) return -1; if (o.Item3 < Item3) return 1;
        if (Item4 < o.Item4) return -1; if (o.Item4 < Item4) return 1;
        if (Item5 < o.Item5) return -1; if (o.Item5 < Item5) return 1;
        if (Item6 < o.Item6) return -1; if (o.Item6 < Item6) return 1;
        if (Item7 < o.Item7) return -1; if (o.Item7 < Item7) return 1;
        return 0;
    }
    [[nodiscard]] std::string ToString() const {
        return "(" + detail::vtItemStr(Item1) + ", " + detail::vtItemStr(Item2) + ", " +
                     detail::vtItemStr(Item3) + ", " + detail::vtItemStr(Item4) + ", " +
                     detail::vtItemStr(Item5) + ", " + detail::vtItemStr(Item6) + ", " +
                     detail::vtItemStr(Item7) + ")";
    }
    bool operator==(const ValueTuple7& o) const { return Equals(o); }
    bool operator!=(const ValueTuple7& o) const { return !Equals(o); }
    bool operator< (const ValueTuple7& o) const { return CompareTo(o) < 0; }
    bool operator<=(const ValueTuple7& o) const { return CompareTo(o) <= 0; }
    bool operator> (const ValueTuple7& o) const { return CompareTo(o) > 0; }
    bool operator>=(const ValueTuple7& o) const { return CompareTo(o) >= 0; }
};

// ===========================================================================
// Factory helpers  —  System.ValueTuple.Create(...)
// ===========================================================================

/** @brief Creates a ValueTuple1 containing the specified value. */
template<typename T1>
ValueTuple1<T1> MakeValueTuple(T1 i1) { return ValueTuple1<T1>(std::move(i1)); }

/** @brief Creates a ValueTuple2 containing the specified values. */
template<typename T1, typename T2>
ValueTuple2<T1,T2> MakeValueTuple(T1 i1, T2 i2) {
    return {std::move(i1), std::move(i2)};
}

/** @brief Creates a ValueTuple3 containing the specified values. */
template<typename T1, typename T2, typename T3>
ValueTuple3<T1,T2,T3> MakeValueTuple(T1 i1, T2 i2, T3 i3) {
    return {std::move(i1), std::move(i2), std::move(i3)};
}

/** @brief Creates a ValueTuple4 containing the specified values. */
template<typename T1, typename T2, typename T3, typename T4>
ValueTuple4<T1,T2,T3,T4> MakeValueTuple(T1 i1, T2 i2, T3 i3, T4 i4) {
    return {std::move(i1), std::move(i2), std::move(i3), std::move(i4)};
}

/** @brief Creates a ValueTuple5 containing the specified values. */
template<typename T1, typename T2, typename T3, typename T4, typename T5>
ValueTuple5<T1,T2,T3,T4,T5> MakeValueTuple(T1 i1, T2 i2, T3 i3, T4 i4, T5 i5) {
    return {std::move(i1), std::move(i2), std::move(i3), std::move(i4), std::move(i5)};
}

/** @brief Creates a ValueTuple6 containing the specified values. */
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
ValueTuple6<T1,T2,T3,T4,T5,T6> MakeValueTuple(T1 i1, T2 i2, T3 i3, T4 i4, T5 i5, T6 i6) {
    return {std::move(i1), std::move(i2), std::move(i3),
            std::move(i4), std::move(i5), std::move(i6)};
}

/** @brief Creates a ValueTuple7 containing the specified values. */
template<typename T1, typename T2, typename T3, typename T4,
         typename T5, typename T6, typename T7>
ValueTuple7<T1,T2,T3,T4,T5,T6,T7> MakeValueTuple(T1 i1, T2 i2, T3 i3, T4 i4,
                                                    T5 i5, T6 i6, T7 i7) {
    return {std::move(i1), std::move(i2), std::move(i3), std::move(i4),
            std::move(i5), std::move(i6), std::move(i7)};
}

} // namespace System
