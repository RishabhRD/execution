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

#include <utility>

namespace functional {
namespace _tag_invoke {
void tag_invoke();

template <class Tag, class... Args>
concept tag_invocable = requires(Tag&& tag, Args&&... args) {
  tag_invoke(std::forward<Tag>(tag), std::forward<Args>(args)...);
};

template <class Tag, class... Args>
concept nothrow_tag_invocable = tag_invocable<Tag, Args...> &&
    requires(Tag&& tag, Args&&... args) {
  { tag_invoke(std::forward<Tag>(tag), std::forward<Args>(args)...) }
  noexcept;
};

template <class Tag, class... Args>
using tag_invoke_result_t =
    decltype(tag_invoke(std::declval<Tag>(), std::declval<Args>()...));

template <class Tag, class... Args>
struct tag_invoke_result {};

template <class Tag, class... Args>
requires tag_invocable<Tag, Args...>
struct tag_invoke_result<Tag, Args...> {
  using type = tag_invoke_result_t<Tag, Args...>;
};

struct tag {
  template <class Tag, class... Args>
  requires tag_invocable<Tag, Args...>
  constexpr auto operator()(Tag&& tag, Args&&... args) const
      noexcept(nothrow_tag_invocable<Tag, Args...>)
          -> tag_invoke_result_t<Tag, Args...> {
    return tag_invoke(std::forward<Tag>(tag), std::forward<Args>(args)...);
  }
};

}  // namespace _tag_invoke
template <auto& Tag>
using tag_t = std::decay_t<decltype(Tag)>;

inline constexpr _tag_invoke::tag tag_invoke{};
using _tag_invoke::nothrow_tag_invocable;
using _tag_invoke::tag_invocable;
using _tag_invoke::tag_invoke_result;
using _tag_invoke::tag_invoke_result_t;
}  // namespace functional
