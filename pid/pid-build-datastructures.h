#pragma once

#include "pid.h"
#include "builder.h"

#include <iostream>
#include <map>
#include <optional>
#include <any>
#include <atomic>

namespace pid {
    template <typename T>
    struct pid_type;

    template <typename T>
    struct pid_base_type
    {
        using type = T;
    };

    template <typename T>
    struct pid_base_type<std::optional<T>>
    {
        // nullopt will just be represented by a null pointer
        using type = typename std::conditional<
            std::is_arithmetic<T>::value || std::is_enum<T>::value, std::optional<T>,
            typename pid::pid_base_type<T>::type>::type;
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

    template <typename Key, typename Value>
    struct pid_base_type<std::map<Key, Value>>
    {
        using type = pid::map32<typename pid_type<Key>::type, typename pid_type<Value>::type>;
    };

    template <>
    struct pid_type<std::string>
    {
        using type = pid::string32;
    };

    template <typename T>
    struct pid_type : std::conditional<
                          std::is_arithmetic<T>::value || std::is_enum<T>::value, T,
                          // TODO: make offset type configurable
                          pid::ptr<typename pid_base_type<T>::type, std::int32_t>>
    {
    };

    template <typename T>
    struct pid_type<std::optional<T>>
        : std::conditional<
              std::is_arithmetic<T>::value || std::is_enum<T>::value, std::optional<T>,
              // TODO: make offset type configurable
              pid::ptr<typename pid_base_type<T>::type, std::int32_t>>
    {
    };

    struct datastructure_builder
    {
        pid::builder & b;
        std::map<std::size_t, std::any> caches{};

        static std::size_t next_cache_index()
        {
            static std::atomic<std::size_t> next_index{0};
            return next_index++;
        }

        template <typename T>
        static std::size_t cache_index()
        {
            static const auto result{next_cache_index()};
            return result;
        }

        template <typename T>
        using CacheType = std::map<
            T, decltype(std::declval<datastructure_builder>()(std::declval<T>())), std::less<>>;

        template <typename T>
        CacheType<T> & get_cache()
        {
            const auto index{cache_index<T>()};
            auto it{caches.find(index)};
            if (it == caches.end()) {
                it = caches.insert(std::make_pair(index, CacheType<T>{})).first;
            }

            return std::any_cast<CacheType<T> &>(it->second);
        }

        template <
            typename T, typename = std::enable_if<
                            std::is_arithmetic<T>::value || std::is_enum<T>::value, bool>>
        T operator()(T value)
        {
            return value;
        }

        inline builder_offset<generic_string_data<std::uint32_t>> operator()(const std::string & s)
        {
            auto & cache{get_cache<std::string>()};
            auto it{cache.find(s)};
            if (it == cache.end()) {
                it = cache.insert(std::make_pair(s, b.add_string(s))).first;
            }
            return it->second;
        }

        template <typename T>
        auto operator()(const std::optional<T> & o)
        {
            if constexpr (std::is_arithmetic<T>::value || std::is_enum<T>::value) {
                return o;
            } else {
                if (o) {
                    return (*this)(*o);
                } else {
                    return builder_offset<typename pid_base_type<T>::type>{b};
                }
            }
        }

        auto operator()(const std::optional<std::string> & o)
        {
            // TODO: we could also try to store this as an std::optional<pid::string32>
            if (o) {
                const builder_offset<generic_string_data<std::uint32_t>> data{(*this)(*o)};
                builder_offset<string32> result{b.add<string32>()};
                result->data = data;
                return result;
            } else {
                return builder_offset<generic_string<std::int32_t, std::uint32_t>>{b};
            }
        }

        template <typename T>
        inline builder_offset<vector32<typename pid_type<T>::type>> operator()(
            const std::vector<T> & v)
        {
            auto & cache{get_cache<std::vector<T>>()};
            auto it{cache.find(v)};
            if (it == cache.end()) {
                it = cache.insert(std::make_pair(v, build(v))).first;
            }
            return it->second;
        }

        template <typename T>
        inline builder_offset<vector32<typename pid_type<T>::type>> build(const std::vector<T> & v)
        {
            builder_offset<vector32<typename pid_type<T>::type>> result =
                b.add_vector<typename pid_type<T>::type, std::uint32_t>(v.size());

            for (std::size_t index{0}; index < v.size(); ++index) {
                (*result)[index] = (*this)(v[index]);
            }

            return result;
        }

        template <typename Key, typename Value>
        builder_offset<map32<typename pid_type<Key>::type, typename pid_type<Value>::type>>
        operator()(const std::map<Key, Value> & m)
        {
            auto result{b.add_map<
                typename pid_type<Key>::type, typename pid_type<Value>::type, std::uint32_t>(
                m.size())};

            for (const auto & [key, value] : m) {
                *result.add_key((*this)(key)) = (*this)(value);
            }

            return result.offset();
        }
    };

}
