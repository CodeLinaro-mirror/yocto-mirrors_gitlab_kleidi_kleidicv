// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
#define INTRINSICCV_TEST_FRAMEWORK_UTILS_H_

#include <cstddef>

namespace test {

class Options {
 public:
  /// Returns the vector length being tested.
  static size_t vector_length() { return vector_length_; }

  /// Sets the vector length.
  static void set_vector_length(size_t value) { vector_length_ = value; }

 private:
  /// Vector length being tested.
  static size_t vector_length_;
};  // end of class Options

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_UTILS_H_
