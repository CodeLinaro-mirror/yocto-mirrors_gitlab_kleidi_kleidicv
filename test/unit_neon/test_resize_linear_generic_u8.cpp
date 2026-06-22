// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "framework/utils.h"
#include "kleidicv/src/resize/resize_linear_generic_u8_neon.h"
#include "test_config.h"

// As the tests reuse internals of the implementations the tests are added to
// the production namespace
namespace kleidicv::neon::resize_linear_generic_u8 {

template <size_t kRatio, size_t kChannels, bool kUpsize>
static std::pair<std::vector<FullVectorInterpolationConstants>,
                 std::vector<HalfVectorInterpolationConstants>>
reference_interpolation_constants(size_t src_width, size_t dst_width) {
  auto to_src_x = [src_width, dst_width](size_t dx) {
    return aligned_scale(static_cast<int64_t>(dx),
                         static_cast<int64_t>(src_width),
                         static_cast<int64_t>(dst_width));
  };

  size_t two_x = 0;
  if ((src_width * kChannels) >= (kStep * kRatio)) {
    two_x = (dst_width * kChannels) / (kStep * 2);
  }
  int64_t dst_index = 0;
  std::vector<FullVectorInterpolationConstants> full_result(two_x * 2);
  for (auto& constants : full_result) {
    int64_t dx = dst_index / kChannels;
    int64_t sx_base = to_src_x(dx);
    ptrdiff_t src_element_base = static_cast<ptrdiff_t>(
        (sx_base >> kFixpBits) * kChannels + (dst_index % kChannels));

    // Pullback if needed
    ptrdiff_t max_src_base_index =
        static_cast<ptrdiff_t>(src_width * kChannels) - kStep * kRatio;
    src_element_base =
        std::clamp<ptrdiff_t>(src_element_base, 0, max_src_base_index);
    constants.src_element_index = src_element_base;

    for (size_t i = 0; i < kStep; ++i, ++dst_index) {
      dx = dst_index / kChannels;
      int64_t sx = to_src_x(dx);
      // Get the high half of the fractional part
      constants.xfrac[i] = (sx & ((1 << kFixpBits) - 1)) >> (kFixpBits - 8);

      unsigned in_pixel_index = dst_index % kChannels;
      int64_t sx0 = sx >> kFixpBits;
      int64_t sx1 = sx0 + 1;
      int64_t max_sx = static_cast<int64_t>(src_width - 1);
      sx0 = std::clamp(sx0, int64_t{0}, max_sx);
      sx1 = std::clamp(sx1, int64_t{0}, max_sx);
      int64_t src_index0 =
          static_cast<int64_t>((sx0 * kChannels) + in_pixel_index);
      int64_t src_index1 =
          static_cast<int64_t>((sx1 * kChannels) + in_pixel_index);
      constants.idx0[i] =
          saturating_cast<int64_t, int8_t>(src_index0 - src_element_base);
      constants.idx1[i] =
          saturating_cast<int64_t, int8_t>(src_index1 - src_element_base);
    }

    if constexpr (kChannels == 3) {
      int8_t min_lane = static_cast<int8_t>(constants.idx0[0]);
      min_lane = std::min(min_lane, static_cast<int8_t>(constants.idx0[1]));
      min_lane = std::min(min_lane, static_cast<int8_t>(constants.idx0[2]));
      ptrdiff_t final_base = std::clamp<ptrdiff_t>(src_element_base + min_lane,
                                                   0, max_src_base_index);
      int8_t adjustment = static_cast<int8_t>(final_base - src_element_base);

      constants.src_element_index = final_base;
      for (size_t i = 0; i < kStep; ++i) {
        constants.idx0[i] =
            static_cast<uint8_t>(constants.idx0[i] - adjustment);
        constants.idx1[i] =
            static_cast<uint8_t>(constants.idx1[i] - adjustment);
      }
    }
  }

  size_t remaining_dst_elements = (dst_width * kChannels) - dst_index;
  size_t half = (remaining_dst_elements + kHalfStep - 1) / kHalfStep;

  std::vector<HalfVectorInterpolationConstants> half_result(half);
  for (auto& constants : half_result) {
    // Pullback dst if needed
    dst_index = std::min(
        dst_index, static_cast<int64_t>(dst_width * kChannels) - kHalfStep);
    constants.dst_element_index = static_cast<ptrdiff_t>(dst_index);

    int64_t dx = dst_index / kChannels;
    int64_t sx_base_fixp = to_src_x(dx);
    ptrdiff_t src_element_base_index =
        static_cast<ptrdiff_t>(((sx_base_fixp >> kFixpBits) * kChannels) +
                               (!kUpsize ? (dst_index % kChannels) : 0));

    // Pullback / pull front src if needed
    size_t src_read_size =
        kStep *
        (!kUpsize && ((kRatio == 2 && kChannels == 3) || kRatio == 3) ? 2 : 1);
    src_element_base_index = std::clamp<ptrdiff_t>(
        src_element_base_index, 0,
        static_cast<ptrdiff_t>(src_width * kChannels) - src_read_size);
    constants.src_element_index = src_element_base_index;

    for (size_t i = 0; i < kHalfStep; ++i, ++dst_index) {
      dx = dst_index / kChannels;
      int64_t sx_fixp = to_src_x(dx);
      // Get the high half of the fractional part
      constants.xfrac[i] =
          (sx_fixp & ((1 << kFixpBits) - 1)) >> (kFixpBits - 8);

      unsigned in_pixel_index = dst_index % kChannels;
      ptrdiff_t sx0 = sx_fixp >> kFixpBits;
      ptrdiff_t sx1 = sx0 + 1;
      ptrdiff_t max_sx = static_cast<ptrdiff_t>(src_width - 1);
      sx0 = std::clamp(sx0, ptrdiff_t{0}, max_sx);
      sx1 = std::clamp(sx1, ptrdiff_t{0}, max_sx);
      ptrdiff_t src_index0 =
          static_cast<ptrdiff_t>((sx0 * kChannels) + in_pixel_index);
      ptrdiff_t src_index1 =
          static_cast<ptrdiff_t>((sx1 * kChannels) + in_pixel_index);
      constants.idx0[i] =
          saturating_cast<int64_t, int8_t>(src_index0 - src_element_base_index);
      constants.idx1[i] =
          saturating_cast<int64_t, int8_t>(src_index1 - src_element_base_index);
    }
  }

  return {full_result, half_result};
}

template <typename ConstantsStruct>
void compare_constants(ConstantsStruct* actual_array,
                       std::vector<ConstantsStruct> ref_vector,
                       int xfrac_tolerance) {
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

    auto compare_idx = [&](const uint8_t* expected_idx,
                           const uint8_t* actual_idx, const char* idx_name) {
      if (std::memcmp(expected_idx, actual_idx, kIdxFracLen) == 0) {
        return;
      }

      size_t diff = 0;
      while (diff < kIdxFracLen && expected_idx[diff] == actual_idx[diff]) {
        ++diff;
      }
      ADD_FAILURE() << vector_name_log_message << i << "]." << idx_name
                    << " mismatch at offset " << diff << ", expected "
                    << static_cast<int>(expected_idx[diff]) << " got "
                    << static_cast<int>(actual_idx[diff]);
    };

    compare_idx(expected.idx0, actual.idx0, "idx0");
    compare_idx(expected.idx1, actual.idx1, "idx1");

    for (size_t xfrac_index = 0; xfrac_index < kIdxFracLen; ++xfrac_index) {
      if (std::abs(static_cast<int>(expected.xfrac[xfrac_index]) -
                   static_cast<int>(actual.xfrac[xfrac_index])) >
          xfrac_tolerance) {
        ADD_FAILURE() << vector_name_log_message << i
                      << "].xfrac mismatch at offset " << xfrac_index
                      << ", expected " << expected.xfrac[xfrac_index] << " got "
                      << actual.xfrac[xfrac_index];
        break;
      }
    }
  }
}

template <size_t kChannels, size_t kSrcWidth, size_t kDstWidth>
void generator_test(int xfrac_tolerance = 0) {
  constexpr size_t kRatio =
      kDstWidth > kSrcWidth && kDstWidth * 14 <= kSrcWidth * 15
          ? 2
          : (kSrcWidth / kDstWidth) + 1;
  constexpr bool kUpsize = kDstWidth >= kSrcWidth;
  static_assert(kRatio >= 1 && kRatio <= 3);

  double ratio = double{kSrcWidth} / double{kDstWidth};
  ASSERT_TRUE(ratio > 0.0 && ratio < 3.0);

  // Reference
  auto [ref_full, ref_half] =
      reference_interpolation_constants<kRatio, kChannels, kUpsize>(kSrcWidth,
                                                                    kDstWidth);

  // Actual
  RowInterpolationConstantsGenerator<kRatio, kChannels, kUpsize> generator{
      kSrcWidth, kDstWidth};
  auto actual_variant = generator();
  auto* ptr = std::get_if<RowInterpolationConstants>(&actual_variant);
  ASSERT_NE(nullptr, ptr);
  auto& actual_constants = *ptr;

  // Comparisons of full vectors
  auto* actual_full = actual_constants.full_vector_constants_array();
  ASSERT_NE(nullptr, actual_full);
  ASSERT_EQ(ref_full.size(), actual_constants.num_of_vector_paths().two_x * 2);

  compare_constants(actual_full, ref_full, xfrac_tolerance);

  // Comparisons of half vectors
  auto* actual_half = actual_constants.half_vector_constants_array();
  ASSERT_NE(nullptr, actual_half);
  ASSERT_EQ(ref_half.size(), actual_constants.num_of_vector_paths().half);

  compare_constants(actual_half, ref_half, xfrac_tolerance);
}

#ifdef KLEIDICV_ALLOCATION_TESTS
template <size_t kChannels, size_t kSrcWidth, size_t kDstWidth>
void rowbuffer_allocation_test() {
  constexpr size_t kRatio =
      kDstWidth > kSrcWidth && kDstWidth * 14 <= kSrcWidth * 15
          ? 2
          : (kSrcWidth / kDstWidth) + 1;
  constexpr bool kUpsize = kDstWidth >= kSrcWidth;
  constexpr size_t kSrcHeight = 2;
  constexpr size_t kDstHeight = 2;
  constexpr size_t kSrcStride = kSrcWidth * kChannels;
  constexpr size_t kDstStride = kDstWidth * kChannels;
  static_assert(kRatio >= 1 && kRatio <= 3);

  RowInterpolationConstantsGenerator<kRatio, kChannels, kUpsize> generator{
      kSrcWidth, kDstWidth};
  auto constants_variant = generator();
  auto* ptr = std::get_if<RowInterpolationConstants>(&constants_variant);
  ASSERT_NE(nullptr, ptr);
  auto& row_interpolation_constants = *ptr;

  std::vector<uint8_t> src(kSrcStride * kSrcHeight);
  std::vector<uint8_t> dst(kDstStride * kDstHeight);
  ResizeGenericU8Operation<kRatio, kChannels, false, kUpsize> operation(
      src.data(), kSrcStride, kSrcHeight, 0, 1, dst.data(), kDstStride,
      kDstWidth, kDstHeight);

  MockMallocToFail::enable();
  auto res = operation.process_rows_separated(row_interpolation_constants);
  MockMallocToFail::disable();
  EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION, res);
}
#endif  // KLEIDICV_ALLOCATION_TESTS
}  // namespace kleidicv::neon::resize_linear_generic_u8

#ifdef KLEIDICV_ALLOCATION_TESTS
TEST(GenericResize_u8, CannotAllocateRowBuffer) {
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<1, 16,
                                                                      17>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<2, 16,
                                                                      17>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<3, 16,
                                                                      17>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<4, 16,
                                                                      17>();

  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<1, 16,
                                                                      28>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<2, 16,
                                                                      28>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<3, 16,
                                                                      28>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<4, 16,
                                                                      28>();

  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<1, 16,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<2, 16,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<3, 16,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<4, 16,
                                                                      9>();

  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<1, 25,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<2, 25,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<3, 25,
                                                                      9>();
  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<4, 25,
                                                                      9>();

  kleidicv::neon::resize_linear_generic_u8::rowbuffer_allocation_test<3, 26,
                                                                      9>();
}
#endif  // KLEIDICV_ALLOCATION_TESTS

TEST(GenericResize_u8, rounding_div) {
  EXPECT_EQ(1, kleidicv::neon::resize_linear_generic_u8::rounding_div(4, 5));
}

TEST(GenericResize_u8_Generator, 1channel_r1_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 16, 17>();
}

TEST(GenericResize_u8_Generator, 1channel_r1_short_big) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 16, 23>();
}

TEST(GenericResize_u8_Generator, 1channel_r2_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 29, 15>();
}

TEST(GenericResize_u8_Generator, 1channel_r2_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 33, 17>();
}

TEST(GenericResize_u8_Generator, 1channel_r3_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 44, 15>();
}

TEST(GenericResize_u8_Generator, 1channel_r3_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<1, 49, 17>();
}

TEST(GenericResize_u8_Generator, 2channels_r1_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 9, 10>();
}

TEST(GenericResize_u8_Generator, 2channels_r1_short_big) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 9, 15>();
}

TEST(GenericResize_u8_Generator, 2channels_r1_mid) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 31, 32>();
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

TEST(GenericResize_u8_Generator, 2channels_r2_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 13, 7>();
}

TEST(GenericResize_u8_Generator, 2channels_r3_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 17, 7>();
}

TEST(GenericResize_u8_Generator, 3channels_half_only_with_pullback) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 11, 5>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_short_small) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 11, 12>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_short_big1) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 7, 11>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_short_big2) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 11, 17>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_short_big3) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 12, 23>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_short_big4) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 13, 23>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_mid1) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 23, 35>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_mid2) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 43, 71>();
}

TEST(GenericResize_u8_Generator, 3channels_r1_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 142, 283>(1);
}

TEST(GenericResize_u8_Generator, 3channels_r2_long) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 149, 101>();
}

TEST(GenericResize_u8_Generator, 3channels_r3_short) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 11, 4>();
}

TEST(GenericResize_u8_Generator, 3channels_r3_mid) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 17, 6>();
}

TEST(GenericResize_u8_Generator, 3channels_r3_long1) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 32, 11>();
}

TEST(GenericResize_u8_Generator, 3channels_r3_long2) {
  kleidicv::neon::resize_linear_generic_u8::generator_test<3, 98, 33>();
}

TEST(GenericResize_u8_Generator, 2channels_extra_long) {
  // For this test, the tolerance is set to 1 because it cannot be guaranteed
  // that the reference implementation produces exactly the same values as the
  // actual algorithm.
  //
  // Reasoning:
  // The reference implementation computes results directly using the simplified
  // formula:
  //      source_x = destination_x * source_width / destination_width
  // ---> rounded to integers.
  //
  // The actual algorithm uses an iterative approach instead of multiplication
  // and division (for better performance), which introduces very small errors,
  // i.e. on the order of 1/256 in the xfrac values.
  //
  // In some corner cases, these small errors can affect rounding. For example,
  // a reference value of 0.4996 may be rounded down, while the actual value is
  // slightly higher and rounded up to 1.
  //
  // These differences are negligible in practice; they only increase the xfrac
  // error from approximately 0.5 to about 0.504.
  kleidicv::neon::resize_linear_generic_u8::generator_test<2, 1479, 813>(1);
}
