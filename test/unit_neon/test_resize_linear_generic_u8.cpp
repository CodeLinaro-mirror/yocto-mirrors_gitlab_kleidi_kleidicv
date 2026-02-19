// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "kleidicv/src/resize/resize_linear_generic_u8_neon.h"

// As the tests reuse internals of the implementations the tests are added to
// the production namespace
namespace kleidicv::neon::resize_linear_generic_u8 {

template <size_t kRatio, size_t kChannels>
static std::pair<std::vector<FullVectorInterpolationConstants>,
                 std::vector<HalfVectorInterpolationConstants>>
reference_interpolation_constants(size_t src_width, size_t dst_width) {
  auto to_src_x = [src_width, dst_width](uint64_t dx) {
    return aligned_scale(dx, src_width, dst_width);
  };

  size_t two_x = 0;
  if ((src_width * kChannels) >= (kStep * kRatio)) {
    two_x = (dst_width * kChannels) / (kStep * 2);
  }
  uint64_t dst_index = 0;
  std::vector<FullVectorInterpolationConstants> full_result(two_x * 2);
  for (auto& constants : full_result) {
    uint64_t dx = dst_index / kChannels;
    uint64_t sx_base = to_src_x(dx);
    ptrdiff_t src_element_base = static_cast<ptrdiff_t>(
        ((sx_base >> kFixpBits) * kChannels) + (dst_index % kChannels));

    // Pullback if needed
    if ((src_element_base + (kStep * kRatio)) > (src_width * kChannels)) {
      src_element_base = (src_width * kChannels) - (kStep * kRatio);
    }
    constants.src_element_index = src_element_base;

    for (size_t i = 0; i < kStep; ++i, ++dst_index) {
      dx = dst_index / kChannels;
      uint64_t sx = to_src_x(dx);
      // Get the high half of the fractional part
      constants.xfrac[i] = (sx & ((1 << kFixpBits) - 1)) >> (kFixpBits / 2);

      unsigned in_pixel_index = dst_index % kChannels;
      ptrdiff_t src_index = static_cast<ptrdiff_t>(
          ((sx >> kFixpBits) * kChannels) + in_pixel_index);
      constants.idx[i] = src_index - src_element_base;
    }
  }

  size_t remaining_dst_elements = (dst_width * kChannels) - dst_index;
  size_t half = remaining_dst_elements / kHalfStep;
  if ((remaining_dst_elements % kHalfStep) != 0) {
    half++;
  }

  std::vector<HalfVectorInterpolationConstants> half_result(half);
  for (auto& constants : half_result) {
    uint64_t dx = dst_index / kChannels;
    uint64_t sx_base = to_src_x(dx);
    ptrdiff_t src_element_base = static_cast<ptrdiff_t>(
        ((sx_base >> kFixpBits) * kChannels) + (dst_index % kChannels));

    // Pullback src if needed
    if ((src_element_base + (kStep * (kRatio - 1))) > (src_width * kChannels)) {
      src_element_base = (src_width * kChannels) - (kStep * (kRatio - 1));
    }
    constants.src_element_index = src_element_base;

    // Pullback dst if needed
    if ((dst_index + kHalfStep) > (dst_width * kChannels)) {
      dst_index = (dst_width * kChannels) - kHalfStep;
    }
    constants.dst_element_index = static_cast<ptrdiff_t>(dst_index);

    for (size_t i = 0; i < kHalfStep; ++i, ++dst_index) {
      dx = dst_index / kChannels;
      uint64_t sx = to_src_x(dx);
      // Get the high half of the fractional part
      constants.xfrac[i] = (sx & ((1 << kFixpBits) - 1)) >> (kFixpBits / 2);

      unsigned in_pixel_index = dst_index % kChannels;
      ptrdiff_t src_index = static_cast<ptrdiff_t>(
          ((sx >> kFixpBits) * kChannels) + in_pixel_index);
      constants.idx[i] = src_index - src_element_base;
    }
  }

  return {full_result, half_result};
}

template <typename ConstantsStruct>
void compare_constants(ConstantsStruct* actual_array,
                       std::vector<ConstantsStruct> ref_vector) {
  constexpr size_t kIdxFracLen =
      std::is_same_v<ConstantsStruct, FullVectorInterpolationConstants> ? 16
                                                                        : 8;
  std::string vector_name_log_message =
      std::is_same_v<ConstantsStruct, FullVectorInterpolationConstants>
          ? "full_vector_constants["
          : "half_vector_constants[";

  for (size_t i = 0; i < ref_vector.size(); ++i) {
    const auto& expected = ref_vector[i];
    const auto& actual = actual_array[i];

    if (expected.src_element_index != actual.src_element_index) {
      ADD_FAILURE() << vector_name_log_message << i
                    << "].src_element_index mismatch, expected "
                    << expected.src_element_index << " got "
                    << actual.src_element_index;
    }

    if constexpr (std::is_same_v<ConstantsStruct,
                                 HalfVectorInterpolationConstants>) {
      if (expected.dst_element_index != actual.dst_element_index) {
        ADD_FAILURE() << vector_name_log_message << i
                      << "].dst_element_index mismatch, expected "
                      << expected.dst_element_index << " got "
                      << actual.dst_element_index;
      }
    }

    if (std::memcmp(expected.idx, actual.idx, sizeof(expected.idx)) != 0) {
      size_t diff = 0;
      while (diff < kIdxFracLen && expected.idx[diff] == actual.idx[diff]) {
        ++diff;
      }
      ADD_FAILURE() << vector_name_log_message << i
                    << "].idx mismatch at offset " << diff << ", expected "
                    << static_cast<int>(expected.idx[diff]) << " got "
                    << static_cast<int>(actual.idx[diff]);
    }

    if (std::memcmp(expected.xfrac, actual.xfrac, sizeof(expected.xfrac)) !=
        0) {
      size_t diff = 0;
      while (diff < kIdxFracLen && expected.xfrac[diff] == actual.xfrac[diff]) {
        ++diff;
      }
      ADD_FAILURE() << vector_name_log_message << i
                    << "].xfrac mismatch at offset " << diff << ", expected "
                    << expected.xfrac[diff] << " got " << actual.xfrac[diff];
    }
  }
}

template <size_t kChannels, size_t kSrcWidth, size_t kDstWidth>
void generator_test() {
  double ratio = double{kSrcWidth} / double{kDstWidth};
  ASSERT_TRUE(ratio > 1.0 && ratio < 3.0);
  constexpr size_t kRatio = (kSrcWidth / kDstWidth) + 1;

  // Refenence
  auto [ref_full, ref_half] =
      reference_interpolation_constants<kRatio, kChannels>(kSrcWidth,
                                                           kDstWidth);

  // Actual
  RowInterpolationConstantsGenerator<kRatio, kChannels> generator{kSrcWidth,
                                                                  kDstWidth};
  auto actual_variant = generator();
  auto* ptr = std::get_if<RowInterpolationConstants>(&actual_variant);
  ASSERT_NE(nullptr, ptr);
  auto& actual_constants = *ptr;

  // Comparisons of full vectors
  auto* actual_full = actual_constants.full_vector_constants_array();
  ASSERT_NE(nullptr, actual_full);
  ASSERT_EQ(ref_full.size(), actual_constants.num_of_vector_paths().two_x * 2);

  compare_constants(actual_full, ref_full);

  // Comparisons of half vectors
  auto* actual_half = actual_constants.half_vector_constants_array();
  ASSERT_NE(nullptr, actual_half);
  ASSERT_EQ(ref_half.size(), actual_constants.num_of_vector_paths().half);

  compare_constants(actual_half, ref_half);
}

}  // namespace kleidicv::neon::resize_linear_generic_u8

TEST(GenericResize_u8, rounding_div) {
  EXPECT_EQ(1, kleidicv::neon::resize_linear_generic_u8::rounding_div(4, 5));
}

TEST(GenericResize_u8_Generator, 2channels_two_x_only_no_pullback) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 63, 32>();
}

TEST(GenericResize_u8_Generator, 2channels_two_x_only_with_pullback) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 33, 32>();
}

TEST(GenericResize_u8_Generator, 2channels_half_only_with_pullback) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 17, 16>();
}

TEST(GenericResize_u8_Generator, 2channels_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 149, 101>();
}

TEST(GenericResize_u8_Generator, 3channels_half_only_with_pullback) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 6, 5>();
}

TEST(GenericResize_u8_Generator, 3channels_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 14, 11>();
}

TEST(GenericResize_u8_Generator, 3channels_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 149, 101>();
}
