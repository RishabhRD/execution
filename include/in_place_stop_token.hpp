/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <spin_wait.hpp>
#include <thread>
#include <type_traits>

namespace execution {

class in_place_stop_source;
class in_place_stop_token;
template <typename F>
class in_place_stop_callback;

class in_place_stop_callback_base {
 public:
  void execute() noexcept { this->execute_(this); }

 protected:
  using execute_fn = void(in_place_stop_callback_base* cb) noexcept;

  explicit in_place_stop_callback_base(in_place_stop_source* source,
                                       execute_fn* execute) noexcept
      : source_(source), execute_(execute) {}

  void register_callback() noexcept;

  friend in_place_stop_source;

  in_place_stop_source* source_;                     // NOLINT
  execute_fn* execute_;                              // NOLINT
  in_place_stop_callback_base* next_ = nullptr;      // NOLINT
  in_place_stop_callback_base** prevPtr_ = nullptr;  // NOLINT
  bool* removedDuringCallback_ = nullptr;            // NOLINT
  std::atomic<bool> callbackCompleted_{false};       // NOLINT
};

class in_place_stop_source {
 public:
  in_place_stop_source() noexcept = default;

  ~in_place_stop_source();

  in_place_stop_source(const in_place_stop_source&) = delete;
  in_place_stop_source(in_place_stop_source&&) = delete;
  auto operator=(in_place_stop_source&&) -> in_place_stop_source& = delete;
  auto operator=(const in_place_stop_source&) -> in_place_stop_source& = delete;

  auto request_stop() noexcept -> bool;

  auto get_token() noexcept -> in_place_stop_token;

  [[nodiscard]] auto stop_requested() const noexcept -> bool {
    return (state_.load(std::memory_order_acquire) & stop_requested_flag) != 0;
  }

 private:
  friend in_place_stop_token;
  friend in_place_stop_callback_base;
  template <typename F>
  friend class in_place_stop_callback;

  auto lock() noexcept -> std::uint8_t;
  void unlock(std::uint8_t oldState) noexcept;

  auto try_lock_unless_stop_requested(bool setStopRequested) noexcept -> bool;

  auto try_add_callback(in_place_stop_callback_base* callback) noexcept -> bool;

  void remove_callback(in_place_stop_callback_base* callback) noexcept;

  static constexpr std::uint8_t stop_requested_flag = 1;
  static constexpr std::uint8_t locked_flag = 2;

  std::atomic<std::uint8_t> state_{0};
  in_place_stop_callback_base* callbacks_ = nullptr;
  std::thread::id notifyingThreadId_;
};

class in_place_stop_token {
 public:
  template <typename F>
  using callback_type = in_place_stop_callback<F>;

  in_place_stop_token() noexcept = default;

  in_place_stop_token(const in_place_stop_token& other) noexcept = default;

  in_place_stop_token(in_place_stop_token&& other) noexcept
      : source_(std::exchange(other.source_, {})) {}

  auto operator=(const in_place_stop_token& other) noexcept
      -> in_place_stop_token& = default;

  auto operator=(in_place_stop_token&& other) noexcept -> in_place_stop_token& {
    source_ = std::exchange(other.source_, nullptr);
    return *this;
  }

  [[nodiscard]] auto stop_requested() const noexcept -> bool {
    return source_ != nullptr && source_->stop_requested();
  }

  [[nodiscard]] auto stop_possible() const noexcept -> bool {
    return source_ != nullptr;
  }

  void swap(in_place_stop_token& other) noexcept {
    std::swap(source_, other.source_);
  }

  friend auto operator==(const in_place_stop_token& a,
                         const in_place_stop_token& b) noexcept -> bool {
    return a.source_ == b.source_;
  }

  friend auto operator!=(const in_place_stop_token& a,
                         const in_place_stop_token& b) noexcept -> bool {
    return !(a == b);
  }

  ~in_place_stop_token() = default;

 private:
  friend in_place_stop_source;
  template <typename F>
  friend class in_place_stop_callback;

  explicit in_place_stop_token(in_place_stop_source* source) noexcept
      : source_(source) {}

  in_place_stop_source* source_{};
};

inline auto in_place_stop_source::get_token() noexcept -> in_place_stop_token {
  return in_place_stop_token{this};
}

template <typename F>
class in_place_stop_callback final : private in_place_stop_callback_base {
 public:
  // Not movable/copyable
  in_place_stop_callback(in_place_stop_callback const&) = delete;
  in_place_stop_callback(in_place_stop_callback&&) = delete;
  auto operator=(in_place_stop_callback&&) -> in_place_stop_callback& = delete;
  auto operator=(in_place_stop_callback const&)
      -> in_place_stop_callback& = delete;

  template <typename T = F>
  requires std::convertible_to<T, F>
  explicit in_place_stop_callback(in_place_stop_token token, T&& func) noexcept(
      std::is_nothrow_constructible_v<F, T>)
      : in_place_stop_callback_base(token.source_,
                                    &in_place_stop_callback::execute_impl),
        func_(std::forward<T>(func)) {
    this->register_callback();
  }

  ~in_place_stop_callback() {
    if (source_ != nullptr) {
      source_->remove_callback(this);
    }
  }

 private:
  static void execute_impl(in_place_stop_callback_base* cb) noexcept {
    auto& self = *static_cast<in_place_stop_callback*>(cb);
    self.func_();
  }

  [[no_unique_address]] F func_;
};

inline void in_place_stop_callback_base::register_callback() noexcept {
  if (source_ != nullptr) {
    if (!source_->try_add_callback(this)) {
      source_ = nullptr;
      // Callback not registered because stop_requested() was true.
      // Execute inline here.
      execute();
    }
  }
}

inline in_place_stop_source::~in_place_stop_source() {
  assert((state_.load(std::memory_order_relaxed) & locked_flag) ==  // NOLINT
         0);
  assert(callbacks_ == nullptr);  // NOLINT
}

inline auto in_place_stop_source::request_stop() noexcept -> bool {
  if (!try_lock_unless_stop_requested(true)) {
    return true;
  }

  notifyingThreadId_ = std::this_thread::get_id();

  // We are responsible for executing callbacks.
  while (callbacks_ != nullptr) {  // NOLINT
    auto* callback = callbacks_;
    callback->prevPtr_ = nullptr;
    callbacks_ = callback->next_;
    if (callbacks_ != nullptr) {
      callbacks_->prevPtr_ = &callbacks_;
    }

    // unlock()
    state_.store(stop_requested_flag, std::memory_order_release);

    bool removedDuringCallback = false;
    callback->removedDuringCallback_ = &removedDuringCallback;

    callback->execute();

    if (!removedDuringCallback) {
      callback->removedDuringCallback_ = nullptr;
      callback->callbackCompleted_.store(true, std::memory_order_release);
    }

    lock();
  }

  // unlock()
  state_.store(stop_requested_flag, std::memory_order_release);

  return false;
}

inline auto in_place_stop_source::lock() noexcept -> std::uint8_t {
  spin_wait spin;
  auto oldState = state_.load(std::memory_order_relaxed);
  do {
    while ((oldState & locked_flag) != 0) {  // NOLINT
      spin.wait();
      oldState = state_.load(std::memory_order_relaxed);
    }
  } while (!state_.compare_exchange_weak(  // NOLINT
      oldState, oldState | locked_flag, std::memory_order_acquire,
      std::memory_order_relaxed));

  return oldState;
}

inline void in_place_stop_source::unlock(std::uint8_t oldState) noexcept {
  (void)state_.store(oldState, std::memory_order_release);
}

inline auto in_place_stop_source::try_lock_unless_stop_requested(
    bool setStopRequested) noexcept -> bool {
  spin_wait spin;
  auto oldState = state_.load(std::memory_order_relaxed);
  do {
    while (true) {
      if ((oldState & stop_requested_flag) != 0) {
        // Stop already requested.
        return false;
      }
      if (oldState == 0) {
        break;
      }
      spin.wait();
      oldState = state_.load(std::memory_order_relaxed);
    }
  } while (!state_.compare_exchange_weak(  // NOLINT
      oldState,
      setStopRequested ? (locked_flag | stop_requested_flag) : locked_flag,
      std::memory_order_acq_rel, std::memory_order_relaxed));

  // Lock acquired successfully
  return true;
}

inline auto in_place_stop_source::try_add_callback(
    in_place_stop_callback_base* callback) noexcept -> bool {
  if (!try_lock_unless_stop_requested(false)) {
    return false;
  }

  callback->next_ = callbacks_;
  callback->prevPtr_ = &callbacks_;
  if (callbacks_ != nullptr) {
    callbacks_->prevPtr_ = &callback->next_;
  }
  callbacks_ = callback;

  unlock(0);

  return true;
}

inline void in_place_stop_source::remove_callback(
    in_place_stop_callback_base* callback) noexcept {
  auto oldState = lock();

  if (callback->prevPtr_ != nullptr) {
    // Callback has not been executed yet.
    // Remove from the list.
    *callback->prevPtr_ = callback->next_;
    if (callback->next_ != nullptr) {
      callback->next_->prevPtr_ = callback->prevPtr_;
    }
    unlock(oldState);
  } else {
    auto notifyingThreadId = notifyingThreadId_;
    unlock(oldState);

    // Callback has either already been executed or is
    // currently executing on another thread.
    if (std::this_thread::get_id() == notifyingThreadId) {
      if (callback->removedDuringCallback_ != nullptr) {
        *callback->removedDuringCallback_ = true;
      }
    } else {
      // Concurrently executing on another thread.
      // Wait until the other thread finishes executing the callback.
      spin_wait spin;
      while (!callback->callbackCompleted_.load(std::memory_order_acquire)) {
        spin.wait();
      }
    }
  }
}

}  // namespace execution
