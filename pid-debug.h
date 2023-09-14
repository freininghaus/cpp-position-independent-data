#include <iostream>
#include <iomanip>
#include <vector>

namespace pid {

    void dump(const std::vector<char> & data)
    {
        const auto ints{reinterpret_cast<const std::int32_t *>(data.data())};

        for (std::size_t index{0}; index < (data.size() + 3) / 4; ++index) {
            std::cout << std::setw(3) << std::setfill(' ') << 4 * index << ".." << std::setw(3)
                      << std::setfill('.') << std::min(4 * index + 3, data.size() - 1) << ": ";

            for (std::size_t i{0}; i < 4; ++i) {
                const auto byteIndex{4 * index + 1};

                if (byteIndex >= data.size()) {
                    std::cout << " ";
                }
                else {
                    const auto c{data[4 * index + i]};
                    if (c >= 32) {
                        std::cout << c;
                    }
                    else {
                        std::cout << '.';
                    }
                }
            }

            std::cout << "  (";
            for (std::size_t i{0}; i < 4 && 4 * index + i < data.size(); ++i) {
                std::cout << " " << std::setw(3) << std::setfill(' ')
                          << std::int32_t{data[4 * index + i]};
            }
            std::cout << " )";

            if (4 * index + 4 <= data.size()) {
                std::cout << " (" << std::setw(3) << ints[index] << " -> ["
                          << 4 * index + ints[index] << "])";
            }

            std::cout << std::endl;
        }
    }
}