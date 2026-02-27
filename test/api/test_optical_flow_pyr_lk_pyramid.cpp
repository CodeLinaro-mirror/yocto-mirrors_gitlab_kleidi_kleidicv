// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/utils.h"
#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/optical_flow/optical_flow_lk_pyramid.h"
#include "test_config.h"

namespace {

// Map one potentially out-of-range coordinate to a valid in-range coordinate
// using reflect-101 behavior.
size_t border_index_reflect_101(ptrdiff_t index, size_t length) {
  if (length <= 1) {
    return 0;
  }

  ptrdiff_t reflected = index;
  while (reflected < 0 || reflected >= static_cast<ptrdiff_t>(length)) {
    if (reflected < 0) {
      reflected = -reflected;
    } else {
      reflected = 2 * static_cast<ptrdiff_t>(length) - reflected - 2;
    }
  }

  return static_cast<size_t>(reflected);
}

// Build a temporary image that contains a 1-pixel reflect-101 border around
// the input interior image.
void build_reflect_101_with_1px_border(const uint8_t* src, size_t src_stride,
                                       size_t width, size_t height,
                                       uint8_t* dst, size_t dst_stride) {
  for (size_t y = 0; y < height + 2; ++y) {
    const size_t src_y =
        border_index_reflect_101(static_cast<ptrdiff_t>(y) - 1, height);
    for (size_t x = 0; x < width + 2; ++x) {
      const size_t src_x =
          border_index_reflect_101(static_cast<ptrdiff_t>(x) - 1, width);
      dst[y * dst_stride + x] = src[src_y * src_stride + src_x];
    }
  }
}

// Compute one output pixel for scalar pyramid downsampling with a separable
// 5x5 Gaussian kernel [1 4 6 4 1] and reflect-101 borders.
uint8_t pyr_down_scalar_at(const uint8_t* src, size_t src_stride, size_t width,
                           size_t height, size_t dst_x, size_t dst_y) {
  static constexpr int kernel[5] = {1, 4, 6, 4, 1};
  const ptrdiff_t source_x = static_cast<ptrdiff_t>(dst_x * 2);
  const ptrdiff_t source_y = static_cast<ptrdiff_t>(dst_y * 2);

  int sum = 0;
  for (ptrdiff_t ky = -2; ky <= 2; ++ky) {
    const size_t sy = border_index_reflect_101(source_y + ky, height);
    for (ptrdiff_t kx = -2; kx <= 2; ++kx) {
      const size_t sx = border_index_reflect_101(source_x + kx, width);
      sum += kernel[ky + 2] * kernel[kx + 2] * src[sy * src_stride + sx];
    }
  }

  // Matches the rounding used by the production pyramid-down path.
  const int rounded = (sum + 128) / 256;
  return static_cast<uint8_t>(rounded);
}

// Scalar reference implementation for one full pyramid level downsample.
void pyr_down_scalar_reflect_101(const uint8_t* src, size_t src_stride,
                                 size_t width, size_t height, uint8_t* dst,
                                 size_t dst_stride) {
  const size_t out_width = (width + 1) / 2;
  const size_t out_height = (height + 1) / 2;

  for (size_t y = 0; y < out_height; ++y) {
    for (size_t x = 0; x < out_width; ++x) {
      dst[y * dst_stride + x] =
          pyr_down_scalar_at(src, src_stride, width, height, x, y);
    }
  }
}

// Saturate an int value to the int16 range.
int16_t saturate_to_s16(int value) {
  if (value > std::numeric_limits<int16_t>::max()) {
    return std::numeric_limits<int16_t>::max();
  }
  if (value < std::numeric_limits<int16_t>::min()) {
    return std::numeric_limits<int16_t>::min();
  }
  return static_cast<int16_t>(value);
}

// Scalar reference for interleaved Scharr gradients (dx, dy) using a
// reflect-101 border around the source image.
void scharr_interleaved_scalar_reflect_101(const uint8_t* src,
                                           size_t src_stride, size_t width,
                                           size_t height, int16_t* dst,
                                           size_t dst_stride) {
  static constexpr int kx[3][3] = {
      {-3, 0, 3},
      {-10, 0, 10},
      {-3, 0, 3},
  };
  static constexpr int ky[3][3] = {
      {-3, -10, -3},
      {0, 0, 0},
      {3, 10, 3},
  };

  test::Array2D<uint8_t> bordered(width + 2, height + 2);
  ASSERT_TRUE(bordered.valid());
  build_reflect_101_with_1px_border(src, src_stride, width, height,
                                    bordered.data(), bordered.stride());

  for (size_t y = 0; y < height; ++y) {
    auto* out_row = reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(dst) +
                                               y * dst_stride);
    for (size_t x = 0; x < width; ++x) {
      int dx = 0;
      int dy = 0;
      for (size_t ky_i = 0; ky_i < 3; ++ky_i) {
        for (size_t kx_i = 0; kx_i < 3; ++kx_i) {
          const int pixel = bordered.at(y + ky_i, x + kx_i)[0];
          dx += kx[ky_i][kx_i] * pixel;
          dy += ky[ky_i][kx_i] * pixel;
        }
      }
      out_row[x * 2] = saturate_to_s16(dx);
      out_row[x * 2 + 1] = saturate_to_s16(dy);
    }
  }
}

}  // namespace

// End-to-end API validation:
// - build pyramid
// - verify level count truncation logic
// - compare each image level against scalar pyramid-down reference
// - compare each Scharr level against scalar Scharr reference
// - verify out-of-range level access fails
TEST(OpticalFlowPyrLKPyramid, API) {
  constexpr size_t width = 32;
  constexpr size_t height = 24;
  constexpr size_t channels = 1;
  constexpr size_t requested_levels = 5;
  constexpr size_t window_width = 5;
  constexpr size_t window_height = 5;

  test::Array2D<uint8_t> src(width, height);
  ASSERT_TRUE(src.valid());
  test::PseudoRandomNumberGenerator<uint8_t> input_value_random_range;
  src.fill(input_value_random_range);

  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), width, height, channels,
                requested_levels, window_width, window_height));
  ASSERT_NE(nullptr, pyramid);

  size_t actual_levels = 0;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                             pyramid, &actual_levels));

  size_t expected_levels = 0;
  size_t expected_width = width;
  size_t expected_height = height;
  for (size_t i = 0; i < requested_levels; ++i) {
    ++expected_levels;
    const size_t next_width = (expected_width + 1) / 2;
    const size_t next_height = (expected_height + 1) / 2;
    if (next_width <= window_width || next_height <= window_height) {
      break;
    }
    expected_width = next_width;
    expected_height = next_height;
  }

  ASSERT_EQ(expected_levels, actual_levels);

  // Build scalar reference image levels.
  std::vector<test::Array2D<uint8_t>> expected_images(actual_levels);
  std::vector<size_t> expected_image_width(actual_levels);
  std::vector<size_t> expected_image_height(actual_levels);

  expected_image_width[0] = width;
  expected_image_height[0] = height;
  expected_images[0] = test::Array2D<uint8_t>{width, height};
  ASSERT_TRUE(expected_images[0].valid());
  expected_images[0].set(0, 0, &src);

  for (size_t level = 1; level < actual_levels; ++level) {
    expected_image_width[level] = (expected_image_width[level - 1] + 1) / 2;
    expected_image_height[level] = (expected_image_height[level - 1] + 1) / 2;

    expected_images[level] = test::Array2D<uint8_t>{
        expected_image_width[level], expected_image_height[level]};
    ASSERT_TRUE(expected_images[level].valid());

    pyr_down_scalar_reflect_101(
        expected_images[level - 1].data(), expected_images[level - 1].stride(),
        expected_image_width[level - 1], expected_image_height[level - 1],
        expected_images[level].data(), expected_images[level].stride());
  }

  for (size_t level = 0; level < actual_levels; ++level) {
    const uint8_t* image_data = nullptr;
    size_t image_stride = 0;
    size_t image_width = 0;
    size_t image_height = 0;
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                               pyramid, level, &image_data, &image_stride,
                               &image_width, &image_height));

    ASSERT_EQ(expected_image_width[level], image_width);
    ASSERT_EQ(expected_image_height[level], image_height);
    ASSERT_GE(image_stride, image_width * channels);

    // Compare image level pixels.
    for (size_t y = 0; y < image_height; ++y) {
      for (size_t x = 0; x < image_width; ++x) {
        ASSERT_EQ(expected_images[level].at(y, x)[0],
                  image_data[y * image_stride + x]);
      }
    }

    const int16_t* scharr_data = nullptr;
    size_t scharr_stride = 0;
    size_t scharr_width = 0;
    size_t scharr_height = 0;
    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                  pyramid, level, &scharr_data, &scharr_stride, &scharr_width,
                  &scharr_height));

    ASSERT_EQ(image_width, scharr_width);
    ASSERT_EQ(image_height, scharr_height);
    ASSERT_GE(scharr_stride, image_width * channels * 2 * sizeof(int16_t));

    // Build and compare scalar Scharr reference for this level.
    test::Array2D<int16_t> expected_scharr(image_width * 2, image_height);
    ASSERT_TRUE(expected_scharr.valid());
    scharr_interleaved_scalar_reflect_101(image_data, image_stride, image_width,
                                          image_height, expected_scharr.data(),
                                          expected_scharr.stride());

    for (size_t y = 0; y < scharr_height; ++y) {
      const auto* actual_row =
          reinterpret_cast<const uint8_t*>(scharr_data) + y * scharr_stride;
      const auto* expected_row =
          reinterpret_cast<const uint8_t*>(expected_scharr.data()) +
          y * expected_scharr.stride();
      for (size_t b = 0; b < image_width * 2 * sizeof(int16_t); ++b) {
        ASSERT_EQ(expected_row[b], actual_row[b]);
      }
    }
  }

  const uint8_t* bad_image_data = nullptr;
  size_t bad_image_stride = 0;
  size_t bad_image_width = 0;
  size_t bad_image_height = 0;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                pyramid, actual_levels, &bad_image_data, &bad_image_stride,
                &bad_image_width, &bad_image_height));

  const int16_t* bad_scharr_data = nullptr;
  size_t bad_scharr_stride = 0;
  size_t bad_scharr_width = 0;
  size_t bad_scharr_height = 0;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                pyramid, actual_levels, &bad_scharr_data, &bad_scharr_stride,
                &bad_scharr_width, &bad_scharr_height));

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// Fixed golden-data regression test.
//
// This test uses precomputed expected values for:
// - level 1 image
// - Scharr level 0
// - Scharr level 1
// and checks exact bitwise equality.
TEST(OpticalFlowPyrLKPyramid, PrecalculatedGolden) {
  static constexpr uint8_t kInputLevel0[64] = {
      3,  17, 25, 9,  0,  31, 44, 12, 7,  21, 29, 13, 4,  35, 48, 16,
      11, 23, 33, 15, 8,  39, 52, 20, 14, 27, 36, 18, 10, 42, 55, 24,
      19, 30, 40, 22, 12, 45, 58, 28, 26, 34, 43, 32, 14, 49, 62, 38,
      37, 46, 53, 41, 16, 57, 70, 50, 47, 54, 60, 51, 18, 63, 75, 59};
  static constexpr uint8_t kInputLevel1[16] = {16, 19, 17, 33, 20, 23, 21, 38,
                                               28, 31, 28, 45, 42, 44, 37, 57};
  static constexpr int16_t kScharrLevel0[128] = {
      0,    0,   352,  0,   -128, 0,   -400, 0,   352,  0,   704,  0,
      -304, 0,   0,    0,   0,    116, 352,  108, -128, 116, -400, 108,
      358,  122, 704,  128, -304, 128, 0,    128, 0,    106, 352,  102,
      -131, 103, -403, 89,  378,  96,  707,  109, -301, 115, 0,    122,
      0,    122, 349,  115, -138, 112, -419, 103, 381,  79,  720,  90,
      -288, 102, 0,    116, 0,    162, 327,  127, -113, 133, -445, 173,
      353,  103, 739,  103, -257, 133, 0,    182, 0,    276, 281,  253,
      -59,  235, -485, 241, 287,  133, 780,  168, -182, 222, 0,    292,
      0,    330, 250,  314, -65,  287, -583, 253, 247,  139, 855,  191,
      -115, 235, 0,    288, 0,    0,   226,  0,   -60,  0,   -642, 0,
      216,  0,   894,  0,   -82,  0,   0,    0};
  static constexpr int16_t kScharrLevel1[32] = {
      0, 0,   16,  0,   230, 0,   0, 0,   0, 192, 13,  189, 234, 182, 0, 186,
      0, 346, -12, 324, 224, 280, 0, 286, 0, 0,   -50, 0,   214, 0,   0, 0};

  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_build_optical_flow_pyr_lk_pyramid(
                             &pyramid, kInputLevel0, 8, 8, 8, 1, 5, 3, 3));
  ASSERT_NE(nullptr, pyramid);

  size_t level_count = 0;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                             pyramid, &level_count));
  ASSERT_EQ(static_cast<size_t>(2), level_count);

  const uint8_t* image0 = nullptr;
  const uint8_t* image1 = nullptr;
  size_t stride0 = 0, width0 = 0, height0 = 0;
  size_t stride1 = 0, width1 = 0, height1 = 0;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                             pyramid, 0, &image0, &stride0, &width0, &height0));
  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                             pyramid, 1, &image1, &stride1, &width1, &height1));
  ASSERT_EQ(static_cast<size_t>(8), width0);
  ASSERT_EQ(static_cast<size_t>(8), height0);
  ASSERT_EQ(static_cast<size_t>(4), width1);
  ASSERT_EQ(static_cast<size_t>(4), height1);
  for (size_t y = 0; y < 8; ++y) {
    for (size_t x = 0; x < 8; ++x) {
      ASSERT_EQ(kInputLevel0[y * 8 + x], image0[y * stride0 + x]);
    }
  }
  for (size_t y = 0; y < 4; ++y) {
    for (size_t x = 0; x < 4; ++x) {
      ASSERT_EQ(kInputLevel1[y * 4 + x], image1[y * stride1 + x]);
    }
  }

  const int16_t* scharr0 = nullptr;
  const int16_t* scharr1 = nullptr;
  size_t sstride0 = 0, swidth0 = 0, sheight0 = 0;
  size_t sstride1 = 0, swidth1 = 0, sheight1 = 0;
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                pyramid, 0, &scharr0, &sstride0, &swidth0, &sheight0));
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                pyramid, 1, &scharr1, &sstride1, &swidth1, &sheight1));
  ASSERT_EQ(static_cast<size_t>(8), swidth0);
  ASSERT_EQ(static_cast<size_t>(8), sheight0);
  ASSERT_EQ(static_cast<size_t>(4), swidth1);
  ASSERT_EQ(static_cast<size_t>(4), sheight1);

  for (size_t y = 0; y < 8; ++y) {
    const auto* row = reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(scharr0) + y * sstride0);
    for (size_t x = 0; x < 16; ++x) {
      ASSERT_EQ(kScharrLevel0[y * 16 + x], row[x]);
    }
  }
  for (size_t y = 0; y < 4; ++y) {
    const auto* row = reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(scharr1) + y * sstride1);
    for (size_t x = 0; x < 8; ++x) {
      ASSERT_EQ(kScharrLevel1[y * 8 + x], row[x]);
    }
  }

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// Null argument contract for builder API.
TEST(OpticalFlowPyrLKPyramidBuild, NullPointer) {
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::test_null_args(kleidicv_build_optical_flow_pyr_lk_pyramid, &pyramid,
                       src.data(), src.stride(), 3, 3, 1, 1, 3, 3);
}

// Parameter validation for range/shape constraints in builder API.
TEST(OpticalFlowPyrLKPyramidBuild, ParameterRange) {
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 0, 3, 1, 1, 3, 3));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 0, 1, 1, 3, 3));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 0, 3, 3));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 2, 3));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 2));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), sizeof(uint8_t) * 2, 3, 3, 1, 1, 3, 3));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), sizeof(uint8_t),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1, 1, 1, 3, 3));
}

// Additional range checks to exercise remaining validation branches.
TEST(OpticalFlowPyrLKPyramidBuild, ParameterRangeAdditionalBranches) {
  uint8_t src[1] = {};
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src, sizeof(uint8_t), 3, 3, 0, 1, 3, 3));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src, sizeof(uint8_t), 3, 3,
                KLEIDICV_MAXIMUM_CHANNEL_COUNT + 1, 1, 3, 3));

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src, sizeof(uint8_t),
                std::numeric_limits<size_t>::max(), 2, 1, 1, 3, 3));
}

// Covers level-selection loop paths:
// 1) natural loop termination by requested level count,
// 2) early break caused by height condition in stop criteria.
TEST(OpticalFlowPyrLKPyramidBuild, LevelSelectionPaths) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  {
    test::Array2D<uint8_t> src{64, 64};
    ASSERT_TRUE(src.valid());
    test::PseudoRandomNumberGenerator<uint8_t> generator;
    src.fill(generator);

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 64, 64, 1, 1, 3, 3));
    ASSERT_NE(nullptr, pyramid);

    size_t level_count = 0;
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                               pyramid, &level_count));
    ASSERT_EQ(static_cast<size_t>(1), level_count);
    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
    pyramid = nullptr;
  }

  {
    test::Array2D<uint8_t> src{64, 5};
    ASSERT_TRUE(src.valid());
    test::PseudoRandomNumberGenerator<uint8_t> generator;
    src.fill(generator);

    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_build_optical_flow_pyr_lk_pyramid(
                  &pyramid, src.data(), src.stride(), 64, 5, 1, 4, 3, 3));
    ASSERT_NE(nullptr, pyramid);

    size_t level_count = 0;
    ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_level_count(
                               pyramid, &level_count));
    ASSERT_EQ(static_cast<size_t>(1), level_count);
    ASSERT_EQ(KLEIDICV_OK,
              kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
  }
}

// For single-row input, src_stride is allowed to be less than width*channels.
TEST(OpticalFlowPyrLKPyramidBuild, SingleRowStrideBelowWidth) {
  uint8_t src_row[8] = {3, 7, 9, 11, 17, 19, 23, 29};
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  ASSERT_EQ(KLEIDICV_OK, kleidicv_build_optical_flow_pyr_lk_pyramid(
                             &pyramid, src_row, 1, 8, 1, 1, 1, 3, 3));
  ASSERT_NE(nullptr, pyramid);

  const uint8_t* image_data = nullptr;
  size_t image_stride = 0;
  size_t image_width = 0;
  size_t image_height = 0;
  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                             pyramid, 0, &image_data, &image_stride,
                             &image_width, &image_height));
  ASSERT_EQ(static_cast<size_t>(8), image_width);
  ASSERT_EQ(static_cast<size_t>(1), image_height);
  for (size_t x = 0; x < 8; ++x) {
    ASSERT_EQ(src_row[x], image_data[x]);
  }

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// Current implementation supports only single-channel input.
TEST(OpticalFlowPyrLKPyramidBuild, UnsupportedChannels) {
  test::Array2D<uint8_t> src{6, 3};
  ASSERT_TRUE(src.valid());
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 2, 1, 3, 3));
}

// Release API must reject null handle.
TEST(OpticalFlowPyrLKPyramidRelease, NullPointer) {
  EXPECT_EQ(KLEIDICV_ERROR_NULL_POINTER,
            kleidicv_optical_flow_pyr_lk_pyramid_release(nullptr));
}

// get_level_count must validate required output pointers.
TEST(OpticalFlowPyrLKPyramidGetLevelCount, NullPointer) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3));

  size_t level_count = 0;
  test::test_null_args(kleidicv_optical_flow_pyr_lk_pyramid_get_level_count,
                       pyramid, &level_count);

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// get_image_level must validate required output pointers.
TEST(OpticalFlowPyrLKPyramidGetImageLevel, NullPointer) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3));

  const uint8_t* data = nullptr;
  size_t stride = 0;
  size_t width = 0;
  size_t height = 0;
  test::test_null_args(kleidicv_optical_flow_pyr_lk_pyramid_get_image_level,
                       pyramid, static_cast<size_t>(0), &data, &stride, &width,
                       &height);

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// get_image_level must reject out-of-range level index.
TEST(OpticalFlowPyrLKPyramidGetImageLevel, InvalidLevel) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3));

  const uint8_t* data = nullptr;
  size_t stride = 0;
  size_t width = 0;
  size_t height = 0;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_optical_flow_pyr_lk_pyramid_get_image_level(
                pyramid, 1, &data, &stride, &width, &height));

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// get_scharr_level must validate required output pointers.
TEST(OpticalFlowPyrLKPyramidGetScharrLevel, NullPointer) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3));

  const int16_t* data = nullptr;
  size_t stride = 0;
  size_t width = 0;
  size_t height = 0;
  test::test_null_args(kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level,
                       pyramid, static_cast<size_t>(0), &data, &stride, &width,
                       &height);

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

// get_scharr_level must reject out-of-range level index.
TEST(OpticalFlowPyrLKPyramidGetScharrLevel, InvalidLevel) {
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  ASSERT_EQ(KLEIDICV_OK,
            kleidicv_build_optical_flow_pyr_lk_pyramid(
                &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3));

  const int16_t* data = nullptr;
  size_t stride = 0;
  size_t width = 0;
  size_t height = 0;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            kleidicv_optical_flow_pyr_lk_pyramid_get_scharr_level(
                pyramid, 1, &data, &stride, &width, &height));

  ASSERT_EQ(KLEIDICV_OK, kleidicv_optical_flow_pyr_lk_pyramid_release(pyramid));
}

TEST(OpticalFlowLKPyramidInternal, CreateOverflowImageStride) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            OpticalFlowLKPyramid::create(
                storage, 1, std::numeric_limits<size_t>::max() / 2, 8, 3));
}

TEST(OpticalFlowLKPyramidInternal, CreateOverflowScharrStride) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;

  const size_t large_channels = std::numeric_limits<size_t>::max() / 4;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            OpticalFlowLKPyramid::create(storage, 1, 1, 1, large_channels));
}

TEST(OpticalFlowLKPyramidInternal, CreateOverflowImageBytes) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;

  const size_t large_channels = std::numeric_limits<size_t>::max() / 12;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            OpticalFlowLKPyramid::create(storage, 1, 1, 3, large_channels));
}

TEST(OpticalFlowLKPyramidInternal, CreateOverflowScharrBytes) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;

  const size_t large_channels = std::numeric_limits<size_t>::max() / 24;
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            OpticalFlowLKPyramid::create(storage, 1, 1, 1, large_channels));
}

TEST(OpticalFlowLKPyramidInternal, ProcessPropagatesScharrErrorLevel0) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;
  ASSERT_EQ(KLEIDICV_OK, OpticalFlowLKPyramid::create(storage, 1, 8, 8, 1));
  ASSERT_NE(nullptr, storage.get());

  test::Array2D<uint8_t> src{8, 8};
  ASSERT_TRUE(src.valid());
  test::PseudoRandomNumberGenerator<uint8_t> generator;
  src.fill(generator);

  auto blur_ok = [](const uint8_t*, size_t, size_t, size_t, uint8_t*, size_t,
                    size_t, kleidicv_border_type_t) { return KLEIDICV_OK; };
  auto scharr_fail = [](const uint8_t*, size_t, size_t, size_t, size_t,
                        int16_t*,
                        size_t) { return KLEIDICV_ERROR_NOT_IMPLEMENTED; };

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            storage->process(src.data(), src.stride(), blur_ok, scharr_fail));
}

TEST(OpticalFlowLKPyramidInternal, ProcessPropagatesBlurError) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;
  ASSERT_EQ(KLEIDICV_OK, OpticalFlowLKPyramid::create(storage, 2, 8, 8, 1));
  ASSERT_NE(nullptr, storage.get());

  test::Array2D<uint8_t> src{8, 8};
  ASSERT_TRUE(src.valid());
  test::PseudoRandomNumberGenerator<uint8_t> generator;
  src.fill(generator);

  auto blur_fail = [](const uint8_t*, size_t, size_t, size_t, uint8_t*, size_t,
                      size_t, kleidicv_border_type_t) {
    return KLEIDICV_ERROR_NOT_IMPLEMENTED;
  };
  auto scharr_ok = [](const uint8_t*, size_t, size_t, size_t, size_t, int16_t*,
                      size_t) { return KLEIDICV_OK; };

  EXPECT_EQ(KLEIDICV_ERROR_NOT_IMPLEMENTED,
            storage->process(src.data(), src.stride(), blur_fail, scharr_ok));
}

TEST(OpticalFlowLKPyramidInternal, ProcessPropagatesScharrErrorLaterLevel) {
  using KLEIDICV_TARGET_NAMESPACE::OpticalFlowLKPyramid;
  OpticalFlowLKPyramid::Pointer storage;
  ASSERT_EQ(KLEIDICV_OK, OpticalFlowLKPyramid::create(storage, 2, 8, 8, 1));
  ASSERT_NE(nullptr, storage.get());

  test::Array2D<uint8_t> src{8, 8};
  ASSERT_TRUE(src.valid());
  test::PseudoRandomNumberGenerator<uint8_t> generator;
  src.fill(generator);

  auto blur_ok = [](const uint8_t*, size_t, size_t, size_t, uint8_t*, size_t,
                    size_t, kleidicv_border_type_t) { return KLEIDICV_OK; };
  size_t scharr_calls = 0;
  auto scharr_fail_on_second = [&scharr_calls](const uint8_t*, size_t, size_t,
                                               size_t, size_t, int16_t*,
                                               size_t) {
    ++scharr_calls;
    return scharr_calls == 2 ? KLEIDICV_ERROR_RANGE : KLEIDICV_OK;
  };

  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            storage->process(src.data(), src.stride(), blur_ok,
                             scharr_fail_on_second));
}

#ifdef KLEIDICV_ALLOCATION_TESTS
// Allocation failure path coverage for builder API.
TEST(OpticalFlowPyrLKPyramidBuild, Allocation) {
  test::Array2D<uint8_t> src{3, 3};
  ASSERT_TRUE(src.valid());
  kleidicv_optical_flow_pyr_lk_pyramid_t* pyramid = nullptr;

  MockMallocToFail::enable();
  const auto ret = kleidicv_build_optical_flow_pyr_lk_pyramid(
      &pyramid, src.data(), src.stride(), 3, 3, 1, 1, 3, 3);
  MockMallocToFail::disable();

  EXPECT_EQ(KLEIDICV_ERROR_ALLOCATION, ret);
  EXPECT_EQ(nullptr, pyramid);
}
#endif
