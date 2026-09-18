// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include "framework/generator.h"

template <typename T>
class GenerateLinearSeriesTest : public testing::Test {};

using IntegerTypes = testing::Types<int8_t, uint8_t, int16_t, uint16_t, int32_t,
                                    uint32_t, int64_t, uint64_t>;
TYPED_TEST_SUITE(GenerateLinearSeriesTest, IntegerTypes);

TYPED_TEST(GenerateLinearSeriesTest, WrapsAtMaximum) {
  constexpr TypeParam kMaximum = std::numeric_limits<TypeParam>::max();
  constexpr TypeParam kLowest = std::numeric_limits<TypeParam>::lowest();
  test::GenerateLinearSeries<TypeParam> generator(kMaximum - 1);
  EXPECT_EQ(generator.next(), kMaximum - 1);
  EXPECT_EQ(generator.next(), kMaximum);
  EXPECT_EQ(generator.next(), kLowest);
  EXPECT_EQ(generator.next(), kLowest + 1);
}

// Tests test::PseudoRandomNumberGenerator::reset() works.
TEST(PseudoRandomNumberGenerator, Reset) {
  using ElementType = uint8_t;

  test::PseudoRandomNumberGenerator<ElementType> generator;
  ElementType initial_value = generator.next().value_or(123);
  generator.next();
  generator.reset();
  ElementType value = generator.next().value_or(234);
  EXPECT_EQ(initial_value, value);
}

// Tests test::SequenceGenerator::reset() works.
TEST(SequenceGenerator, Reset) {
  using ElementType = uint8_t;

  std::array<ElementType, 3> array{1, 2, 3};
  test::SequenceGenerator generator{array};
  EXPECT_EQ(generator.next().value_or(0), 1);
  EXPECT_EQ(generator.next().value_or(0), 2);
  EXPECT_EQ(generator.next().value_or(0), 3);
  EXPECT_EQ(generator.next(), std::nullopt);

  generator.reset();
  EXPECT_EQ(generator.next().value_or(0), 1);
}
