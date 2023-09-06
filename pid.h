#pragma once

#include <vector>

namespace pid {
    struct builder {
        std::vector<char> data;
    };

    template <typename T>
    struct builder_offset
    {
        builder& b;
        std::size_t offset;

        T& operator*() {
            return *operator->();
        }

        const T& operator*() const{
            return *operator->();
        }

        T* operator->() {
            return reinterpret_cast<T*>(b.data.data() + offset);
        }

        const T* operator->() const {
            return reinterpret_cast<const T*>(b.data.data() + offset);
        }
    };
}