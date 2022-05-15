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

#include <concepts>
#include <stop_token>
#include <utility>

namespace execution {
namespace detail {
template <template <class> class>
struct check_type_alias_exists;

template <typename B>
concept boolean_testable_impl = std::convertible_to<B, bool>;

template <typename B>
concept boolean_testable = boolean_testable_impl<B> && requires(B&& b) {
  { !std::forward<B>(b) } -> boolean_testable_impl;
};
}  // namespace detail

template <typename T>
concept stoppable_token = std::copy_constructible<T> &&
    std::move_constructible<T> &&
    std::is_nothrow_copy_constructible_v<T> &&
    std::is_nothrow_move_constructible_v<T> &&
    std::equality_comparable<T> &&
    requires(T const& token) {
      { token.stop_requested() } noexcept -> detail::boolean_testable;
      { token.stop_possible() }  noexcept -> detail::boolean_testable;
      typename detail::check_type_alias_exists<T::template callback_type>;
    };

template <typename T, typename CB, typename Initializer>
concept stoppable_token_for = stoppable_token<T> &&
    std::invocable<CB> &&
    requires {
      typename T::template callback_type<CB>;
    } &&
    std::constructible_from<CB, Initializer> &&
    std::constructible_from<typename T::template callback_type<CB>, T, Initializer> &&
    std::constructible_from<typename T::template callback_type<CB>, T&, Initializer> &&
    std::constructible_from<typename T::template callback_type<CB>, const T, Initializer> &&
    std::constructible_from<typename T::template callback_type<CB>, const T&, Initializer>;

template <typename T>
concept unstoppable_token = stoppable_token<T> &&
    requires {
      { T::stop_possible() } -> detail::boolean_testable;
    } &&
    (!T::stop_possible());

}  // namespace execution
