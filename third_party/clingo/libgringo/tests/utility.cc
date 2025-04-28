// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#include "gringo/utility.hh"
#include "tests/tests.hh"

namespace Gringo { namespace Test {

using Gringo::IO::to_string;

namespace {

struct CloneMe {
    CloneMe(int x) : x(x) { }
    CloneMe(CloneMe const &) = delete;
    CloneMe(CloneMe &&) = default;
    CloneMe *clone() const { return new CloneMe(x); }
    int x;
};

std::ostream &operator<<(std::ostream &out, CloneMe const &x) {
    out << "c" << x.x;
    return out;
}

} // namespace

TEST_CASE("utility", "[base]") {
    SECTION("to_string") {
        int x1 { 1 };
        std::unique_ptr<int> x2 { new int(1) };
        std::pair<int, int> x3 { 1, 2 };
        std::tuple<> x4 { };
        std::tuple<int> x5 { 1 };
        std::tuple<int,int,int> x6 { 1, 2, 3 };
        std::vector<int> x7 { 1, 2, 3 };
        std::vector<std::vector<int>> x8 { {1,2}, {3,4}, {5,6,7}, {8} };

        REQUIRE("1" == to_string(x1));
        REQUIRE("1" == to_string(x2));
        REQUIRE("(1,2)" == to_string(x3));
        REQUIRE("()" == to_string(x4));
        REQUIRE("(1)" == to_string(x5));
        REQUIRE("(1,2,3)" == to_string(x6));
        REQUIRE("[1,2,3]" == to_string(x7));
        REQUIRE("[[1,2],[3,4],[5,6,7],[8]]" == to_string(x8));
    }

    SECTION("cross_product") {
        std::vector<std::vector<int>> x1 { };
        std::vector<std::vector<int>> x2 { {} };
        std::vector<std::vector<int>> x3 { {1} };
        std::vector<std::vector<int>> x4 { {1,2} };
        std::vector<std::vector<int>> x5 { {1,2}, {3} };
        std::vector<std::vector<int>> x6 { {1,2}, {3,4} };
        std::vector<std::vector<int>> x7 { {1,2}, {3,4}, {} };
        std::vector<std::vector<int>> x8 { {0,1}, {0,1}, {0,1} };
        std::vector<std::vector<int>> x9 { {1,2}, {3,4}, {5,6,7}, {8} };
        cross_product(x1);
        cross_product(x2);
        cross_product(x3);
        cross_product(x4);
        cross_product(x5);
        cross_product(x6);
        cross_product(x7);
        cross_product(x8);
        cross_product(x9);
        REQUIRE("[[]]" == to_string(x1));
        REQUIRE("[]" == to_string(x2));
        REQUIRE("[[1]]" == to_string(x3));
        REQUIRE("[[1],[2]]" == to_string(x4));
        REQUIRE("[[1,3],[2,3]]" == to_string(x5));
        REQUIRE("[[1,3],[2,3],[1,4],[2,4]]" == to_string(x6));
        REQUIRE("[]" == to_string(x7));
        REQUIRE("[[0,0,0],[1,0,0],[0,1,0],[1,1,0],[0,0,1],[1,0,1],[0,1,1],[1,1,1]]" == to_string(x8));
        REQUIRE("[[1,3,5,8],[2,3,5,8],[1,4,5,8],[2,4,5,8],[1,3,6,8],[2,3,6,8],[1,4,6,8],[2,4,6,8],[1,3,7,8],[2,3,7,8],[1,4,7,8],[2,4,7,8]]" == to_string(x9));
    }

    SECTION("clone") {
        typedef std::unique_ptr<CloneMe> U;
        CloneMe x1 { 1 };
        auto x2(gringo_make_unique<CloneMe>(1));
        auto x3(std::make_pair(gringo_make_unique<CloneMe>(1), gringo_make_unique<CloneMe>(2)));
        std::vector<U> x4;
        x4.emplace_back(gringo_make_unique<CloneMe>(1));
        x4.emplace_back(gringo_make_unique<CloneMe>(2));
        x4.emplace_back(gringo_make_unique<CloneMe>(3));
        std::tuple<U, U, U> x5 { gringo_make_unique<CloneMe>(1), gringo_make_unique<CloneMe>(2), gringo_make_unique<CloneMe>(3) };
        std::tuple<> x6 { };
        REQUIRE(to_string(1) == to_string(get_clone(1)));
        REQUIRE(to_string(x1) == to_string(U(get_clone(&x1))));
        REQUIRE(to_string(x2) == to_string(get_clone(x2)));
        REQUIRE(to_string(x3) == to_string(get_clone(x3)));
        REQUIRE(to_string(x4) == to_string(get_clone(x4)));
        REQUIRE(to_string(x5) == to_string(get_clone(x5)));
        REQUIRE(to_string(x6) == to_string(get_clone(x6)));
    }

    SECTION("hash") {
        std::vector<int> x1 { };
        std::vector<int> x1e { };
        std::vector<int> x2 { 1, 2, 3 };
        std::vector<int> x2e { 1, 2, 3 };
        std::vector<int> x3 { 1, 2, 3, 4 };
        std::vector<int> x3e { 1, 2, 3, 4 };
        std::pair<int, int> x4 { 1, 2 };
        std::pair<int, int> x4e { 1, 2 };
        std::pair<int, int> x5 { 2, 3 };
        std::pair<int, int> x5e { 2, 3 };
        std::tuple<> x6 { };
        std::tuple<> x6e { };
        std::tuple<int, int, int> x7 { 1, 2, 4 };
        std::tuple<int, int, int> x7e { 1, 2, 4 };
        std::tuple<int, int, int> x8 { 1, 2, 3 };
        std::tuple<int, int, int> x8e { 1, 2, 3 };
        std::unique_ptr<int> x9 { new int(1) };
        std::unique_ptr<int> x9e { new int(1) };
        std::unique_ptr<int> x10 { new int(2) };
        std::unique_ptr<int> x10e { new int(2) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x11 { std::make_pair(std::vector<int> {1, 2, 3}, gringo_make_unique<int>(4)) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x11e { std::make_pair(std::vector<int> {1, 2, 3}, gringo_make_unique<int>(4)) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x12 { std::make_pair(std::vector<int> {1, 2, 4}, gringo_make_unique<int>(4)) };


        REQUIRE(get_value_hash(1) == get_value_hash(1));
        REQUIRE(get_value_hash(x1) == get_value_hash(x1e));
        REQUIRE(get_value_hash(x2) == get_value_hash(x2e));
        REQUIRE(get_value_hash(x3) == get_value_hash(x3e));
        REQUIRE(get_value_hash(x4) == get_value_hash(x4e));
        REQUIRE(get_value_hash(x5) == get_value_hash(x5e));
        REQUIRE(get_value_hash(x6) == get_value_hash(x6e));
        REQUIRE(get_value_hash(x7) == get_value_hash(x7e));
        REQUIRE(get_value_hash(x8) == get_value_hash(x8e));
        REQUIRE(get_value_hash(x9) == get_value_hash(x9e));
        REQUIRE(get_value_hash(x10) == get_value_hash(x10e));
        REQUIRE(get_value_hash(x11) == get_value_hash(x11e));

        REQUIRE(get_value_hash(x1) != get_value_hash(x2));
        REQUIRE(get_value_hash(x1) != get_value_hash(x3));
        REQUIRE(get_value_hash(x2) != get_value_hash(x3));
        REQUIRE(get_value_hash(x4) != get_value_hash(x5));
        REQUIRE(get_value_hash(x6) != get_value_hash(x7));
        REQUIRE(get_value_hash(x6) != get_value_hash(x8));
        REQUIRE(get_value_hash(x7) != get_value_hash(x8));
        REQUIRE(get_value_hash(x9) != get_value_hash(x10));
        REQUIRE(get_value_hash(x11) != get_value_hash(x12));
    }

    SECTION("equal_to") {
        std::vector<int> x1 { };
        std::vector<int> x1e { };
        std::vector<int> x2 { 1, 2, 3 };
        std::vector<int> x2e { 1, 2, 3 };
        std::vector<int> x3 { 1, 2, 3, 4 };
        std::vector<int> x3e { 1, 2, 3, 4 };
        std::pair<int, int> x4 { 1, 2 };
        std::pair<int, int> x4e { 1, 2 };
        std::pair<int, int> x5 { 2, 3 };
        std::pair<int, int> x5e { 2, 3 };
        std::tuple<> x6 { };
        std::tuple<> x6e { };
        std::tuple<int, int, int> x7 { 1, 2, 4 };
        std::tuple<int, int, int> x7e { 1, 2, 4 };
        std::tuple<int, int, int> x8 { 1, 2, 3 };
        std::tuple<int, int, int> x8e { 1, 2, 3 };
        std::unique_ptr<int> x9 { new int(1) };
        std::unique_ptr<int> x9e { new int(1) };
        std::unique_ptr<int> x10 { new int(2) };
        std::unique_ptr<int> x10e { new int(2) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x11 { std::make_pair(std::vector<int> {1, 2, 3}, gringo_make_unique<int>(4)) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x11e { std::make_pair(std::vector<int> {1, 2, 3}, gringo_make_unique<int>(4)) };
        std::tuple<std::pair<std::vector<int>, std::unique_ptr<int>>> x12 { std::make_pair(std::vector<int> {1, 2, 4}, gringo_make_unique<int>(4)) };

        REQUIRE(is_value_equal_to(x1, x1e));
        REQUIRE(is_value_equal_to(x2, x2e));
        REQUIRE(is_value_equal_to(x3, x3e));
        REQUIRE(is_value_equal_to(x4, x4e));
        REQUIRE(is_value_equal_to(x5, x5e));
        REQUIRE(is_value_equal_to(x6, x6e));
        REQUIRE(is_value_equal_to(x7, x7e));
        REQUIRE(is_value_equal_to(x8, x8e));
        REQUIRE(is_value_equal_to(x9, x9e));
        REQUIRE(is_value_equal_to(x10, x10e));
        REQUIRE(is_value_equal_to(x11, x11e));

        REQUIRE(!is_value_equal_to(x1, x2));
        REQUIRE(!is_value_equal_to(x1, x3));
        REQUIRE(!is_value_equal_to(x2, x3));
        REQUIRE(!is_value_equal_to(x4, x5));
        REQUIRE(!is_value_equal_to(x7, x8));
        REQUIRE(!is_value_equal_to(x9, x10));
        REQUIRE(!is_value_equal_to(x11, x12));
    }
}

} } // namespace Test Gringo

