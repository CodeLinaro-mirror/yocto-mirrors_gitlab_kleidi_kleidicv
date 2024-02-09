// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"

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

TYPED_TEST(Split, OneChannelOutOfRange) {
  test_not_implemented<TypeParam, 1>(INTRINSICCV_ERROR_RANGE);
}

TYPED_TEST(Split, FiveChannelsNotImplemented) {
  test_not_implemented<TypeParam, 5>();
}

TEST(Split128, NotImplemented) { test_not_implemented<__uint128_t, 2>(); }

TYPED_TEST(Split, NullPointer) {
  const size_t kChannels = 4;

  TypeParam src_data[kChannels];
  size_t src_stride = kChannels * sizeof(TypeParam);
  TypeParam dst_arrays[kChannels];
  void* dst_data[kChannels] = {dst_arrays, dst_arrays + 1, dst_arrays + 2,
                               dst_arrays + 3};
  size_t dst_strides[kChannels] = {sizeof(TypeParam), sizeof(TypeParam),
                                   sizeof(TypeParam), sizeof(TypeParam)};

  test::test_null_args(intrinsiccv_split, src_data, src_stride, dst_data,
                       dst_strides, 1, 1, kChannels, sizeof(TypeParam));

  for (int channels = 2; channels <= 4; ++channels) {
    for (int null_src = 0; null_src < channels; ++null_src) {
      for (int i = 0; i < channels; ++i) {
        dst_data[i] = (i == null_src) ? nullptr : dst_arrays + i;
      }
      EXPECT_EQ(INTRINSICCV_ERROR_NULL_POINTER,
                intrinsiccv_split(src_data, src_stride, dst_data, dst_strides,
                                  1, 1, channels, sizeof(TypeParam)));
    }
  }
}

TYPED_TEST(Split, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }

  const size_t kChannels = 4;
  // A size comfortably large enough to hold the data, taking into account the
  // various offsets that this test will make.
  const size_t kBufSize = kChannels * sizeof(TypeParam) * 2;
  alignas(TypeParam) char src_data[kBufSize];
  size_t src_stride = kChannels * sizeof(TypeParam);
  alignas(TypeParam) char dst_arrays[kBufSize];
  char* dst_data[kChannels] = {};
  size_t dst_strides[kChannels] = {};

  auto init = [&]() {
    for (size_t i = 0; i < kChannels; ++i) {
      dst_data[i] = dst_arrays + sizeof(TypeParam) * i;
      dst_strides[i] = sizeof(TypeParam);
    }
  };

  auto check_split = [&](int channels, void* src_maybe_misaligned,
                         size_t src_stride_maybe_misaligned) {
    EXPECT_EQ(
        INTRINSICCV_ERROR_ALIGNMENT,
        intrinsiccv_split(src_maybe_misaligned, src_stride_maybe_misaligned,
                          reinterpret_cast<void**>(dst_data), dst_strides, 1, 1,
                          channels, sizeof(TypeParam)));
  };

  for (size_t channels = 2; channels <= kChannels; ++channels) {
    init();

    // Misaligned source pointer
    check_split(channels, src_data + 1, src_stride);

    // Misaligned source stride
    check_split(channels, src_data, src_stride + 1);

    for (size_t misaligned_channel = 0; misaligned_channel < channels;
         ++misaligned_channel) {
      // Misaligned destination pointer
      init();
      ++dst_data[misaligned_channel];
      check_split(channels, src_data, src_stride);

      // Misaligned destination stride
      init();
      ++dst_strides[misaligned_channel];
      check_split(channels, src_data, src_stride);
    }
  }
}

TYPED_TEST(Split, ImageSize) {
  const size_t kChannels = 2;
  TypeParam src[1], dst1[1], dst2[1];
  const size_t src_stride = kChannels * sizeof(TypeParam);
  void* dsts[kChannels] = {dst1, dst2};
  size_t dst_strides[kChannels] = {sizeof(TypeParam), sizeof(TypeParam)};

  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_split(src, src_stride, dsts, dst_strides,
                              INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, kChannels,
                              sizeof(TypeParam)));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            intrinsiccv_split(src, src_stride, dsts, dst_strides,
                              INTRINSICCV_MAX_IMAGE_PIXELS,
                              INTRINSICCV_MAX_IMAGE_PIXELS, kChannels,
                              sizeof(TypeParam)));
}
