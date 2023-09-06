#pragma once

#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>

namespace pid {
    template <typename T, typename offset_type = std::int32_t>
    struct offset
    {
        offset_type offset;

        auto & operator=(const T * p)
        {
            offset = reinterpret_cast<const char *>(p) - reinterpret_cast<const char *>(this);
            return *this;
        }

        T & operator*()
        {
            return *operator->();
        }

        const T & operator*() const
        {
            return *operator->();
        }

        T * operator->()
        {
            return get<T *>();
        }

        const T * operator->() const
        {
            return get<const T *>();
        }

        template <typename Pointer>
        Pointer get() const
        {
            return reinterpret_cast<Pointer>(reinterpret_cast<const char *>(this) + offset);
        }
    };

    template <typename SizeType = std::uint32_t>
    struct string
    {
        SizeType size;
        char data[];
    };

    template <typename T>
    struct builder_offset;

    struct builder
    {
        std::vector<char> data;

        template <typename T>
        builder_offset<T> add()
        {
            // TODO: alignment!
            const auto offset{data.size()};
            data.resize(data.size() + sizeof(T));
            return {*this, offset};
        }

        template <typename SizeType = std::uint32_t>
        builder_offset<string<SizeType>> add_string(std::string_view s)
        {
            const auto size = (sizeof(SizeType) + s.size() + 1 + 3) / 4 * 4;
            const auto offset{data.size()};
            data.resize(data.size() + size);
            builder_offset<string<SizeType>> result{*this, offset};
            result->size = s.size();
            std::memcpy(result->data, s.begin(), s.size());
            result->data[s.size()] = 0;

            return result;
        }
    };

    template <typename T>
    struct builder_offset
    {
        builder & b;
        const std::size_t offset;

        T & operator*()
        {
            return *operator->();
        }

        const T & operator*() const
        {
            return *operator->();
        }

        T * operator->()
        {
            return reinterpret_cast<T *>(b.data.data() + offset);
        }

        const T * operator->() const
        {
            return reinterpret_cast<const T *>(b.data.data() + offset);
        }
    };
}