// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "framework/generator.h"

/// Tests test::PseudoRandomNumberGenerator::reset() works.
TEST(PseudoRandomNumberGenerator, Reset) {
  using ElementType = uint8_t;

  test::PseudoRandomNumberGenerator<ElementType> generator;
  ElementType initial_value = generator.next().value();
  generator.next();
  generator.reset();
  ElementType value = generator.next().value();
  EXPECT_EQ(initial_value, value);
}
