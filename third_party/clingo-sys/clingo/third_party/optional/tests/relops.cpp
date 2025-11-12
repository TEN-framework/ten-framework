#include <catch2/catch.hpp>
#include <tl/optional.hpp>

TEST_CASE("Relational ops", "[relops]") {
  tl::optional<int> o1{4};
  tl::optional<int> o2{42};
  tl::optional<int> o3{};

  SECTION("self simple") {
    REQUIRE(!(o1 == o2));
    REQUIRE(o1 == o1);
    REQUIRE(o1 != o2);
    REQUIRE(!(o1 != o1));
    REQUIRE(o1 < o2);
    REQUIRE(!(o1 < o1));
    REQUIRE(!(o1 > o2));
    REQUIRE(!(o1 > o1));
    REQUIRE(o1 <= o2);
    REQUIRE(o1 <= o1);
    REQUIRE(!(o1 >= o2));
    REQUIRE(o1 >= o1);
  }

  SECTION("nullopt simple") {
    REQUIRE(!(o1 == tl::nullopt));
    REQUIRE(!(tl::nullopt == o1));
    REQUIRE(o1 != tl::nullopt);
    REQUIRE(tl::nullopt != o1);
    REQUIRE(!(o1 < tl::nullopt));
    REQUIRE(tl::nullopt < o1);
    REQUIRE(o1 > tl::nullopt);
    REQUIRE(!(tl::nullopt > o1));
    REQUIRE(!(o1 <= tl::nullopt));
    REQUIRE(tl::nullopt <= o1);
    REQUIRE(o1 >= tl::nullopt);
    REQUIRE(!(tl::nullopt >= o1));

    REQUIRE(o3 == tl::nullopt);
    REQUIRE(tl::nullopt == o3);
    REQUIRE(!(o3 != tl::nullopt));
    REQUIRE(!(tl::nullopt != o3));
    REQUIRE(!(o3 < tl::nullopt));
    REQUIRE(!(tl::nullopt < o3));
    REQUIRE(!(o3 > tl::nullopt));
    REQUIRE(!(tl::nullopt > o3));
    REQUIRE(o3 <= tl::nullopt);
    REQUIRE(tl::nullopt <= o3);
    REQUIRE(o3 >= tl::nullopt);
    REQUIRE(tl::nullopt >= o3);
  }

  SECTION("with T simple") {
    REQUIRE(!(o1 == 1));
    REQUIRE(!(1 == o1));
    REQUIRE(o1 != 1);
    REQUIRE(1 != o1);
    REQUIRE(!(o1 < 1));
    REQUIRE(1 < o1);
    REQUIRE(o1 > 1);
    REQUIRE(!(1 > o1));
    REQUIRE(!(o1 <= 1));
    REQUIRE(1 <= o1);
    REQUIRE(o1 >= 1);
    REQUIRE(!(1 >= o1));

    REQUIRE(o1 == 4);
    REQUIRE(4 == o1);
    REQUIRE(!(o1 != 4));
    REQUIRE(!(4 != o1));
    REQUIRE(!(o1 < 4));
    REQUIRE(!(4 < o1));
    REQUIRE(!(o1 > 4));
    REQUIRE(!(4 > o1));
    REQUIRE(o1 <= 4);
    REQUIRE(4 <= o1);
    REQUIRE(o1 >= 4);
    REQUIRE(4 >= o1);
  }

  tl::optional<std::string> o4{"hello"};
  tl::optional<std::string> o5{"xyz"};

  SECTION("self complex") {
    REQUIRE(!(o4 == o5));
    REQUIRE(o4 == o4);
    REQUIRE(o4 != o5);
    REQUIRE(!(o4 != o4));
    REQUIRE(o4 < o5);
    REQUIRE(!(o4 < o4));
    REQUIRE(!(o4 > o5));
    REQUIRE(!(o4 > o4));
    REQUIRE(o4 <= o5);
    REQUIRE(o4 <= o4);
    REQUIRE(!(o4 >= o5));
    REQUIRE(o4 >= o4);
  }

  SECTION("nullopt complex") {
    REQUIRE(!(o4 == tl::nullopt));
    REQUIRE(!(tl::nullopt == o4));
    REQUIRE(o4 != tl::nullopt);
    REQUIRE(tl::nullopt != o4);
    REQUIRE(!(o4 < tl::nullopt));
    REQUIRE(tl::nullopt < o4);
    REQUIRE(o4 > tl::nullopt);
    REQUIRE(!(tl::nullopt > o4));
    REQUIRE(!(o4 <= tl::nullopt));
    REQUIRE(tl::nullopt <= o4);
    REQUIRE(o4 >= tl::nullopt);
    REQUIRE(!(tl::nullopt >= o4));

    REQUIRE(o3 == tl::nullopt);
    REQUIRE(tl::nullopt == o3);
    REQUIRE(!(o3 != tl::nullopt));
    REQUIRE(!(tl::nullopt != o3));
    REQUIRE(!(o3 < tl::nullopt));
    REQUIRE(!(tl::nullopt < o3));
    REQUIRE(!(o3 > tl::nullopt));
    REQUIRE(!(tl::nullopt > o3));
    REQUIRE(o3 <= tl::nullopt);
    REQUIRE(tl::nullopt <= o3);
    REQUIRE(o3 >= tl::nullopt);
    REQUIRE(tl::nullopt >= o3);
  }

  SECTION("with T complex") {
    REQUIRE(!(o4 == "a"));
    REQUIRE(!("a" == o4));
    REQUIRE(o4 != "a");
    REQUIRE("a" != o4);
    REQUIRE(!(o4 < "a"));
    REQUIRE("a" < o4);
    REQUIRE(o4 > "a");
    REQUIRE(!("a" > o4));
    REQUIRE(!(o4 <= "a"));
    REQUIRE("a" <= o4);
    REQUIRE(o4 >= "a");
    REQUIRE(!("a" >= o4));

    REQUIRE(o4 == "hello");
    REQUIRE("hello" == o4);
    REQUIRE(!(o4 != "hello"));
    REQUIRE(!("hello" != o4));
    REQUIRE(!(o4 < "hello"));
    REQUIRE(!("hello" < o4));
    REQUIRE(!(o4 > "hello"));
    REQUIRE(!("hello" > o4));
    REQUIRE(o4 <= "hello");
    REQUIRE("hello" <= o4);
    REQUIRE(o4 >= "hello");
    REQUIRE("hello" >= o4);
  }
}
