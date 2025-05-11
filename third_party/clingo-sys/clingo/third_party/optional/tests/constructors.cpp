#include <catch2/catch.hpp>
#include <tl/optional.hpp>
#include <vector>

struct foo {
  foo() = default;
  foo(foo&) = delete;
  foo(foo&&) {};
};

TEST_CASE("Constructors", "[constructors]") {
  tl::optional<int> o1;
  REQUIRE(!o1);

  tl::optional<int> o2 = tl::nullopt;
  REQUIRE(!o2);

  tl::optional<int> o3 = 42;
  REQUIRE(*o3 == 42);

  tl::optional<int> o4 = o3;
  REQUIRE(*o4 == 42);

  tl::optional<int> o5 = o1;
  REQUIRE(!o5);

  tl::optional<int> o6 = std::move(o3);
  REQUIRE(*o6 == 42);

  tl::optional<short> o7 = 42;
  REQUIRE(*o7 == 42);

  tl::optional<int> o8 = o7;
  REQUIRE(*o8 == 42);

  tl::optional<int> o9 = std::move(o7);
  REQUIRE(*o9 == 42);

  {
    tl::optional<int &> o;
    REQUIRE(!o);

    tl::optional<int &> oo = o;
    REQUIRE(!oo);
  }

  {
    auto i = 42;
    tl::optional<int &> o = i;
    REQUIRE(o);
    REQUIRE(*o == 42);

    tl::optional<int &> oo = o;
    REQUIRE(oo);
    REQUIRE(*oo == 42);
  }

  std::vector<foo> v;
  v.emplace_back();
  tl::optional<std::vector<foo>> ov = std::move(v);
  REQUIRE(ov->size() == 1);
}
