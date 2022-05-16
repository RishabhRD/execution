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

#pragma once
#include <concepts.hpp>
#include <concepts>
#include <tag_invoke.hpp>
#include <type_traits>
#include <utility>

namespace execution {
struct no_env {
  friend void tag_invoke(auto, std::same_as<no_env> auto, auto&&...) = delete;
};

namespace _get_env {

struct get_env_t {
  template <class EnvProvider>
  requires functional::tag_invocable<get_env_t, EnvProvider const&>
  auto operator()(EnvProvider const& provider) const
      noexcept(functional::nothrow_tag_invocable<get_env_t, EnvProvider const&>)
          -> functional::tag_invoke_result_t<get_env_t, EnvProvider const&> {
    using Env = functional::tag_invoke_result_t<get_env_t, EnvProvider const&>;
    static_assert(!std::same_as<std::decay_t<Env>, no_env>);
    return tag_invoke(*this, provider);
  }
};

}  // namespace _get_env

inline constexpr _get_env::get_env_t get_env{};

namespace _forwarding_env_query {
struct forwarding_env_query_t {
  template <typename Query>
  requires functional::tag_invocable<forwarding_env_query_t, Query const&>
  auto operator()(Query const& query) const noexcept(
      functional::nothrow_tag_invocable<forwarding_env_query_t, Query const&>)
      -> functional::tag_invoke_result_t<forwarding_env_query_t, Query const&> {
    using result_t =
        functional::tag_invoke_result_t<forwarding_env_query_t, Query const&>;
    static_assert(std::is_nothrow_convertible_v<result_t, bool>);
    // TODO: static_assert(if query is core constant expression then result_t{}
    // should be also core constant expression)
    return functional::tag_invoke(*this, query);
  }
};
}  // namespace _forwarding_env_query

inline constexpr _forwarding_env_query::forwarding_env_query_t
    forwarding_env_query{};

template <typename EnvProvier>
concept environment_provier = requires(EnvProvier& ep) {
  { get_env(std::as_const(ep)) } -> tf::none_of<void, no_env>;
};

}  // namespace execution
