// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_UTILS_H
#define INTRINSICCV_UTILS_H

#include <algorithm>
#include <limits>
#include <type_traits>

#include "config.h"

namespace intrinsiccv {

// Saturating cast from signed to unsigned type.
template <typename S, typename U,
          std::enable_if_t<std::numeric_limits<S>::is_signed, bool> = true,
          std::enable_if_t<not std::numeric_limits<U>::is_signed, bool> = true>
static U saturating_cast(S value) INTRINSICCV_STREAMING_COMPATIBLE {
  if (value > std::numeric_limits<U>::max()) {
    return std::numeric_limits<U>::max();
  } else if (value < 0) {
    return 0;
  }

  return static_cast<U>(value);
}

// Saturating cast from unsigned to unsigned type.
template <
    typename SrcType, typename DstType,
    std::enable_if_t<std::is_unsigned_v<DstType> && std::is_unsigned_v<SrcType>,
                     bool> = true>
static DstType saturating_cast(SrcType value) INTRINSICCV_STREAMING_COMPATIBLE {
  return static_cast<DstType>(value);
}

// Saturating cast from unsigned to signed type.
template <
    typename SrcType, typename DstType,
    std::enable_if_t<std::is_signed_v<DstType> && std::is_unsigned_v<SrcType>,
                     bool> = true>
static DstType saturating_cast(SrcType value) INTRINSICCV_STREAMING_COMPATIBLE {
  DstType max_value = std::numeric_limits<DstType>::max();

  if (value > static_cast<SrcType>(max_value)) {
    return max_value;
  }

  return static_cast<DstType>(value);
}

// Rounding shift right.
template <typename T>
static T rounding_shift_right(T value,
                              size_t shift) INTRINSICCV_STREAMING_COMPATIBLE {
  return (value + (1UL << (shift - 1))) >> shift;
}

// Swap two variables, since non-Android toolchains do not allow using std::swap
// for SVE vectors.
template <typename T>
static inline void swap(T &a, T &b) INTRINSICCV_STREAMING_COMPATIBLE {
  T tmp = a;
  a = b;
  b = tmp;
}

// When placed in a loop, it effectively disables loop vectorization.
static inline void disable_loop_vectorization()
    INTRINSICCV_STREAMING_COMPATIBLE {
  __asm__("");
}

// Helper class to unroll a loop as needed.
class LoopUnroll final {
 public:
  explicit LoopUnroll(size_t length,
                      size_t step) INTRINSICCV_STREAMING_COMPATIBLE
      : length_(length),
        step_(step),
        can_avoid_tail_(length >= step) {}

  // Loop unrolled four times.
  template <typename CallbackType>
  LoopUnroll &unroll_four_times(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<4>(callback);
  }

  // Loop unrolled twice.
  template <typename CallbackType>
  LoopUnroll &unroll_twice(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<2>(callback);
  }

  // Unrolls the loop twice, if enabled.
  template <bool Enable, typename CallbackType>
  LoopUnroll &unroll_twice_if(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if constexpr (Enable) {
      return unroll_twice(callback);
    }

    return *this;
  }

  // Loop unrolled once.
  template <typename CallbackType>
  LoopUnroll &unroll_once(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<1>(callback);
  }

  // Unrolls the loop once, if enabled.
  template <bool Enable, typename CallbackType>
  LoopUnroll &unroll_once_if(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if constexpr (Enable) {
      return unroll_once(callback);
    }

    return *this;
  }

  // Processes trailing data.
  template <typename CallbackType>
  LoopUnroll &tail(CallbackType callback) INTRINSICCV_STREAMING_COMPATIBLE {
    for (index_ = 0; index_ < remaining_length(); ++index_) {
      disable_loop_vectorization();
      callback(index_);
    }

    length_ = 0;
    return *this;
  }

  // Processes all remaining data at once.
  template <typename CallbackType>
  LoopUnroll &remaining(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (length_) {
      callback(length_, step_);
      length_ = 0;
    }

    return *this;
  }

  // Returns true if there is nothing left to process.
  bool empty() const INTRINSICCV_STREAMING_COMPATIBLE { return length_ == 0; }

  // Returns the step value.
  size_t step() const INTRINSICCV_STREAMING_COMPATIBLE { return step_; }

  // Returns the remaining length.
  size_t remaining_length() const INTRINSICCV_STREAMING_COMPATIBLE {
    return length_;
  }

  // Returns true if it is possible to avoid the tail loop.
  bool can_avoid_tail() const INTRINSICCV_STREAMING_COMPATIBLE {
    return can_avoid_tail_;
  }

  // Instructs the loop logic to prepare to avoid the tail loop.
  void avoid_tail() INTRINSICCV_STREAMING_COMPATIBLE { length_ = step(); }

  template <const size_t UnrollFactor, typename CallbackType>
  LoopUnroll &unroll_n_times(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    const size_t step = UnrollFactor * step_;
    const size_t max_index = remaining_length() / step;

    for (index_ = 0; index_ < max_index; ++index_) {
      callback(step);
    }

    // Adjust length to reflect the processed data.
    length_ -= step * index_;
    return *this;
  }

  // Instructs the loop logic to avoid the tail loop.
  template <typename CallbackType>
  bool try_avoid_tail_loop(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (INTRINSICCV_UNLIKELY(!can_avoid_tail_)) {
      return false;
    }

    if (INTRINSICCV_UNLIKELY(!remaining_length())) {
      return false;
    }

    if (INTRINSICCV_LIKELY(callback(step() - remaining_length()))) {
      length_ = step();
      return true;
    }

    return false;
  }

 private:
  size_t length_;
  size_t step_;
  size_t index_;
  bool can_avoid_tail_;
};  // end of class LoopUnroll

// This is the same as LoopUnroll, except that it passes indices to callbacks.
class LoopUnroll2 final {
 public:
  explicit LoopUnroll2(size_t length,
                       size_t step) INTRINSICCV_STREAMING_COMPATIBLE
      : length_(length),
        step_(step),
        index_(0),
        try_avoid_tail_loop_(false) {}

  explicit LoopUnroll2(size_t start_index, size_t length,
                       size_t step) INTRINSICCV_STREAMING_COMPATIBLE
      : length_(length),
        step_(step),
        index_(std::min(start_index, length)),
        try_avoid_tail_loop_(false) {}

  // Loop unrolled four times.
  template <typename CallbackType>
  LoopUnroll2 &unroll_four_times(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<4>(callback);
  }

  // Loop unrolled twice.
  template <typename CallbackType>
  LoopUnroll2 &unroll_twice(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<2>(callback);
  }

  // Unrolls the loop twice, if enabled.
  template <bool Enable, typename CallbackType>
  LoopUnroll2 &unroll_twice_if(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if constexpr (Enable) {
      return unroll_twice(callback);
    }

    return *this;
  }

  // Loop unrolled once.
  template <typename CallbackType>
  LoopUnroll2 &unroll_once(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    return unroll_n_times<1>(callback);
  }

  // Unrolls the loop once, if enabled.
  template <bool Enable, typename CallbackType>
  LoopUnroll2 &unroll_once_if(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if constexpr (Enable) {
      return unroll_once(callback);
    }

    return *this;
  }

  // Processes trailing data.
  template <typename CallbackType>
  LoopUnroll2 &tail(CallbackType callback) INTRINSICCV_STREAMING_COMPATIBLE {
    while (index_ < length_) {
      disable_loop_vectorization();
      callback(index_++);
    }

    return *this;
  }

  // Processes all remaining data at once.
  template <typename CallbackType>
  LoopUnroll2 &remaining(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (remaining_length()) {
      callback(index_, length_);
      index_ = length_;
    }

    return *this;
  }

  // Instructs the loop logic to try to avoid the tail loop.
  LoopUnroll2 &try_avoid_tail_loop() INTRINSICCV_STREAMING_COMPATIBLE {
    try_avoid_tail_loop_ = true;
    return *this;
  }

  // Returns true if there is nothing left to process.
  bool empty() const INTRINSICCV_STREAMING_COMPATIBLE {
    return length_ == index_;
  }

  // Returns the step value.
  size_t step() const INTRINSICCV_STREAMING_COMPATIBLE { return step_; }

  // Returns the remaining length.
  size_t remaining_length() const INTRINSICCV_STREAMING_COMPATIBLE {
    return length_ - index_;
  }

 private:
  template <const size_t UnrollFactor, typename CallbackType>
  LoopUnroll2 &unroll_n_times(CallbackType callback)
      INTRINSICCV_STREAMING_COMPATIBLE {
    const size_t n_step = UnrollFactor * step();
    size_t max_index = index_ + (remaining_length() / n_step) * n_step;

    while (remaining_length()) {
      while (index_ < max_index) {
        callback(index_);
        index_ += n_step;
      }

      // Try to avoid the tail loop.
      if ((UnrollFactor == 1) && remaining_length() && try_avoid_tail_loop_ &&
          (length_ >= n_step)) {
        index_ = length_ - n_step;
        max_index = length_;
      } else {
        break;
      }
    }  // while (remaining_length())

    return *this;
  }

  size_t length_;
  size_t step_;
  size_t index_;
  bool try_avoid_tail_loop_;
};  // end of class LoopUnroll2

}  // namespace intrinsiccv

#endif  // INTRINSICCV_UTILS_H
