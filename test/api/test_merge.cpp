// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <intrinsiccv.h>

#include "framework/array.h"
#include "framework/utils.h"

template <typename ElementType, size_t Channels>
class MergeTest final {
 public:
  // Shorthand for internal data layout representation.
  using ArrayType = test::Array2D<ElementType>;

  // Sets the number of padding bytes at the end of rows.
  MergeTest<ElementType, Channels>& with_padding(size_t padding) {
    padding_ = padding;
    return *this;
  }

  // Executes the test
  void test() {
    // Width of input is set to execute 2 vector paths and 1 scalar path
    size_t vector_length = test::Options::vector_length();
    size_t input_width = (vector_length * 2) + 1;
    size_t output_width = input_width * Channels;
    size_t height = 2;

    // Create input and output arrays
    std::array<ArrayType, Channels> inputs;
    for (size_t i = 0; i < Channels; ++i) {
      inputs[i] = ArrayType{input_width, height, padding_};
      inputs[i].fill(ElementType{0});
    }

    ArrayType expected_output{output_width, height, padding_};
    expected_output.fill(ElementType{0});
    ArrayType actual_output{output_width, height, padding_};
    // Prefill actual_outputs with a different value than expected
    actual_output.fill(ElementType{1});

    // Place 4 set of special values in input and expected output. The size of
    // the set is equals to 'Channels'. In the expected output 2 set is placed
    // at the beginning of rows, 2 set at the end of the rows.
    ElementType running_test_value = test::Options::seed() % 50;
    for (size_t i = 0; i < Channels; ++i) {
      inputs[i].set(0, 0, {running_test_value});
      expected_output.set(0, i, {running_test_value});
      running_test_value++;

      inputs[i].set(0, vector_length * 2, {running_test_value});
      expected_output.set(0, (vector_length * 2 * Channels) + i,
                          {running_test_value});
      running_test_value++;

      inputs[i].set(1, 0, {running_test_value});
      expected_output.set(1, i, {running_test_value});
      running_test_value++;

      inputs[i].set(1, vector_length * 2, {running_test_value});
      expected_output.set(1, (vector_length * 2 * Channels) + i,
                          {running_test_value});
      running_test_value++;
    }

    // Call the function to be tested
    const void* input_raw_pointers[Channels];
    for (size_t i = 0; i < Channels; ++i) {
      input_raw_pointers[i] = inputs[i].data();
    }
    size_t strides[Channels];
    for (size_t i = 0; i < Channels; ++i) {
      strides[i] = inputs[i].stride();
    }
    ASSERT_EQ(
        INTRINSICCV_OK,
        intrinsiccv_merge(input_raw_pointers, strides, actual_output.data(),
                          actual_output.stride(), input_width, height, Channels,
                          sizeof(ElementType)));

    // Compare the results
    for (size_t i = 0; i < Channels; ++i) {
      EXPECT_EQ_ARRAY2D(expected_output, actual_output);
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
  ElementType src_arrays[kChannels][width * height] = {{234}};
  ElementType dst[kChannels * width * height] = {123};
  size_t src_strides[kChannels];
  const void* srcs[kChannels];
  for (int i = 0; i < kChannels; ++i) {
    srcs[i] = src_arrays[i];
    src_strides[i] = width * sizeof(ElementType);
  }
  size_t dst_stride = kChannels * width * sizeof(ElementType);

  ASSERT_EQ(expected,
            intrinsiccv_merge(srcs, src_strides, dst, dst_stride, width, height,
                              kChannels, sizeof(ElementType)));

  // Destination should not be modified.
  EXPECT_EQ(123, dst[0]);
}

template <typename ElementType>
class Merge : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(Merge, ElementTypes);

TYPED_TEST(Merge, TwoChannels) {
  MergeTest<TypeParam, 2>().test();
  MergeTest<TypeParam, 2>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Merge, ThreeChannels) {
  MergeTest<TypeParam, 3>().test();
  MergeTest<TypeParam, 3>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Merge, FourChannels) {
  MergeTest<TypeParam, 4>().test();
  MergeTest<TypeParam, 4>().with_padding(test::Options::vector_length()).test();
}

TYPED_TEST(Merge, OneChannelOutOfRange) {
  test_not_implemented<TypeParam, 1>(INTRINSICCV_ERROR_RANGE);
}

TYPED_TEST(Merge, FiveChannelsNotImplemented) {
  test_not_implemented<TypeParam, 5>();
}

TEST(Merge128, NotImplemented) { test_not_implemented<__uint128_t, 2>(); }

TYPED_TEST(Merge, NullPointer) {
  const size_t kChannels = 4;
  TypeParam src_arrays[kChannels];
  TypeParam dst[kChannels];
  size_t src_strides[kChannels] = {sizeof(TypeParam), sizeof(TypeParam),
                                   sizeof(TypeParam), sizeof(TypeParam)};
  const void* srcs[kChannels] = {src_arrays, src_arrays + 1, src_arrays + 2,
                                 src_arrays + 3};
  size_t dst_stride = kChannels * sizeof(TypeParam);
  test::test_null_args(intrinsiccv_merge, srcs, src_strides, dst, dst_stride, 1,
                       1, kChannels, sizeof(TypeParam));

  for (int channels = 2; channels <= 4; ++channels) {
    for (int null_src = 0; null_src < channels; ++null_src) {
      for (int i = 0; i < channels; ++i) {
        srcs[i] = (i == null_src) ? nullptr : src_arrays + i;
      }
      EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
                intrinsiccv_merge(srcs, src_strides, dst, dst_stride, 1, 1,
                                  channels, sizeof(TypeParam)));
    }
  }
}
