#pragma once

#include "pid.h"

#include <iostream>

namespace pid {
    template <typename T>
    struct pid_type
        : std::conditional<
              std::is_arithmetic<T>::value || std::is_enum<T>::value, T, pid::relative_ptr<T>>
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

    // TODO: maps
}