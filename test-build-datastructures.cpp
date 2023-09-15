#include "pid-build-datastructures.h"

#include "catch.hpp"

#include <iostream>

using namespace pid;

template<typename T>
auto build_helper(const T &value) {
    pid::builder builder;
    const auto result{pid::build(builder, value)};
    using ResultType = std::remove_reference<decltype(*result)>::type;

    const auto offset{result.offset};
    std::vector<char> data{std::move(builder.data)};

    const auto &reference{reinterpret_cast<const ResultType *>(data.data() + offset)};

    // TODO: maybe return a vector<char> and a casted reference to its beginning instead?
    // then we could also make builder non-copyable and non-movable to prevent odd bugs
    return std::make_pair(reference, std::move(data));
}

TEST_CASE("build vector of ints") {
    std::vector<std::int32_t> v_input{{1, 1, 2, 3, 5, 8}};
    //std::pair<const vector32<int32_t> &, std::vector<char>> result{build_helper(v_input)};

    const auto &[result, data] = build_helper(v_input);

    const pid::vector32<std::int32_t> &v{*result};

    REQUIRE(v.size() == 6);
    REQUIRE(v[0] == 1);
    REQUIRE(v[1] == 1);
    REQUIRE(v[2] == 2);
    REQUIRE(v[3] == 3);
    REQUIRE(v[4] == 5);
    REQUIRE(v[5] == 8);
}