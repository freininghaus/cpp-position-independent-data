#include "pid.h"

#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;

    pid::builder b;
    pid::builder_offset<std::int32_t> o{b, 12};

    *o = 4;
}
