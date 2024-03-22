// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_UTILS_H
#define INTRINSICCV_UTILS_H

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

#include "intrinsiccv/config.h"
#include "intrinsiccv/ctypes.h"

namespace INTRINSICCV_TARGET_NAMESPACE {

// Saturating cast from signed to unsigned type.
template <typename S, typename U,
          std::enable_if_t<std::numeric_limits<S>::is_signed, bool> = true,
          std::enable_if_t<not std::numeric_limits<U>::is_signed, bool> = true>
static U saturating_cast(S value) INTRINSICCV_STREAMING_COMPATIBLE {
  if (value > std::numeric_limits<U>::max()) {
    return std::numeric_limits<U>::max();
  }
  if (value < 0) {
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
        index_(0),
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
    // In practice step will never be zero and we don't want to spend
    // instructions on checking that.
    // NOLINTBEGIN(clang-analyzer-core.DivideZero)
    const size_t max_index = remaining_length() / step;
    // NOLINTEND(clang-analyzer-core.DivideZero)

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

    callback(step() - remaining_length());
    length_ = step();
    return true;
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

// Check whether any of the arguments are null pointers.
template <typename... Pointers>
bool any_null(Pointers... pointers) INTRINSICCV_STREAMING_COMPATIBLE {
  return (... || (pointers == nullptr));
}

#define CHECK_POINTERS(...)                                    \
  do {                                                         \
    if (INTRINSICCV_TARGET_NAMESPACE::any_null(__VA_ARGS__)) { \
      return INTRINSICCV_ERROR_NULL_POINTER;                   \
    }                                                          \
  } while (false)

template <typename AlignType, typename Value>
bool is_misaligned(Value v) INTRINSICCV_STREAMING_COMPATIBLE {
  constexpr size_t kMask = alignof(AlignType) - 1;
  static_assert(kMask == 0b0001 || kMask == 0b0011 || kMask == 0b0111 ||
                kMask == 0b1111);
  return (v & kMask) != 0;
}

// Return value aligned up to the next multiple of alignment
// Assumes alignment is a power of two.
template <typename T>
T align_up(T value, size_t alignment) INTRINSICCV_STREAMING_COMPATIBLE {
  return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
T *align_up(T *value, size_t alignment) INTRINSICCV_STREAMING_COMPATIBLE {
  // NOLINTBEGIN(performance-no-int-to-ptr)
  return reinterpret_cast<T *>(
      align_up(reinterpret_cast<uintptr_t>(value), alignment));
  // NOLINTEND(performance-no-int-to-ptr)
}

// Specialisation for when stride misalignment is possible.
template <typename T>
std::enable_if_t<alignof(T) != 1, intrinsiccv_error_t> check_pointer_and_stride(
    T *pointer, size_t stride) INTRINSICCV_STREAMING_COMPATIBLE {
  if (pointer == nullptr) {
    return INTRINSICCV_ERROR_NULL_POINTER;
  }
  if (is_misaligned<T>(stride)) {
    return INTRINSICCV_ERROR_ALIGNMENT;
  }
  return INTRINSICCV_OK;
}

// Specialisation for when stride misalignment is impossible.
template <typename T>
std::enable_if_t<alignof(T) == 1, intrinsiccv_error_t> check_pointer_and_stride(
    T *pointer, size_t /*stride*/) INTRINSICCV_STREAMING_COMPATIBLE {
  if (pointer == nullptr) {
    return INTRINSICCV_ERROR_NULL_POINTER;
  }
  return INTRINSICCV_OK;
}

#define CHECK_POINTER_AND_STRIDE(pointer, stride)                             \
  do {                                                                        \
    if (intrinsiccv_error_t ptr_stride_err =                                  \
            INTRINSICCV_TARGET_NAMESPACE::check_pointer_and_stride(pointer,   \
                                                                   stride)) { \
      return ptr_stride_err;                                                  \
    }                                                                         \
  } while (false)

#define MAKE_POINTER_CHECK_ALIGNMENT(ElementType, name, from)     \
  if constexpr (alignof(ElementType) > 1) {                       \
    if (INTRINSICCV_TARGET_NAMESPACE::is_misaligned<ElementType>( \
            reinterpret_cast<uintptr_t>(from))) {                 \
      return INTRINSICCV_ERROR_ALIGNMENT;                         \
    }                                                             \
  }                                                               \
  ElementType *name = reinterpret_cast<ElementType *>(from)

// Check whether the image size is acceptable by limiting it.
#define CHECK_IMAGE_SIZE(width, height)                       \
  do {                                                        \
    size_t image_size = 0;                                    \
    if (__builtin_mul_overflow(width, height, &image_size)) { \
      return INTRINSICCV_ERROR_RANGE;                         \
    }                                                         \
                                                              \
    if (image_size > INTRINSICCV_MAX_IMAGE_PIXELS) {          \
      return INTRINSICCV_ERROR_RANGE;                         \
    }                                                         \
  } while (false)

// Check whether the rectangle size is acceptable by limiting it.
#define CHECK_RECTANGLE_SIZE(rect) CHECK_IMAGE_SIZE(rect.width, rect.height)

}  // namespace INTRINSICCV_TARGET_NAMESPACE

#endif  // INTRINSICCV_UTILS_H
