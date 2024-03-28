// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/operation.h"
#include "framework/special_floats.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"
#include "test_config.h"

template <typename ElementType>
class FloatToIntTestBase {
 private:
  template <typename T>
  static constexpr T min() {
    return std::numeric_limits<T>::min();
  }

  template <typename T>
  static constexpr T max() {
    return std::numeric_limits<T>::max();
  }

  template <typename OutputType>
  struct Elements {
    size_t width;
    size_t height;

    std::vector<std::vector<ElementType>> source_rows;
    std::vector<std::vector<OutputType>> expected_rows;

    Elements(size_t _width, size_t _height,
             std::vector<std::vector<ElementType>>&& _source_rows,
             std::vector<std::vector<OutputType>>&& _expected_rows)
        : width(_width),
          height(_height),
          source_rows(std::move(_source_rows)),
          expected_rows(std::move(_expected_rows)) {}
  };

  static constexpr uint32_t quietNaN = 0x7FC00000;
  static constexpr uint32_t signalingNaN = 0x7FA00000;
  static constexpr uint32_t posInfinity = 0x7F800000;
  static constexpr uint32_t negInfinity = 0xFF800000;

  static constexpr uint32_t minusNaN = 0xFF800001;
  static constexpr uint32_t plusNaN = 0x7F800001;
  static constexpr uint32_t plusZero = 0x00000000;
  static constexpr uint32_t minusZero = 0x80000000;

  static constexpr uint32_t oneNaN = 0x7FC00001;
  static constexpr uint32_t zeroDivZero = 0xFFC00000;
  static constexpr uint32_t floatMin = 0x00800000;
  static constexpr uint32_t floatMax = 0x7F7FFFFF;

  static constexpr uint32_t posSubnormalMin = 0x00000001;
  static constexpr uint32_t posSubnormalMax = 0x007FFFFF;
  static constexpr uint32_t negSubnormalMin = 0x80000001;
  static constexpr uint32_t negSubnormalMax = 0x807FFFFF;

  static constexpr float _floatval(uint32_t v) {
    static_assert(sizeof(float) == 4);
    INTRINSICCV_NO_STRICT_ALIASING_BEGIN
    return *reinterpret_cast<float*>(&v);
    INTRINSICCV_NO_STRICT_ALIASING_END
  }

  const Elements<int8_t> test_case_custom_f32_s8 = {
      // clang-format off
    4, 8,
    {{
      { _floatval(quietNaN), _floatval(signalingNaN), _floatval(posInfinity), _floatval(negInfinity) },
      { _floatval(minusNaN), _floatval(plusNaN), _floatval(plusZero), _floatval(minusZero) },
      { _floatval(oneNaN), _floatval(zeroDivZero), _floatval(floatMin), _floatval(floatMax) },
      { _floatval(posSubnormalMin), _floatval(posSubnormalMax), _floatval(negSubnormalMin), _floatval(negSubnormalMax) },
      { 1111.11, -1112.22, 113.33, 114.44 },
      { 111.51, 112.62, 113.73, 114.84 },
      { 126.66, 127.11, 128.66, 129.11 },
      { 11.5, 12.5, -11.5, -12.5 }
    }},
    {{
      { 0, 0, 127, -128 },
      { 0, 0, 0, 0 },
      { 0, 0, 0, 127 },
      { 0, 0, 0, 0 },
      { 127, -128, 113, 114 },
      { 112, 113, 114, 115 },
      { 127, 127, 127, 127 },
      { 12, 12, -12, -12 }
    }}
      // clang-format on
  };

  const Elements<uint8_t> test_case_custom_f32_u8 = {
      // clang-format off
    4, 8,
    {{
      { _floatval(quietNaN), _floatval(signalingNaN), _floatval(posInfinity), _floatval(negInfinity) },
      { _floatval(minusNaN), _floatval(plusNaN), _floatval(plusZero), _floatval(minusZero) },
      { _floatval(oneNaN), _floatval(zeroDivZero), _floatval(floatMin), _floatval(floatMax) },
      { _floatval(posSubnormalMin), _floatval(posSubnormalMax), _floatval(negSubnormalMin), _floatval(negSubnormalMax) },
      { 1111.11, -1112.22, 113.33, 114.44 },
      { 111.51, 112.62, 113.73, 114.84 },
      { 126.66, 127.11, 128.66, 129.11 },
      { 11.5, 12.5, -11.5, -12.5 }
    }},
    {{
      { 0, 0, 255, 0 },
      { 0, 0, 0, 0 },
      { 0, 0, 0, 255 },
      { 0, 0, 0, 0 },
      { 255, 0, 113, 114 },
      { 112, 113, 114, 115 },
      { 127, 127, 129, 129 },
      { 12, 12, 0, 0 }
    }}
      // clang-format on
  };

  template <typename OutputType>
  void calculate_expected(const test::Array2D<ElementType>& source,
                          test::Array2D<OutputType>& expected) {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        OutputType calculated = 0;
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        ElementType result = *source.at(hindex, vindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
        if (result > max<OutputType>()) {
          calculated = max<OutputType>();
        } else if (result < min<OutputType>()) {
          calculated = min<OutputType>();
        } else {
          calculated = result;
        }
        *expected.at(hindex, vindex) = calculated;
      }
    }
  }

  class GenerateLinearSeries : public test::Generator<ElementType> {
   public:
    explicit GenerateLinearSeries(ElementType start_from)
        : counter_{start_from} {}

    std::optional<ElementType> next() override { return counter_++; }

   private:
    ElementType counter_;
  };  // end of class GenerateLinearSeries

  template <typename T>
  size_t get_linear_height(size_t width, size_t minimum_size) {
    size_t image_size =
        std::max(minimum_size, static_cast<size_t>(max<T>() - min<T>()));
    size_t height = image_size / width + 1;

    return height;
  }

  template <typename OutputType>
  std::tuple<test::Array2D<ElementType>, test::Array2D<OutputType>,
             test::Array2D<OutputType>>
  get_linear_arrays(size_t width, size_t height) {
    test::Array2D<ElementType> source(width, height, 1, 1);
    test::Array2D<OutputType> expected(width, height, 1, 1);
    test::Array2D<OutputType> actual(width, height, 1, 1);

    GenerateLinearSeries generator(min<OutputType>());

    source.fill(generator);

    calculate_expected<OutputType>(source, expected);

    return {source, expected, actual};
  }

 public:
  // minimum_size set by caller to trigger the 'big' conversion path.
  void test_linear(size_t width, size_t minimum_size = 1) {
    size_t height = get_linear_height<int8_t>(width, minimum_size);

    auto arrays_s8 = get_linear_arrays<int8_t>(width, height);

    test::Array2D<ElementType>& source_s8 = std::get<0>(arrays_s8);
    test::Array2D<int8_t>& expected_s8 = std::get<1>(arrays_s8);
    test::Array2D<int8_t>& actual_s8 = std::get<2>(arrays_s8);

    ASSERT_EQ(INTRINSICCV_OK,
              intrinsiccv_type_conversion_f32_s8(
                  source_s8.data(), source_s8.stride(), actual_s8.data(),
                  actual_s8.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected_s8, actual_s8);

    auto arrays_u8 = get_linear_arrays<uint8_t>(width, height);

    test::Array2D<ElementType>& source_u8 = std::get<0>(arrays_u8);
    test::Array2D<uint8_t>& expected_u8 = std::get<1>(arrays_u8);
    test::Array2D<uint8_t>& actual_u8 = std::get<2>(arrays_u8);

    ASSERT_EQ(INTRINSICCV_OK,
              intrinsiccv_type_conversion_f32_u8(
                  source_u8.data(), source_u8.stride(), actual_u8.data(),
                  actual_u8.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected_u8, actual_u8);
  }

  void test_custom_f32_s8() {
    const size_t& width = test_case_custom_f32_s8.width;
    const size_t& height = test_case_custom_f32_s8.height;

    test::Array2D<ElementType> source(width, height);
    test::Array2D<int8_t> expected(width, height);
    test::Array2D<int8_t> actual(width, height);

    for (size_t i = 0; i < height; i++) {
      source.set(i, 0, test_case_custom_f32_s8.source_rows[i]);
      expected.set(i, 0, test_case_custom_f32_s8.expected_rows[i]);
    }

    ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_type_conversion_f32_s8(
                                  source.data(), source.stride(), actual.data(),
                                  actual.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  void test_custom_f32_u8() {
    const size_t& width = test_case_custom_f32_u8.width;
    const size_t& height = test_case_custom_f32_u8.height;

    test::Array2D<ElementType> source(width, height);
    test::Array2D<uint8_t> expected(width, height);
    test::Array2D<uint8_t> actual(width, height);

    for (size_t i = 0; i < height; i++) {
      source.set(i, 0, test_case_custom_f32_u8.source_rows[i]);
      expected.set(i, 0, test_case_custom_f32_u8.expected_rows[i]);
    }

    ASSERT_EQ(INTRINSICCV_OK, intrinsiccv_type_conversion_f32_u8(
                                  source.data(), source.stride(), actual.data(),
                                  actual.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  void test_fill(const size_t width, const size_t height) {
    test::Array2D<ElementType> source(width, height, 1, 1);

    test::Array2D<int8_t> expected_s8(width, height, 1, 1);
    test::Array2D<uint8_t> expected_u8(width, height, 1, 1);

    test::Array2D<int8_t> actual_s8(width, height, 1, 1);
    test::Array2D<uint8_t> actual_u8(width, height, 1, 1);

    source.fill(10.67F);

    expected_s8.fill(11);
    expected_u8.fill(11);

    actual_s8.fill(0);
    actual_u8.fill(0);

    ASSERT_EQ(INTRINSICCV_OK,
              intrinsiccv_type_conversion_f32_s8(
                  source.data(), source.stride(), actual_s8.data(),
                  actual_s8.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected_s8, actual_s8);

    ASSERT_EQ(INTRINSICCV_OK,
              intrinsiccv_type_conversion_f32_u8(
                  source.data(), source.stride(), actual_u8.data(),
                  actual_u8.stride(), width, height));

    EXPECT_EQ_ARRAY2D(expected_u8, actual_u8);
  }
};  // end of class FloatToIntTestBase

template <typename TypeParam>
class FloatToIntTest : public testing::Test {};

using ElementTypes = ::testing::Types<float>;

// Tests intrinsiccv_float_to_int API.
TYPED_TEST_SUITE(FloatToIntTest, ElementTypes);

TYPED_TEST(FloatToIntTest, TestScalar) {
  FloatToIntTestBase<TypeParam>{}.test_linear(test::Options::vector_length() -
                                              1);
}
TYPED_TEST(FloatToIntTest, TestVector) {
  FloatToIntTestBase<TypeParam>{}.test_linear(test::Options::vector_length() *
                                              2);
}
TYPED_TEST(FloatToIntTest, TestCustomValuesFloat32ToInt8) {
  FloatToIntTestBase<TypeParam>{}.test_custom_f32_s8();
}
TYPED_TEST(FloatToIntTest, TestCustomValuesFloat32ToUInt8) {
  FloatToIntTestBase<TypeParam>{}.test_custom_f32_u8();
}
TYPED_TEST(FloatToIntTest, TestCustomFits128VectorSize) {
  FloatToIntTestBase<TypeParam>{}.test_fill(4, 1);
}
TYPED_TEST(FloatToIntTest, TestCustomFits128VectorSize2x) {
  FloatToIntTestBase<TypeParam>{}.test_fill(4, 2);
}
TYPED_TEST(FloatToIntTest, TestCustomFits128VectorSize3x) {
  FloatToIntTestBase<TypeParam>{}.test_fill(4, 3);
}
TYPED_TEST(FloatToIntTest, TestCustomFits512VectorSize) {
  FloatToIntTestBase<TypeParam>{}.test_fill(4, 4);
}
TYPED_TEST(FloatToIntTest, TestCustomFits512VectorSize2x) {
  FloatToIntTestBase<TypeParam>{}.test_fill(4, 8);
}
TYPED_TEST(FloatToIntTest, TestCustomFits512VectorSize3x) {
  FloatToIntTestBase<TypeParam>{}.test_fill(6, 8);
}
TYPED_TEST(FloatToIntTest, TestCustom128OneRemaining) {
  FloatToIntTestBase<TypeParam>{}.test_fill(1, 17);
}
TYPED_TEST(FloatToIntTest, TestCustom128AllButOneRemaining) {
  FloatToIntTestBase<TypeParam>{}.test_fill(5, 3);
}
TYPED_TEST(FloatToIntTest, TestCustomAboutHalfRemaining) {
  FloatToIntTestBase<TypeParam>{}.test_fill(19, 2);
}
TYPED_TEST(FloatToIntTest, TestCustomEmpty) {
  FloatToIntTestBase<TypeParam>{}.test_fill(0, 0);
}
TYPED_TEST(FloatToIntTest, TestCustomOne) {
  FloatToIntTestBase<TypeParam>{}.test_fill(1, 1);
}
