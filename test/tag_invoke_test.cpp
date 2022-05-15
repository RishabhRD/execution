/*
 * MIT License
 *
 * Copyright (c) 2022 Rishabh Dwivedi<rishabhdwivedi17@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <doctest/doctest.h>

#include <tag_invoke.hpp>

inline constexpr struct test_cpo {
} cpo;

struct X {
  friend constexpr auto tag_invoke(test_cpo /*unused*/, X /*unused*/) -> void{};
  friend constexpr auto tag_invoke(test_cpo /*unused*/, X /*unused*/,
                                   int a) noexcept -> bool {
    return a > 0;
  };
};

struct Y {};

TEST_CASE("static type checks") {
  static_assert(functional::tag_invocable<functional::tag_t<cpo>, X>);
  static_assert(functional::tag_invocable<functional::tag_t<cpo>, X, int>);
  static_assert(
      functional::nothrow_tag_invocable<functional::tag_t<cpo>, X, int>);
  static_assert(!functional::nothrow_tag_invocable<functional::tag_t<cpo>, X>);
  static_assert(
      std::same_as<functional::tag_invoke_result_t<functional::tag_t<cpo>, X>,
                   void>);
  static_assert(std::same_as<
                functional::tag_invoke_result_t<functional::tag_t<cpo>, X, int>,
                bool>);
  static_assert(!functional::tag_invocable<functional::tag_t<cpo>, Y>);
}

TEST_CASE("value returned by tag_invoke") {
  REQUIRE(functional::tag_invoke(cpo, X{}, 2));
  REQUIRE_FALSE(functional::tag_invoke(cpo, X{}, 0));
}

TEST_CASE("constexpr functions with tag_invoke") {
  static_assert(functional::tag_invoke(cpo, X{}, 2));
  static_assert(!functional::tag_invoke(cpo, X{}, 0));
}
