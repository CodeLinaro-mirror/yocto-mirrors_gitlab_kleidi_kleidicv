// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/array.h"
#include "framework/utils.h"

template <typename ElementType, size_t Channels>
class SplitTest final {
 public:
  // Shorthand for internal data layout representation.
  using ArrayType = test::Array2D<ElementType>;

  // Sets the number of padding bytes at the end of rows.
  SplitTest<ElementType, Channels>& with_padding(size_t padding) {
    padding_ = padding;
    return *this;
  }

  // Executes the test
  void test() {
    // Width of input is set to execute 2 vector paths and 1 scalar path
    size_t vector_length = test::Options::vector_length();
    size_t output_width = (vector_length * 2) + 1;
    size_t input_width = output_width * Channels;
    size_t height = 2;

    // Create input and output arrays
    ArrayType input{input_width, height, padding_};
    input.fill(ElementType{0});

    std::array<ArrayType, Channels> expected_outputs;
    std::array<ArrayType, Channels> actual_outputs;
    for (size_t i = 0; i < Channels; ++i) {
      expected_outputs[i] = ArrayType{output_width, height, padding_};
      expected_outputs[i].fill(ElementType{0});
      actual_outputs[i] = ArrayType{output_width, height, padding_};
      // Prefill actual_outputs with a different value than expected
      actual_outputs[i].fill(ElementType{1});
    }

    // Place 4 set of special values in input and expected output. The size of
    // the set is equals to 'Channels'. In the input 2 set is placed at the
    // beginning of rows, 2 set at the end of the rows.
    ElementType running_test_value = test::Options::seed() % 50;
    for (size_t i = 0; i < Channels; ++i) {
      input.set(0, i, {running_test_value});
      expected_outputs[i].set(0, 0, {running_test_value});
      running_test_value++;

      input.set(0, (vector_length * 2 * Channels) + i, {running_test_value});
      expected_outputs[i].set(0, vector_length * 2, {running_test_value});
      running_test_value++;

      input.set(1, i, {running_test_value});
      expected_outputs[i].set(1, 0, {running_test_value});
      running_test_value++;

      input.set(1, (vector_length * 2 * Channels) + i, {running_test_value});
      expected_outputs[i].set(1, vector_length * 2, {running_test_value});
      running_test_value++;
    }

    // Call the function to be tested
    void* actual_raw_pointers[Channels];
    for (size_t i = 0; i < Channels; ++i) {
      actual_raw_pointers[i] = actual_outputs[i].data();
    }
    size_t strides[Channels];
    for (size_t i = 0; i < Channels; ++i) {
      strides[i] = actual_outputs[i].stride();
    }
    ASSERT_EQ(INTRINSICCV_OK,
              intrinsiccv_split(input.data(), input.stride(),
                                actual_raw_pointers, strides, output_width,
                                height, Channels, sizeof(ElementType)));

    // Compare the results
    for (size_t i = 0; i < Channels; ++i) {
      EXPECT_EQ_ARRAY2D(expected_outputs[i], actual_outputs[i]);
    }
  }

 private:
  // Number of padding bytes at the end of rows.
  size_t padding_{0};
};

template <typename ElementType, int kChannels>
static void test_not_implemented(
    intrinsiccv_error_t expected = INTRINSICCV_ERROR_NOT_IMPLEMENTED) {
  const size_t width = 1, height = 1;

  ElementType src_data[kChannels * width * height] = {234};
  size_t src_stride = kChannels * width * sizeof(ElementType);
  ElementType dst_arrays[kChannels][width * height] = {{123}};
  void* dst_data[kChannels];
  size_t dst_strides[kChannels];
  for (int i = 0; i < kChannels; ++i) {
    dst_data[i] = dst_arrays[i];
    dst_strides[i] = width * sizeof(ElementType);
  }

  ASSERT_EQ(expected,
            intrinsiccv_split(src_data, src_stride, dst_data, dst_strides,
                              width, height, kChannels, sizeof(ElementType)));

  // Destination should not be modified.
  EXPECT_EQ(123, dst_arrays[0][0]);
}

template <typename ElementType>
class Split : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(Split, ElementTypes);

TYPED_TEST(Split, TwoChannels) {
  SplitTest<TypeParam, 2>().test();
  SplitTest<TypeParam, 2>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Split, ThreeChannels) {
  SplitTest<TypeParam, 3>().test();
  SplitTest<TypeParam, 3>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Split, FourChannels) {
  SplitTest<TypeParam, 4>().test();
  SplitTest<TypeParam, 4>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Split, OneChannelNotImplemented) {
  test_not_implemented<TypeParam, 1>();
}

TYPED_TEST(Split, FiveChannelsNotImplemented) {
  test_not_implemented<TypeParam, 5>();
}

TEST(Split128, NotImplemented) { test_not_implemented<__uint128_t, 2>(); }
