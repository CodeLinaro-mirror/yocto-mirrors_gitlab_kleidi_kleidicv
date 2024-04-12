// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/operation.h"
#include "framework/utils.h"
#include "intrinsiccv/intrinsiccv.h"
#include "test_config.h"

#define KLEIDICV_float_conversion(I, input_type_name, O, output_type_name)    \
  KLEIDICV_DIFF_IO_API(                                                       \
      float_conversion,                                                       \
      intrinsiccv_float_conversion_##input_type_name##_##output_type_name, I, \
      O)

KLEIDICV_float_conversion(float, f32, int8_t, s8);
KLEIDICV_float_conversion(float, f32, uint8_t, u8);
KLEIDICV_float_conversion(int8_t, s8, float, f32);
KLEIDICV_float_conversion(uint8_t, u8, float, f32);

template <typename InputType, typename OutputType>
class FloatConversionTest final {
 private:
  template <typename T>
  static constexpr T min() {
    return std::numeric_limits<T>::min();
  }

  template <typename T>
  static constexpr T max() {
    return std::numeric_limits<T>::max();
  }

  struct Elements {
    size_t width;
    size_t height;

    std::vector<std::vector<InputType>> source_rows;
    std::vector<std::vector<OutputType>> expected_rows;

    Elements(size_t _width, size_t _height,
             std::vector<std::vector<InputType>>&& _source_rows,
             std::vector<std::vector<OutputType>>&& _expected_rows)
        : width(_width),
          height(_height),
          source_rows(std::move(_source_rows)),
          expected_rows(std::move(_expected_rows)) {}
  };

  struct Values {
    InputType source;
    OutputType expected;
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
    KLEIDICV_NO_STRICT_ALIASING_BEGIN
    return *reinterpret_cast<float*>(&v);
    KLEIDICV_NO_STRICT_ALIASING_END
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true,
            std::enable_if_t<std::is_same_v<int8_t, O>, bool> = true>
  const Elements& get_custom_elements() {
    static const Elements kTestElements = {
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
    return kTestElements;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true,
            std::enable_if_t<std::is_same_v<uint8_t, O>, bool> = true>
  const Elements& get_custom_elements() {
    static const Elements kTestElements = {
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
    return kTestElements;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, O>, bool> = true>
  const Elements& get_custom_elements() {
    static const Elements kTestElements = {
        // clang-format off
      4, 6,
      {{
        { min<I>(), min<I>(), max<I>() - 1, max<I>() },
        { min<I>(), min<I>(), min<I>(), min<I>() },
        { min<I>(), min<I>(), min<I>(), max<I>() - 1 },
        { max<I>() - 1, max<I>(), 113, 114 },
        { 112, 113, 114, 115 },
        { 12, 12, 12, 12 }
      }},
      {{
        { min<I>(), min<I>(), max<I>() - 1.0, max<I>() },
        { min<I>(), min<I>(), min<I>(), min<I>() },
        { min<I>(), min<I>(), min<I>(), max<I>() - 1.0 },
        { max<I>() - 1.0, max<I>(), 113.0, 114.0 },
        { 112.0, 113.0, 114.0, 115.0 },
        { 12.0, 12.0, 12.0, 12.0 }
      }}
        // clang-format on
    };
    return kTestElements;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true>
  const Values& get_values() {
    static const Values kTestValues = {
        // clang-format off
        10.67F, 11
        // clang-format on
    };
    return kTestValues;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, O>, bool> = true>
  const Values& get_values() {
    static const Values kTestValues = {
        // clang-format off
        11, 11.0
        // clang-format on
    };
    return kTestValues;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true,
            std::enable_if_t<std::is_integral_v<O>, bool> = true>
  void calculate_expected(const test::Array2D<I>& source,
                          test::Array2D<O>& expected) {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        O calculated = 0;
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        I result = *source.at(hindex, vindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
        if (result > max<O>()) {
          calculated = max<O>();
        } else if (result < min<O>()) {
          calculated = min<O>();
        } else {
          calculated = result;
        }
        *expected.at(hindex, vindex) = calculated;
      }
    }
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_integral_v<I>, bool> = true,
            std::enable_if_t<std::is_same_v<float, O>, bool> = true>
  void calculate_expected(const test::Array2D<I>& source,
                          test::Array2D<OutputType>& expected) {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        *expected.at(hindex, vindex) = *source.at(hindex, vindex);
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
      }
    }
  }

  template <typename T>
  size_t get_linear_height(size_t width, size_t minimum_size) {
    size_t image_size =
        std::max(minimum_size, static_cast<size_t>(max<T>() - min<T>()));
    size_t height = image_size / width + 1;

    return height;
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true,
            std::enable_if_t<std::is_integral_v<O>, bool> = true>
  std::tuple<test::Array2D<I>, test::Array2D<O>, test::Array2D<O>>
  get_linear_arrays(size_t width, size_t height) {
    test::Array2D<I> source(width, height, 1, 1);
    test::Array2D<O> expected(width, height, 1, 1);
    test::Array2D<O> actual(width, height, 1, 1);

    test::GenerateLinearSeries<I> generator(min<O>());

    source.fill(generator);

    calculate_expected<I, O>(source, expected);

    return {source, expected, actual};
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_integral_v<I>, bool> = true,
            std::enable_if_t<std::is_same_v<float, O>, bool> = true>
  std::tuple<test::Array2D<I>, test::Array2D<O>, test::Array2D<O>>
  get_linear_arrays(size_t width, size_t height) {
    test::Array2D<I> source(width, height, 1, 1);
    test::Array2D<O> expected(width, height, 1, 1);
    test::Array2D<O> actual(width, height, 1, 1);

    test::GenerateLinearSeries<I> generator(min<I>());

    source.fill(generator);

    calculate_expected<I, O>(source, expected);

    return {source, expected, actual};
  }

 public:
  // minimum_size set by caller to trigger the 'big' conversion path.
  template <typename I, typename O,
            std::enable_if_t<std::is_same_v<float, I>, bool> = true,
            std::enable_if_t<std::is_integral_v<O>, bool> = true>
  void test_linear(size_t width, size_t minimum_size = 1) {
    size_t height = get_linear_height<O>(width, minimum_size);

    auto arrays = get_linear_arrays<I, O>(width, height);

    test::Array2D<I>& source = std::get<0>(arrays);
    test::Array2D<O>& expected = std::get<1>(arrays);
    test::Array2D<O>& actual = std::get<2>(arrays);

    ASSERT_EQ(KLEIDICV_OK, (float_conversion<I, O>()(
                               source.data(), source.stride(), actual.data(),
                               actual.stride(), width, height)));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  template <typename I, typename O,
            std::enable_if_t<std::is_integral_v<I>, bool> = true,
            std::enable_if_t<std::is_same_v<float, O>, bool> = true>
  void test_linear(size_t width, size_t minimum_size = 1) {
    size_t height = get_linear_height<I>(width, minimum_size);

    auto arrays = get_linear_arrays<I, O>(width, height);

    test::Array2D<I>& source = std::get<0>(arrays);
    test::Array2D<O>& expected = std::get<1>(arrays);
    test::Array2D<O>& actual = std::get<2>(arrays);

    ASSERT_EQ(KLEIDICV_OK, (float_conversion<I, O>()(
                               source.data(), source.stride(), actual.data(),
                               actual.stride(), width, height)));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  void test_custom() {
    auto elements_list = get_custom_elements<InputType, OutputType>();
    const size_t& width = elements_list.width;
    const size_t& height = elements_list.height;

    test::Array2D<InputType> source(width, height);
    test::Array2D<OutputType> expected(width, height);
    test::Array2D<OutputType> actual(width, height);

    for (size_t i = 0; i < height; i++) {
      source.set(i, 0, elements_list.source_rows[i]);
      expected.set(i, 0, elements_list.expected_rows[i]);
    }

    ASSERT_EQ(KLEIDICV_OK, (float_conversion<InputType, OutputType>()(
                               source.data(), source.stride(), actual.data(),
                               actual.stride(), width, height)));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

  void test_sizes(const size_t width, const size_t height) {
    auto values_list = get_values<InputType, OutputType>();
    test::Array2D<InputType> source(width, height, 1, 1);

    test::Array2D<OutputType> expected(width, height, 1, 1);

    test::Array2D<OutputType> actual(width, height, 1, 1);

    source.fill(values_list.source);

    expected.fill(values_list.expected);

    actual.fill(0);

    ASSERT_EQ(KLEIDICV_OK, (float_conversion<InputType, OutputType>()(
                               source.data(), source.stride(), actual.data(),
                               actual.stride(), width, height)));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }
};  // end of class FloatConversionTest

template <typename ElementType>
class FloatConversion : public testing::Test {};

using ElementTypes =
    ::testing::Types<std::pair<float, int8_t>, std::pair<float, uint8_t>,
                     std::pair<int8_t, float>, std::pair<uint8_t, float>>;

// Tests intrinsiccv_float_conversion API.
TYPED_TEST_SUITE(FloatConversion, ElementTypes);

TYPED_TEST(FloatConversion, NullPointer) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  InputType src[1] = {};
  OutputType dst[1];
  test::test_null_args(float_conversion<InputType, OutputType>(), src,
                       sizeof(InputType), dst, sizeof(OutputType), 1, 1);
}

TYPED_TEST(FloatConversion, OversizeImage) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  InputType src[1] = {};
  OutputType dst[1];
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            (float_conversion<InputType, OutputType>()(
                src, sizeof(InputType), dst, sizeof(OutputType),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, 1)));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            (float_conversion<InputType, OutputType>()(
                src, sizeof(InputType), dst, sizeof(OutputType), 1,
                KLEIDICV_MAX_IMAGE_PIXELS + 1)));
  EXPECT_EQ(KLEIDICV_ERROR_RANGE,
            (float_conversion<InputType, OutputType>()(
                src, sizeof(TypeParam), dst, sizeof(OutputType),
                KLEIDICV_MAX_IMAGE_PIXELS + 1, KLEIDICV_MAX_IMAGE_PIXELS + 1)));
}

TYPED_TEST(FloatConversion, Scalar) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}
      .template test_linear<InputType, OutputType>(
          test::Options::vector_length() - 1);
}
TYPED_TEST(FloatConversion, Vector) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}
      .template test_linear<InputType, OutputType>(
          test::Options::vector_length() * 2);
}
TYPED_TEST(FloatConversion, Custom) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_custom();
}
TYPED_TEST(FloatConversion, CustomFits128VectorSize) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(4, 1);
}
TYPED_TEST(FloatConversion, CustomFits128VectorSize2x) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(4, 2);
}
TYPED_TEST(FloatConversion, CustomFits128VectorSize3x) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(4, 3);
}
TYPED_TEST(FloatConversion, CustomFits512VectorSize) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(4, 4);
}
TYPED_TEST(FloatConversion, CustomFits512VectorSize2x) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(4, 8);
}
TYPED_TEST(FloatConversion, CustomFits512VectorSize3x) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(6, 8);
}
TYPED_TEST(FloatConversion, Custom128OneRemaining) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(1, 17);
}
TYPED_TEST(FloatConversion, Custom128AllButOneRemaining) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(5, 3);
}
TYPED_TEST(FloatConversion, CustomAboutHalfRemaining) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(19, 2);
}
TYPED_TEST(FloatConversion, CustomEmpty) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(0, 0);
}
TYPED_TEST(FloatConversion, CustomOne) {
  using InputType = typename TypeParam::first_type;
  using OutputType = typename TypeParam::second_type;
  FloatConversionTest<InputType, OutputType>{}.test_sizes(1, 1);
}
