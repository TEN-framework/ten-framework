#include <catch2/catch.hpp>
#include <tl/optional.hpp>

#include <tuple>
#include <vector>

struct takes_init_and_variadic {
  std::vector<int> v;
  std::tuple<int, int> t;
  template <class... Args>
  takes_init_and_variadic(std::initializer_list<int> l, Args &&... args)
      : v(l), t(std::forward<Args>(args)...) {}
};

TEST_CASE("In place", "[in_place]") {
  tl::optional<int> o1{tl::in_place};
  tl::optional<int> o2(tl::in_place);
  REQUIRE(o1);
  REQUIRE(o1 == 0);
  REQUIRE(o2);
  REQUIRE(o2 == 0);

  tl::optional<int> o3(tl::in_place, 42);
  REQUIRE(o3 == 42);

  tl::optional<std::tuple<int, int>> o4(tl::in_place, 0, 1);
  REQUIRE(o4);
  REQUIRE(std::get<0>(*o4) == 0);
  REQUIRE(std::get<1>(*o4) == 1);

  tl::optional<std::vector<int>> o5(tl::in_place, {0, 1});
  REQUIRE(o5);
  REQUIRE((*o5)[0] == 0);
  REQUIRE((*o5)[1] == 1);

  tl::optional<takes_init_and_variadic> o6(tl::in_place, {0, 1}, 2, 3);
  REQUIRE(o6->v[0] == 0);
  REQUIRE(o6->v[1] == 1);
  REQUIRE(std::get<0>(o6->t) == 2);
  REQUIRE(std::get<1>(o6->t) == 3);
}
