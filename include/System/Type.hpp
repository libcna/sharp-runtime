#pragma once

#include <string>
#include <typeinfo>

namespace System
{
    /**
     * @brief Represents type declarations — classes, interfaces, arrays, value types, enumerations.
     *
     * Minimal C++ counterpart of the .NET System.Type class, sufficient for
     * use as a service key in GameServiceContainer and similar patterns.
     *
     * Unlike .NET reflection, this implementation identifies types by their
     * C++ RTTI type_info, accessible via the static From<T>() factory.
     */
    class Type
    {
    private:
        const std::type_info* info_;

        explicit Type(const std::type_info& info) : info_(&info) {}

    public:
        Type() : info_(nullptr) {}

        /// Creates a Type instance representing the template parameter T.
        template<typename T>
        static Type From()
        {
            return Type(typeid(T));
        }

        /// Returns the name of the type as reported by RTTI.
        [[nodiscard]] std::string getName() const
        {
            return info_ ? info_->name() : "(null)";
        }

        [[nodiscard]] bool operator==(const Type& other) const
        {
            if (!info_ || !other.info_) return info_ == other.info_;
            return *info_ == *other.info_;
        }

        [[nodiscard]] bool operator!=(const Type& other) const { return !(*this == other); }

        /// Returns the underlying std::type_info for use in hash maps etc.
        [[nodiscard]] const std::type_info* getTypeInfo() const { return info_; }
    };
}

namespace std
{
    template<>
    struct hash<System::Type>
    {
        std::size_t operator()(const System::Type& t) const noexcept
        {
            return t.getTypeInfo() ? t.getTypeInfo()->hash_code() : 0;
        }
    };
}
