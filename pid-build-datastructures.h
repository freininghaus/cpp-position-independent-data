#pragma once

#include "pid.h"

#include <iostream>
#include <map>

namespace pid {
    template <typename T>
    struct pid_type;

    template <typename T>
    struct pid_base_type
    {
        using type = T;
    };

    template <>
    struct pid_base_type<std::string>
    {
        using type = pid::string32;
    };

    template <typename T>
    struct pid_base_type<std::vector<T>>
    {
        using type = pid::vector32<typename pid_type<T>::type>;
    };

    template <typename T>
    struct pid_type : std::conditional<
                          std::is_arithmetic<T>::value || std::is_enum<T>::value, T,
                          pid::relative_ptr<typename pid_base_type<T>::type>>
    {
    };

    template <
        typename T,
        typename = std::enable_if<std::is_arithmetic<T>::value || std::is_enum<T>::value, bool>>
    T build(pid::builder &, T value)
    {
        return value;
    }

    inline builder_offset<string32> build(pid::builder & b, const std::string & s)
    {
        return b.add_string(s);
    }

    template <typename T>
    inline builder_offset<vector32<typename pid_type<T>::type>> build(
        pid::builder & b, const std::vector<T> & v)
    {
        builder_offset<vector32<typename pid_type<T>::type>> result =
            b.add_vector<typename pid_type<T>::type, std::uint32_t>(v.size());

        for (std::size_t index{0}; index < v.size(); ++index) {
            (*result)[index] = build(b, v[index]);
        }

        return result;
    }

    template <typename Key, typename Value>
    builder_offset<map32<typename pid_type<Key>::type, typename pid_type<Value>::type>> build(
        pid::builder & b, const std::map<Key, Value> & m)
    {
        auto result{
            b.add_map<typename pid_type<Key>::type, typename pid_type<Value>::type, std::uint32_t>(
                m.size())};

        for (const auto & [key, value] : m) {
            *result.add_key(build(b, key)) = build(b, value);
        }

        return result.offset();
    }
}