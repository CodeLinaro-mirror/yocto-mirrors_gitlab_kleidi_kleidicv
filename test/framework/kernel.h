// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_KERNEL_H_
#define INTRINSICCV_TEST_FRAMEWORK_KERNEL_H_

#include "framework/abstract.h"
#include "framework/array.h"
#include "framework/types.h"

namespace test {

/// Represents a kernel operator.
template <typename ElementType>
class Kernel : protected Array2D<ElementType>, public Bordered {
 public:
  // Make some features of Array2D<ElementType> base class public.
  using Array2D<ElementType>::at;
  using Array2D<ElementType>::channels;
  using Array2D<ElementType>::height;
  using Array2D<ElementType>::width;

  explicit Kernel(Array2D<ElementType> mask)
      : Array2D<ElementType>(mask),
        anchor_{mask.width() / 2, mask.height() / 2} {}

  /// Returns the anchor point of the kernel.
  Point anchor() const { return anchor_; }

  /// Returns the number of elements to the left of the anchor point.
  size_t left() const override { return anchor().x; }

  /// Returns the number of elements above the anchor point.
  size_t top() const override { return anchor().y; }

  /// Returns the number of elements to the right of the anchor point.
  size_t right() const override {
    return (width() > 0) ? width() - anchor().x - 1 : 0;
  }

  /// Returns the number of elements above the anchor point.
  size_t bottom() const override {
    return (height() > 0) ? height() - anchor().y - 1 : 0;
  }

 private:
  /// The anchor point of the kernel.
  Point anchor_;
};  // end of class Kernel<ElementType>

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_KERNEL_H_
