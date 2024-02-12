// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"

class GrayTest final {
 public:
  explicit GrayTest(bool hasAlpha)
      : hasAlpha_(hasAlpha), outChannels_{hasAlpha ? 4U : 3U} {}

  template <typename F>
  void execute_test(F impl) {
    // Set width to be more than vector length, but not quite a multiple of
    // vector length to force both vector and scalar paths
    size_t logical_width = 3 * test::Options::vector_lanes<uint8_t>() - 1;
    test::Array2D<uint8_t> source{logical_width, 3};
    test::Array2D<uint8_t> actual{logical_width * outChannels_, 3};
    test::Array2D<uint8_t> expected{logical_width * outChannels_, 3};

    source.set(0, 0, {1});
    source.set(1, 0, {0xFF});
    source.set(2, 0, {10, 15, 1});

    calculate_expected(source, expected);

    auto err = impl(source.data(), source.stride(), actual.data(),
                    actual.stride(), logical_width, actual.height());

    ASSERT_EQ(INTRINSICCV_OK, err);
    EXPECT_EQ_ARRAY2D(actual, expected);

    test::test_null_args(impl, source.data(), source.stride(), actual.data(),
                         actual.stride(), logical_width, actual.height());
  }

 private:
  void calculate_expected(test::Array2D<uint8_t> &src,
                          test::Array2D<uint8_t> &expected) const {
    for (size_t vindex = 0; vindex < expected.height(); vindex++) {
      for (size_t hindex = 0; hindex < expected.width() / outChannels_;
           hindex++) {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        uint8_t y = *src.at(vindex, hindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)

        if (hasAlpha_) {
          expected.set(vindex, hindex * outChannels_, {y, y, y, 0xff});
        } else {
          expected.set(vindex, hindex * outChannels_, {y, y, y});
        }
      }
    }
  }

  bool hasAlpha_;
  size_t outChannels_;
};

class ColourTest final {
 public:
  ColourTest(size_t src_channels, size_t dst_channels, bool swapBlue)
      : inChannels_(src_channels),
        outChannels_(dst_channels),
        swapBlue_(swapBlue) {}

  template <typename F>
  void execute_test(F impl) {
    // Set width to be more than vector length, but not quite a multiple of
    // vector length to force both vector and scalar paths
    size_t logical_width = 3 * test::Options::vector_lanes<uint8_t>() - 1;
    test::Array2D<uint8_t> source{logical_width * inChannels_, 3};
    test::Array2D<uint8_t> actual{logical_width * outChannels_, 3};
    test::Array2D<uint8_t> expected{logical_width * outChannels_, 3};

    source.set(0, 0, {123, 230, 11, 203});
    source.set(1, 0, {0xFF, 0xFF, 0xFF, 0});
    source.set(2, 0, {0, 0, 0, 0xFF});

    calculate_expected(source, expected);

    auto err = impl(source.data(), source.stride(), actual.data(),
                    actual.stride(), logical_width, actual.height());

    ASSERT_EQ(INTRINSICCV_OK, err);
    EXPECT_EQ_ARRAY2D(actual, expected);

    test::test_null_args(impl, source.data(), source.stride(), actual.data(),
                         actual.stride(), logical_width, actual.height());
  }

 private:
  void calculate_expected(test::Array2D<uint8_t> &src,
                          test::Array2D<uint8_t> &expected) const {
    for (size_t vindex = 0; vindex < expected.height(); vindex++) {
      for (size_t hindex = 0; hindex < expected.width() / outChannels_;
           hindex++) {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        uint8_t r = *src.at(vindex, hindex * inChannels_);
        uint8_t g = *src.at(vindex, hindex * inChannels_ + 1);
        uint8_t b = *src.at(vindex, hindex * inChannels_ + 2);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
        uint8_t a = 0xff;
        if (inChannels_ == 4) {
          a = *src.at(vindex, hindex * inChannels_ + 3);
        }

        if (outChannels_ == 4) {
          if (swapBlue_) {
            expected.set(vindex, hindex * outChannels_, {b, g, r, a});
          } else {
            expected.set(vindex, hindex * outChannels_, {r, g, b, a});
          }
        } else {
          if (swapBlue_) {
            expected.set(vindex, hindex * outChannels_, {b, g, r});
          } else {
            expected.set(vindex, hindex * outChannels_, {r, g, b});
          }
        }
      }
    }
  }

  size_t inChannels_;
  size_t outChannels_;
  bool swapBlue_;
};

TEST(GRAY2, RGB) {
  GrayTest gray_test(false);
  gray_test.execute_test(intrinsiccv_gray_to_rgb_u8);
}

TEST(GRAY2, RGBA) {
  GrayTest gray_test(true);
  gray_test.execute_test(intrinsiccv_gray_to_rgba_u8);
}

TEST(RGB2, RGB) {
  ColourTest colour_test(3, 3, false);
  colour_test.execute_test(intrinsiccv_rgb_to_rgb_u8);
}

TEST(RGBA2, RGBA) {
  ColourTest colour_test(4, 4, false);
  colour_test.execute_test(intrinsiccv_rgba_to_rgba_u8);
}

TEST(RGB2, BGR) {
  ColourTest colour_test(3, 3, true);
  colour_test.execute_test(intrinsiccv_rgb_to_bgr_u8);
}

TEST(RGBA2, BGRA) {
  ColourTest colour_test(4, 4, true);
  colour_test.execute_test(intrinsiccv_rgba_to_bgra_u8);
}

TEST(RGB2, BGRA) {
  ColourTest colour_test(3, 4, true);
  colour_test.execute_test(intrinsiccv_rgb_to_bgra_u8);
}

TEST(RGB2, RGBA) {
  ColourTest colour_test(3, 4, false);
  colour_test.execute_test(intrinsiccv_rgb_to_rgba_u8);
}

TEST(RGBA2, BGR) {
  ColourTest colour_test(4, 3, true);
  colour_test.execute_test(intrinsiccv_rgba_to_bgr_u8);
}

TEST(RGBA2, RGB) {
  ColourTest colour_test(4, 3, false);
  colour_test.execute_test(intrinsiccv_rgba_to_rgb_u8);
}
