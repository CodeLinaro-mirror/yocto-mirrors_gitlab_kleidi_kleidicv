// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_OPERATION_H_
#define INTRINSICCV_TEST_FRAMEWORK_OPERATION_H_

#include <array>
#include <limits>
#include <vector>

#include "framework/array.h"
#include "framework/utils.h"

/// Abstract base class for operations with InputsSize number of inputs and
/// OutputsSize number of outputs.
template <typename ElementType, size_t InputsSize, size_t OutputsSize>
class OperationTest {
 public:
  /// Shorthand for internal data layout representation.
  using ArrayType = test::Array2D<ElementType>;
  /// Shorthand for elements.
  struct Elements {
    ElementType values[InputsSize + OutputsSize];
  };  // end of struct Elements

  virtual ~OperationTest() = default;

  /// Sets the number of padding bytes at the end of rows.
  OperationTest<ElementType, InputsSize, OutputsSize>& with_padding(
      size_t padding) {
    padding_ = padding;
    return *this;
  }

  void test() {
    for (auto& input : inputs_) {
      input = ArrayType{width(), height(), padding()};
      ASSERT_TRUE(input.valid());
    }

    for (auto& expected : expected_) {
      expected = ArrayType{width(), height()};
      ASSERT_TRUE(expected.valid());
    }

    for (auto& actual : actual_) {
      actual = ArrayType{width(), height(), padding()};
      ASSERT_TRUE(actual.valid());
    }

    setup();
    call_api();
    check();
  }

 protected:
  /// Returns test data.
  virtual const std::vector<Elements>& test_elements() = 0;

  /// Prepares inputs and expected outputs for the operation.
  void setup() {
    auto elements_list = test_elements();
    // Check that the number of elements fit into the buffers.
    ASSERT_LE(elements_list.size(), height());

    size_t row_index = 0;
    for (auto elements : elements_list) {
      // Fill elements one vector length apart.
      for (size_t column_index = 0; column_index < width();
           column_index += test::Options::vector_lanes<ElementType>()) {
        for (size_t index = 0; index < inputs_.size(); ++index) {
          inputs_[index].set(row_index, column_index, {elements.values[index]});
        }

        for (size_t index = 0; index < expected_.size(); ++index) {
          expected_[index].set(row_index, column_index,
                               {elements.values[inputs_.size() + index]});
        }
      }

      // Increment loop counter.
      ++row_index;
    }
  }

  /// Calls the API-under-test in the appropriate way.
  virtual void call_api() = 0;

  /// Checks that the result meets the expectations.
  virtual void check() {
    for (size_t index = 0; index < expected_.size(); ++index) {
      EXPECT_EQ_ARRAY2D(expected_[index], actual_[index]);
    }
  }

  /// Tested number of rows.
  virtual size_t height() { return test_elements().size(); }

  /// Tested number of elements in a row.
  size_t width() const {
    // Sufficient number of elements to exercise both vector and scalar paths.
    return 3 * test::Options::vector_lanes<ElementType>() - 1;
  }

  /// Returns the number of padding bytes at the end of rows.
  size_t padding() const { return padding_; }

  /// Returns the minimum value for ElementType.
  static constexpr ElementType min() {
    return std::numeric_limits<ElementType>::min();
  }

  /// Returns the maximum value for ElementType.
  static constexpr ElementType max() {
    return std::numeric_limits<ElementType>::max();
  }

  /// Input operand(s) for the operation.
  std::array<ArrayType, InputsSize> inputs_;
  /// Expected result of the operation.
  std::array<ArrayType, OutputsSize> expected_;
  /// Actual result of the operation.
  std::array<ArrayType, OutputsSize> actual_;
  /// Number of padding bytes at the end of rows.
  size_t padding_{0};
};  // end of class OperationTest<ElementType, InputsSize, OutputsSize>

template <typename ElementType>
class BinaryOperationTest : public OperationTest<ElementType, 2, 1> {
};  // end of class BinaryOperationTest<ElementType>

template <typename ElementType>
class UnaryOperationTest : public OperationTest<ElementType, 1, 1> {
};  // end of class UnaryOperationTest<ElementType>

#endif  // INTRINSICCV_TEST_FRAMEWORK_OPERATION_H_
