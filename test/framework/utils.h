// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
#define INTRINSICCV_TEST_FRAMEWORK_UTILS_H_

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <type_traits>

#include "framework/abstract.h"

#define INTRINSICCV_API(name, impl, type)                                     \
  template <typename ElementType,                                             \
            std::enable_if_t<std::is_same_v<ElementType, type>, bool> = true> \
  static decltype(auto) name() {                                              \
    return &impl;                                                             \
  }

namespace test {

class Options {
 public:
  /// Returns the vector length being tested. This is in bytes.
  static size_t vector_length() { return vector_length_; }

  /// Returns seed to use.
  static uint64_t seed() { return seed_; }

  /// Returns the number of lanes in a vector for a given integral type.
  template <typename ElementType,
            std::enable_if_t<std::is_integral_v<ElementType>, bool> = true>
  static size_t vector_lanes() {
    return vector_length_ / sizeof(ElementType);
  }

  /// Sets the vector length in bytes.
  static void set_vector_length(size_t value) {
    // Check for power of two.
    if ((value == 0) || ((value & (value - 1)) != 0)) {
      std::cerr << "Vector length must be a power of two: " << value
                << std::endl;
      std::exit(1);
    }

    vector_length_ = value;
  }

  /// Sets the seed.
  static void set_seed(uint64_t value) { seed_ = value; }

 private:
  /// Vector length being tested.
  static size_t vector_length_;
  /// Seed to use.
  static uint64_t seed_;
};  // end of class Options

/// Prints all the elements in a two-dimensional space.
template <typename ElementType>
void dump(const TwoDimensional<ElementType> *elements);

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
