// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_TEST_FRAMEWORK_BORDER_H_
#define INTRINSICCV_TEST_FRAMEWORK_BORDER_H_

#include <intrinsiccv.h>

#include "framework/abstract.h"

namespace test {

// Prepares bordering elements given a border type and bordering requirements.
template <typename ElementType>
void prepare_borders(intrinsiccv_border_type_t border_type,
                     const Bordered *bordered,
                     TwoDimensional<ElementType> *elements);

}  // namespace test

#endif  // INTRINSICCV_TEST_FRAMEWORK_BORDER_H_
