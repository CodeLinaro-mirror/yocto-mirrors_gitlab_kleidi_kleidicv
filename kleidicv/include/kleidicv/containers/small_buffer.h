// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_SMALL_BUFFER_H
#define KLEIDICV_SMALL_BUFFER_H

#include <cstdlib>

namespace kleidicv {

// A buffer that goes on the stack if it's small enough, and otherwise is
// allocated on the heap. Similar to the "small vector" idea, but its size is
// fixed at creation time.
template <typename T, size_t SizeOnStack>
class SmallBuffer {
 public:
  static_assert(std::is_trivial_v<T>);

  explicit SmallBuffer(size_t size)
      : ptr_(size <= SizeOnStack
                 ? buf_
                 : reinterpret_cast<T *>(std::malloc(size * sizeof(T)))) {}

  // non-copyable
  SmallBuffer(const SmallBuffer &) = delete;
  SmallBuffer &operator=(const SmallBuffer &) = delete;

  ~SmallBuffer() {
    if (ptr_ != buf_) {
      std::free(ptr_);
    }
  }

  T *get() { return ptr_; }

 private:
  T *ptr_;
  T buf_[SizeOnStack];
};

}  // namespace kleidicv

#endif  // KLEIDICV_SMALL_BUFFER_H
