// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include <type_traits>

#include "framework/operation.h"

template <typename ElementType>
class SplitOperation : public OperationTest<ElementType, 1, 4> {};

template <typename ElementType, size_t ChannelNumber>
class SplitTest final : public SplitOperation<ElementType> {
  /// Expose constructor of base class.
  using SplitOperation<ElementType>::SplitOperation;
  using ArrayType = test::Array2D<ElementType>;

 public:
  SplitTest() : channelnumber_(ChannelNumber) {}

  void test() override {
    for (auto& input : this->inputs_) {
      input = ArrayType{this->width(), this->height(), this->padding(),
                        channelnumber()};
      ASSERT_TRUE(input.valid());
    }

    for (auto& expected : this->expected_) {
      expected = ArrayType{this->width(), this->height(), this->padding()};
      ASSERT_TRUE(expected.valid());
    }
    for (auto& actual : this->actual_) {
      actual = ArrayType{this->width(), this->height(), this->padding()};
      ASSERT_TRUE(actual.valid());
    }

    this->setup();
    call_api();
    this->check();
  }

 protected:
  using Elements = typename SplitOperation<ElementType>::Elements;

  /// Calls the API-under-test in the appropriate way.
  void call_api() override {
    ElementType* dsts[] = {this->actual_[0].data(), this->actual_[1].data(),
                           this->actual_[2].data(), this->actual_[3].data()};
    size_t dst_strides[] = {
        this->actual_[0].stride(), this->actual_[1].stride(),
        this->actual_[2].stride(), this->actual_[3].stride()};

    intrinsiccv_split(this->inputs_[0].data(), this->inputs_[0].stride(),
                      reinterpret_cast<void**>(dsts), dst_strides,
                      this->width(), this->height(), channelnumber(),
                      sizeof(ElementType));
  }

  /// Returns different test data for signed and unsigned element types.
  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {{1, 2, 3, 4}};
    return kTestElements;
  }

  /// Prepares inputs and expected outputs for the operation.
  void setup() override {
    auto elements_list = test_elements();
    // Check that the number of elements fit into the buffers.
    ASSERT_LE(elements_list.size(), this->height());

    size_t row_index = 0;
    for (auto elements : elements_list) {
      // Fill elements one by one.
      for (size_t column_index = 0; column_index < this->inputs_[0].width();
           ++column_index) {
        this->inputs_[0].set(row_index, column_index,
                             {elements.values[column_index % channelnumber()]});
      }
      for (size_t column_index = 0; column_index < this->expected_[0].width();
           ++column_index) {
        for (size_t index = 0; index < channelnumber(); ++index) {
          this->expected_[index].set(
              row_index, column_index,
              {elements.values[index % channelnumber()]});
        }
      }

      // Increment loop counter.
      ++row_index;
    }
  }
  // Returns the number of channel of the output.
  size_t channelnumber() const { return channelnumber_; }

 private:
  size_t channelnumber_;
};  // end of class MergeTest<ElementType>

template <typename ElementType>
class Split : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(Split, ElementTypes);

// /// Tests \ref intrinsiccv_merge API.
TYPED_TEST(Split, API) {
  // Test without padding.
  SplitTest<TypeParam, 2>{}.test();
  SplitTest<TypeParam, 3>{}.test();
  SplitTest<TypeParam, 4>{}.test();
  // Test with padding.
  SplitTest<TypeParam, 2>{}.with_padding(test::Options::vector_length()).test();
  SplitTest<TypeParam, 3>{}.with_padding(test::Options::vector_length()).test();
  SplitTest<TypeParam, 4>{}.with_padding(test::Options::vector_length()).test();
}
