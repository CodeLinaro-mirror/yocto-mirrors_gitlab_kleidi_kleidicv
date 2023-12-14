// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_ABSTRACT_H_
#define INTRINSICCV_TEST_FRAMEWORK_ABSTRACT_H_

#include <cstddef>

namespace test {

/// Interface for objects which represent a finite two-dimensional array.
template <typename ElementType>
class TwoDimensional {
 public:
  virtual ~TwoDimensional() = default;

  /// Returns the number of elements in a row of a two-dimensional array.
  virtual size_t width() const = 0;

  /// Returns the number of rows of a two-dimensional array.
  virtual size_t height() const = 0;

  /// Returns the number of channels.
  virtual size_t channels() const { return 1ul; }

  /// Returns a pointer to a data element at a given row and column position, or
  /// nullptr if the requested position is invalid.
  virtual ElementType *at(size_t row, size_t column) = 0;

  /// Returns a const pointer to a data element at a given row and column
  /// position, or nullptr if the requested position is invalid.
  virtual const ElementType *at(size_t row, size_t column) const = 0;
};  // end of class TwoDimensional<ElementType>

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_ABSTRACT_H_
