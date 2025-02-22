#include <pid/pid-build-datastructures.h>

#include "catch.hpp"
#include "pid-debug.h"

#include <iostream>

using namespace pid;

template <typename T>
struct is_vector : std::false_type
{
};

template <typename T>
struct is_vector<std::vector<T>> : std::true_type
{
};

template <typename T>
struct is_map : std::false_type
{
};

template <typename Key, typename Value>
struct is_map<std::map<Key, Value>> : std::true_type
{
};

template <typename T>
auto build_helper(const T & value)
{
    pid::builder builder;
    pid::datastructure_builder d_builder{builder};
    const auto result{[&]() {
        if constexpr (is_vector<T>::value) {
            // Add a top-level vector to simplify testing of vector functions
            const auto items{d_builder(value)};
            auto result{builder.add<vector32<typename pid_type<typename T::value_type>::type>>()};
            *result = items;
            return result;
        } else if constexpr (is_map<T>::value) {
            // Add a top-level map to simplify testing of map functions
            const auto items{d_builder(value)};
            auto result{builder.add<map32<
                typename pid_type<typename T::key_type>::type,
                typename pid_type<typename T::mapped_type>::type>>()};
            *result = items;
            return result;
        } else {
            return d_builder(value);
        }
    }()};

    using ResultType = std::remove_reference<decltype(*result)>::type;

    const auto offset{result.offset};
    std::vector<char> data{std::move(builder.data)};

    const auto & reference{reinterpret_cast<const ResultType *>(data.data() + offset)};

    // TODO: maybe return a vector<char> and a casted reference to its beginning instead?
    // then we could also make builder non-copyable and non-movable to prevent odd bugs
    return std::make_pair(reference, std::move(data));
}

TEST_CASE("build vector of ints")
{
    std::vector<std::int32_t> v_input{{1, 1, 2, 3, 5, 8}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<std::int32_t> & v{*result};

    REQUIRE(v.size() == 6);
    CHECK(v[0] == 1);
    CHECK(v[1] == 1);
    CHECK(v[2] == 2);
    CHECK(v[3] == 3);
    CHECK(v[4] == 5);
    CHECK(v[5] == 8);
}

TEST_CASE("build vector of strings")
{
    std::vector<std::string> v_input{{"a", "b", "c"}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<pid::string32> & v = *result;

    REQUIRE(v.size() == 3);
    CHECK(v[0] == "a");
    CHECK(v[1] == "b");
    CHECK(v[2] == "c");
}

TEST_CASE("build map (int -> int)")
{
    std::map<std::int32_t, std::int32_t> m_input{{42, 1}, {-1, 2}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, std::int32_t> & m{*result};

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(m.at(42) == 1);
    CHECK(m.at(-1) == 2);
}

TEST_CASE("build map (int -> str)")
{
    std::map<std::int32_t, std::string> m_input{{42, "a"}, {-1, "b"}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, pid::string32> & m = *result;

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(m.at(42) == "a");
    CHECK(m.at(-1) == "b");
}

TEST_CASE("build map (str -> int)")
{
    std::map<std::string, std::int32_t> m_input{{"one", 1}, {"two", 2}, {"three", 3}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<pid::string32, std::int32_t> & m = *result;

    REQUIRE(m.size() == 3);
    CHECK_THROWS_AS(m.at("four"), std::out_of_range);
    CHECK(m.at("one") == 1);
    CHECK(m.at("two") == 2);
    CHECK(m.at("three") == 3);
}

TEST_CASE("build map (int -> [str])")
{
    std::map<std::int32_t, std::vector<std::string>> m_input{
        {1, {}},       {2, {"two"}},         {3, {"three"}}, {4, {"two", "two"}},
        {5, {"five"}}, {6, {"two", "three"}}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, pid::vector32<pid::string32>> & m = *result;

    REQUIRE(m.size() == 6);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    REQUIRE(m.at(1).size() == 0);
    REQUIRE(m.at(2).size() == 1);
    CHECK(m.at(2)[0] == "two");
    REQUIRE(m.at(3).size() == 1);
    CHECK(m.at(3)[0] == "three");
    REQUIRE(m.at(4).size() == 2);
    CHECK(m.at(4)[0] == "two");
    CHECK(m.at(4)[1] == "two");
    REQUIRE(m.at(5).size() == 1);
    CHECK(m.at(5)[0] == "five");
    REQUIRE(m.at(6).size() == 2);
    CHECK(m.at(6)[0] == "two");
    CHECK(m.at(6)[1] == "three");
}

TEST_CASE("build map (str -> (str -> [int]))")
{
    std::map<std::string, std::map<std::string, std::vector<std::int32_t>>> m_input{{
        {"a", {}},
        {"b", {{"b1", {}}}},
        {"c", {{"c1", {42}}, {"c2", {7, 8, 9}}}},
    }};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<pid::string32, pid::map32<pid::string32, pid::vector32<std::int32_t>>> & m =
        *result;

    REQUIRE(m.size() == 3);
    CHECK_THROWS_AS(m.at("foo"), std::out_of_range);
    REQUIRE(m.at("a").size() == 0);
    REQUIRE(m.at("b").size() == 1);
    REQUIRE(m.at("b").at("b1").size() == 0);
    REQUIRE(m.at("c").size() == 2);
    REQUIRE(m.at("c").at("c1").size() == 1);
    CHECK(m.at("c").at("c1")[0] == 42);
    REQUIRE(m.at("c").at("c2").size() == 3);
    CHECK(m.at("c").at("c2")[0] == 7);
    CHECK(m.at("c").at("c2")[1] == 8);
    CHECK(m.at("c").at("c2")[2] == 9);
}

TEST_CASE("build vector of optional ints")
{
    std::vector<std::optional<std::int32_t>> v_input{{1, std::nullopt, 2, 3, 5, 8}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<std::optional<std::int32_t>> & v = *result;

    REQUIRE(v.size() == 6);
    CHECK(v[0]);
    CHECK(*v[0] == 1);
    CHECK(not v[1]);
    CHECK(v[2]);
    CHECK(v[2] == 2);
    CHECK(v[3]);
    CHECK(v[3] == 3);
    CHECK(v[4]);
    CHECK(v[4] == 5);
    CHECK(v[5]);
    CHECK(v[5] == 8);
}

TEST_CASE("build vector of optional strings")
{
    std::vector<std::optional<std::string>> v_input{{"a", std::nullopt, "b", "c"}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<pid::ptr<pid::string32, std::int32_t>> & v = *result;

    REQUIRE(v.size() == 4);
    CHECK(*v[0] == "a");
    CHECK(not v[1]);
    CHECK(*v[2] == "b");
    CHECK(*v[3] == "c");
}

TEST_CASE("test caching 1")
{
    std::map<std::string, std::vector<std::optional<std::string>>> m_input{
        {{"a", {"x", std::nullopt, "z"}},
         {"b", {"a", "b", "c"}},
         {"c", {"x", std::nullopt, "z"}}}};

    const auto & [result, data] = build_helper(m_input);

    const pid::map32<pid::string32, pid::vector32<pid::ptr32<pid::string32>>> & m = *result;

    REQUIRE(m.size() == 3);

    const auto itA{m.find("a")};
    const auto itB{m.find("b")};
    const auto itC{m.find("c")};

    REQUIRE(itA != m.end());
    REQUIRE(itB != m.end());
    REQUIRE(itC != m.end());

    REQUIRE(itA->second.size() == 3);
    REQUIRE(itB->second.size() == 3);
    REQUIRE(itB->second.size() == 3);

    // Verify deduplication of strings
    CHECK(&*itA->first.begin() == &*itB->second[0]->begin());       // "a"
    CHECK(&*itB->first.begin() == &*itB->second[1]->begin());       // "b"
    CHECK(&*itA->second[0]->begin() == &*itC->second[0]->begin());  // "x"
    CHECK(&*itA->second[2]->begin() == &*itC->second[2]->begin());  // "z

    // Verify deduplication of vectors
    CHECK(&*itA->second.begin() == &*itC->second.begin());
}

// TODO: deduplication of maps
