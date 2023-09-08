#pragma once

#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>

namespace pid {
    template<typename T, typename offset_type=std::int32_t>
    struct offset {
        offset_type offset;

        // TODO: make sure that Pointer points to T or const T with std::enable_if
        template<typename Pointer>
        auto &operator=(Pointer p) {
            offset = reinterpret_cast<const char *>(&*p) - reinterpret_cast<const char *>(this);
            return *this;
        }

        T &operator*() {
            return *operator->();
        }

        const T &operator*() const {
            return *operator->();
        }

        T *operator->() {
            return get<T *>();
        }

        const T *operator->() const {
            return get<const T *>();
        }

        template<typename Pointer>
        Pointer get() const {
            return reinterpret_cast<Pointer>(reinterpret_cast<const char *>(this) + offset);
        }
    };

    template<typename SizeType>
    struct generic_string {
        SizeType string_length;
        char data[];

        SizeType size() const {
            return string_length;
        }

        [[nodiscard]] const char *begin() const {
            return data;
        }

        [[nodiscard]] const char *end() const {
            return data + string_length;
        }

        operator std::string_view() const {
            return {begin(), end()};
        }

        template<typename String>
        bool operator==(const String &other) const {
            return std::string_view{*this} == other;
        }
    };

    using string32 = generic_string<std::uint32_t>;

    template<typename T, typename SizeType>
    struct generic_vector {
        SizeType vector_length;
        T data[];

        SizeType size() const {
            return vector_length;
        }

        [[nodiscard]] const T *begin() const {
            return data;
        }

        [[nodiscard]] const T *end() const {
            return data + vector_length;
        }

        T &operator[](SizeType index) {
            return data[index];
        }

        const T &operator[](SizeType index) const {
            return data[index];
        }
    };

    template<typename T>
    using vector32 = generic_vector<T, std::uint32_t>;

    template<typename T>
    struct builder_offset;

    struct builder {
        std::vector<char> data;

        template<typename T>
        builder_offset<T> add() {
            // TODO: alignment!
            const auto offset{data.size()};
            data.resize(data.size() + sizeof(T));
            return {*this, offset};
        }

        template<typename SizeType=std::uint32_t>
        builder_offset<generic_string<SizeType>> add_string(std::string_view s) {
            // TODO: alignment!
            const auto total_size = (sizeof(SizeType) + s.size() + 1 + 3) / 4 * 4;
            const auto offset{data.size()};
            data.resize(data.size() + total_size);
            builder_offset<generic_string<SizeType>> result{*this, offset};
            result->string_length = s.size();
            std::memcpy(result->data, s.begin(), s.size());
            result->data[s.size()] = 0;

            return result;
        }

        template<typename T, typename SizeType=std::uint32_t>
        builder_offset<generic_vector<T, SizeType>> add_vector(SizeType size) {
            // TODO: alignment!
            const auto total_size = sizeof(SizeType) * size;
            const auto offset{data.size()};
            data.resize(data.size() + total_size);

            builder_offset<generic_vector<T, SizeType>> result{*this, offset};
            result->vector_length = size;

            return result;
        }
    };

    template<typename T>
    struct builder_offset {
        builder &b;
        const std::size_t offset;

        T &operator*() {
            return *operator->();
        }

        const T &operator*() const {
            return *operator->();
        }

        T *operator->() {
            return reinterpret_cast<T *>(b.data.data() + offset);
        }

        const T *operator->() const {
            return reinterpret_cast<const T *>(b.data.data() + offset);
        }
    };
}