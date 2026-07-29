// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_THREAD_HEAP_ARRAY_H
#define KLEIDICV_THREAD_HEAP_ARRAY_H

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

namespace kleidicv::thread_internal {

// Minimal owning storage for the threaded C API. It deliberately uses the C
// allocator so linking kleidicv_thread does not require a C++ runtime library.
template <typename T>
class HeapArray {
 public:
  static_assert(alignof(T) <= alignof(std::max_align_t),
                "HeapArray does not support over-aligned types");

  HeapArray() = default;
  ~HeapArray() { reset(); }

  HeapArray(HeapArray &&other) noexcept
      : data_{other.data_}, size_{other.size_} {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  HeapArray &operator=(HeapArray &&other) noexcept {
    if (this != &other) {
      reset();
      data_ = other.data_;
      size_ = other.size_;
      other.data_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  [[nodiscard]] bool allocate(size_t count) {
    if (count == 0) {
      return true;
    }
    if (data_ || count > std::numeric_limits<size_t>::max() / sizeof(T)) {
      return false;
    }

    data_ = static_cast<T *>(std::malloc(count * sizeof(T)));
    if (!data_) {
      return false;
    }
    size_ = count;
    for (size_t i = 0; i < size_; ++i) {
      ::new (static_cast<void *>(data_ + i)) T{};
    }
    return true;
  }

  [[nodiscard]] bool allocate_and_fill(size_t count, const T &initial_value) {
    if (!allocate(count)) {
      return false;
    }
    for (size_t i = 0; i < size_; ++i) {
      data_[i] = initial_value;
    }
    return true;
  }

  T *data() { return data_; }
  const T *data() const { return data_; }
  size_t size() const { return size_; }

  T *begin() { return data_; }
  const T *begin() const { return data_; }
  T *end() { return data_ ? data_ + size_ : nullptr; }
  const T *end() const { return data_ ? data_ + size_ : nullptr; }

  T &operator[](size_t index) { return data_[index]; }
  const T &operator[](size_t index) const { return data_[index]; }

  HeapArray(const HeapArray &) = delete;
  HeapArray &operator=(const HeapArray &) = delete;

 private:
  void reset() {
    for (size_t i = 0; i < size_; ++i) {
      data_[i].~T();
    }
    std::free(data_);
    data_ = nullptr;
    size_ = 0;
  }

  T *data_{nullptr};
  size_t size_{0};
};

}  // namespace kleidicv::thread_internal

#endif  // KLEIDICV_THREAD_HEAP_ARRAY_H
