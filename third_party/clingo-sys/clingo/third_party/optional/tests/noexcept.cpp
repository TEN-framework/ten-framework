#include <catch2/catch.hpp>
#include <tl/optional.hpp>

TEST_CASE("Noexcept", "[noexcept]") {
  tl::optional<int> o1{4};
  tl::optional<int> o2{42};

  SECTION("comparison with nullopt") {
    REQUIRE(noexcept(o1 == tl::nullopt));
    REQUIRE(noexcept(tl::nullopt == o1));
    REQUIRE(noexcept(o1 != tl::nullopt));
    REQUIRE(noexcept(tl::nullopt != o1));
    REQUIRE(noexcept(o1 < tl::nullopt));
    REQUIRE(noexcept(tl::nullopt < o1));
    REQUIRE(noexcept(o1 <= tl::nullopt));
    REQUIRE(noexcept(tl::nullopt <= o1));
    REQUIRE(noexcept(o1 > tl::nullopt));
    REQUIRE(noexcept(tl::nullopt > o1));
    REQUIRE(noexcept(o1 >= tl::nullopt));
    REQUIRE(noexcept(tl::nullopt >= o1));
  }

  SECTION("swap") {
      //TODO see why this fails
#if !defined(_MSC_VER) || _MSC_VER > 1900
    REQUIRE(noexcept(swap(o1, o2)) == noexcept(o1.swap(o2)));

    struct nothrow_swappable {
      nothrow_swappable &swap(const nothrow_swappable &) noexcept {
        return *this;
      }
    };

    struct throw_swappable {
      throw_swappable() = default;
      throw_swappable(const throw_swappable &) {}
      throw_swappable(throw_swappable &&) {}
      throw_swappable &swap(const throw_swappable &) { return *this; }
    };

    tl::optional<nothrow_swappable> ont;
    tl::optional<throw_swappable> ot;

    REQUIRE(noexcept(ont.swap(ont)));
    REQUIRE(!noexcept(ot.swap(ot)));
    #endif
  }

  SECTION("constructors") {
      //TODO see why this fails
#if !defined(_MSC_VER) || _MSC_VER > 1900
    REQUIRE(noexcept(tl::optional<int>{}));
    REQUIRE(noexcept(tl::optional<int>{tl::nullopt}));

    struct nothrow_move {
      nothrow_move(nothrow_move &&) noexcept = default;
    };

    struct throw_move {
      throw_move(throw_move &&){};
    };

    using nothrow_opt = tl::optional<nothrow_move>;
    using throw_opt = tl::optional<throw_move>;

    REQUIRE(std::is_nothrow_move_constructible<nothrow_opt>::value);
    REQUIRE(!std::is_nothrow_move_constructible<throw_opt>::value);
#endif
  }

  SECTION("assignment") {
    REQUIRE(noexcept(o1 = tl::nullopt));

    struct nothrow_move_assign {
      nothrow_move_assign() = default;
      nothrow_move_assign(nothrow_move_assign &&) noexcept = default;
      nothrow_move_assign &operator=(const nothrow_move_assign &) = default;
    };

    struct throw_move_assign {
      throw_move_assign() = default;
      throw_move_assign(throw_move_assign &&){};
      throw_move_assign &operator=(const throw_move_assign &) { return *this; }
    };

    using nothrow_opt = tl::optional<nothrow_move_assign>;
    using throw_opt = tl::optional<throw_move_assign>;

    REQUIRE(
        noexcept(std::declval<nothrow_opt>() = std::declval<nothrow_opt>()));
    REQUIRE(!noexcept(std::declval<throw_opt>() = std::declval<throw_opt>()));
  }

  SECTION("observers") {
    REQUIRE(noexcept(static_cast<bool>(o1)));
    REQUIRE(noexcept(o1.has_value()));
  }

  SECTION("modifiers") { REQUIRE(noexcept(o1.reset())); }
}
