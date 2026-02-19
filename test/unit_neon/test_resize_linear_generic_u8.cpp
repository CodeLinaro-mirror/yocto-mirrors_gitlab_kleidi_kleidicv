// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "kleidicv/src/resize/resize_linear_generic_u8_neon.h"

using namespace kleidicv::neon::resize_linear_generic_u8;

TEST(GenericResize, rounding_div) { EXPECT_EQ(1, rounding_div(4, 5)); }
