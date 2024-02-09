// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/array.h"
#include "framework/generator.h"
#include "framework/operation.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_SCALE(type, suffix) \
  INTRINSICCV_API(scale, intrinsiccv_scale_##suffix, type)

INTRINSICCV_SCALE(uint8_t, u8);

template <typename ElementType>
class ScaleTestBase : public UnaryOperationTest<ElementType> {
  using UnaryOperationTest<ElementType>::min;
  using UnaryOperationTest<ElementType>::max;

  // Calls the API-under-test in the appropriate way.
  intrinsiccv_error_t call_api() override {
    return intrinsiccv_scale_u8(
        this->inputs_[0].data(), this->inputs_[0].stride(),
        this->actual_[0].data(), this->actual_[0].stride(), this->width(),
        this->height(), this->scale(), this->shift());
  }
  virtual float scale() = 0;
  virtual float shift() = 0;

  // Prepares expected outputs for the operation.
  void setup() override {
    ElementType expected = 0;
    if (shift() < min()) {
      expected = min();
    } else if (shift() > max()) {
      expected = max();
    } else {
      expected = lrintf(shift());
    }
    this->expected_[0].fill(expected);
    UnaryOperationTest<ElementType>::setup();
  }
};  // end of class ScaleTestBase<ElementType>

template <typename ElementType>
class ScaleTestLinearBase {
  static constexpr ElementType min() {
    return std::numeric_limits<ElementType>::min();
  }
  static constexpr ElementType max() {
    return std::numeric_limits<ElementType>::max();
  }
  virtual float scale() = 0;
  virtual float shift() = 0;

 public:
  void test_scalar() {
    size_t width = test::Options::vector_length() - 1;
    test_linear(width);
  }
  void test_vector() {
    size_t width = test::Options::vector_length() * 2;
    test_linear(width);
  }

 private:
  class GenerateLinearSeries : public test::Generator<ElementType> {
   public:
    explicit GenerateLinearSeries(ElementType start_from)
        : counter_{start_from} {}

    std::optional<ElementType> next() override { return counter_++; }

   private:
    ElementType counter_;
  };  // end of class GenerateLinearSeries

  void test_linear(size_t width) {
    size_t height = ((std::numeric_limits<ElementType>::max() -
                      std::numeric_limits<ElementType>::min()) /
                     width) +
                    1;
    test::Array2D<ElementType> source(width, height, 1, 1);
    test::Array2D<ElementType> expected(width, height, 1, 1);
    test::Array2D<ElementType> actual =
        test::Array2D<ElementType>(width, height, 1, 1);

    GenerateLinearSeries generator(std::numeric_limits<ElementType>::min());

    source.fill(&generator);

    calculate_expected(source, expected);

    ASSERT_EQ(
        INTRINSICCV_OK,
        intrinsiccv_scale_u8(source.data(), source.stride(), actual.data(),
                             actual.stride(), width, height, scale(), shift()));

    EXPECT_EQ_ARRAY2D(expected, actual);
  }

 protected:
  void calculate_expected(const test::Array2D<ElementType>& source,
                          test::Array2D<ElementType>& expected) {
    for (size_t hindex = 0; hindex < source.height(); ++hindex) {
      for (size_t vindex = 0; vindex < source.width(); ++vindex) {
        ElementType calculated = 0;
        // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
        float result = *source.at(hindex, vindex) * scale() + shift();
        // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)
        // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
        if (result > max()) {
          calculated = max();
        } else if (result < min()) {
          calculated = min();
        } else {
          calculated = lrintf(result);
        }
        *expected.at(hindex, vindex) = calculated;
        // NOLINTEND(clang-analyzer-core.uninitialized.Assign)
      }
    }
  }
};  // end of class ScaleTestLinearBase

template <typename ElementType>
class ScaleTestLinear1 final : public ScaleTestLinearBase<ElementType> {
  float scale() override { return 10; };
  float shift() override { return 13; };
};

template <typename ElementType>
class ScaleTestLinear2 final : public ScaleTestLinearBase<ElementType> {
  float scale() override { return 14.69; };
  float shift() override { return 10.13; };
};

template <typename ElementType>
class ScaleTestLinear3 final : public ScaleTestLinearBase<ElementType> {
  float scale() override { return 0.18; };
  float shift() override { return 1.41; };
};

template <typename ElementType>
class ScaleTestAdd final : public ScaleTestBase<ElementType> {
  using ArrayType = test::Array2D<ElementType>;
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  float scale() override { return 6; }
  float shift() override { return 2; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 8, 50},
      {12, 74},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestSubtract final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  float scale() override { return 8; }
  float shift() override { return -3; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 6,  45},
      { 20, 157},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestDivide final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  float scale() override { return 0.25; }
  float shift() override { return 3; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 252, 66},
      { 255, 67},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestMultiply final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;

  float scale() override { return 3.14; }
  float shift() override { return 2.72; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { 60, 191},
      { 75, 238},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestZero final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::min;
  using UnaryOperationTest<ElementType>::max;

  float scale() override { return 0; }
  float shift() override { return 0; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { min(), 0},
      {     0, 0},
      { max(), 0},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestUnderflowByShift final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::min;
  using UnaryOperationTest<ElementType>::max;

  float scale() override { return 1; }
  float shift() override { return -1; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      {min() + 1, min()},
      {    min(), min()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestOverflowByShift final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::max;

  float scale() override { return 1; }
  float shift() override { return 1; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      {max() - 1, max()},
      {    max(), max()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestUnderflowByScale final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::max;
  using UnaryOperationTest<ElementType>::min;

  float scale() override { return -2; }
  float shift() override { return 0; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { max(), min()}
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTestOverflowByScale final : public ScaleTestBase<ElementType> {
  using Elements = typename UnaryOperationTest<ElementType>::Elements;
  using UnaryOperationTest<ElementType>::max;

  float scale() override { return 2; }
  float shift() override { return 0; }

  const std::vector<Elements>& test_elements() override {
    static const std::vector<Elements> kTestElements = {
        // clang-format off
      { max(), max()},
        // clang-format on
    };
    return kTestElements;
  }
};

template <typename ElementType>
class ScaleTest : public testing::Test {};

using ElementTypes = ::testing::Types<uint8_t>;

// Tests \ref intrinsiccv_scale_u8 API.
TYPED_TEST_SUITE(ScaleTest, ElementTypes);

TYPED_TEST(ScaleTest, TestScalar1) {
  ScaleTestLinear1<TypeParam>{}.test_scalar();
}
TYPED_TEST(ScaleTest, TestVector1) {
  ScaleTestLinear1<TypeParam>{}.test_vector();
}

TYPED_TEST(ScaleTest, TestScalar2) {
  ScaleTestLinear2<TypeParam>{}.test_scalar();
}
TYPED_TEST(ScaleTest, TestVector2) {
  ScaleTestLinear2<TypeParam>{}.test_vector();
}

TYPED_TEST(ScaleTest, TestScalar3) {
  ScaleTestLinear3<TypeParam>{}.test_scalar();
}
TYPED_TEST(ScaleTest, TestVector3) {
  ScaleTestLinear3<TypeParam>{}.test_vector();
}

TYPED_TEST(ScaleTest, TestAdd) { ScaleTestAdd<TypeParam>{}.test(); }

TYPED_TEST(ScaleTest, TestSubtract) { ScaleTestSubtract<TypeParam>{}.test(); }

TYPED_TEST(ScaleTest, TestDivide) { ScaleTestDivide<TypeParam>{}.test(); }

TYPED_TEST(ScaleTest, TestMultiply) { ScaleTestMultiply<TypeParam>{}.test(); }

TYPED_TEST(ScaleTest, TestZero) { ScaleTestZero<TypeParam>{}.test(); }

TYPED_TEST(ScaleTest, TestUnderflowByShift) {
  ScaleTestUnderflowByShift<TypeParam>{}.test();
}

TYPED_TEST(ScaleTest, TestOverflowByShift) {
  ScaleTestOverflowByShift<TypeParam>{}.test();
}

TYPED_TEST(ScaleTest, TestUnderflowByScale) {
  ScaleTestUnderflowByScale<TypeParam>{}.test();
}

TYPED_TEST(ScaleTest, TestOverflowByScale) {
  ScaleTestOverflowByScale<TypeParam>{}.test();
}

TYPED_TEST(ScaleTest, NullPointer) {
  TypeParam src[1] = {}, dst[1];
  test::test_null_args(scale<TypeParam>(), src, sizeof(TypeParam), dst,
                       sizeof(TypeParam), 1, 1, 2, 0);
}

TYPED_TEST(ScaleTest, Misalignment) {
  if (sizeof(TypeParam) == 1) {
    // misalignment impossible
    return;
  }
  TypeParam src[1] = {}, dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            scale<TypeParam>()(src, sizeof(TypeParam) + 1, dst,
                               sizeof(TypeParam), 1, 1, 2, 0));
  EXPECT_EQ(INTRINSICCV_ERROR_ALIGNMENT,
            scale<TypeParam>()(src, sizeof(TypeParam), dst,
                               sizeof(TypeParam) + 1, 1, 1, 2, 0));
}

TYPED_TEST(ScaleTest, ImageSize) {
  TypeParam src[1], dst[1];
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            scale<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS + 1, 1, 2, 0));
  EXPECT_EQ(INTRINSICCV_ERROR_RANGE,
            scale<TypeParam>()(src, sizeof(TypeParam), dst, sizeof(TypeParam),
                               INTRINSICCV_MAX_IMAGE_PIXELS,
                               INTRINSICCV_MAX_IMAGE_PIXELS, 2, 0));
}
