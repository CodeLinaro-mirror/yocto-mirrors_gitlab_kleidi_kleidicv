// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "framework/utils.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <ios>
#include <iostream>
#include <limits>
#include <type_traits>

namespace test {

template <typename ElementType>
void dump(const TwoDimensional<ElementType> *elements) {
  if (!elements) {
    return;
  }

  auto mask = std::numeric_limits<std::make_unsigned_t<ElementType>>::max();

  for (size_t row = 0; row < elements->height(); ++row) {
    for (size_t column = 0; column < elements->width(); ++column) {
      ElementType value = elements->at(row, column)[0];
      std::cout << std::setw(2 * sizeof(ElementType)) << std::setfill('0')
                << std::hex << (static_cast<uint64_t>(value) & mask) << " ";
    }

    std::cout << std::endl;
  }

  if ((elements->height() > 0) && (elements->width() > 0)) {
    std::cout << std::endl << std::flush;
  }
}

template void dump<int8_t>(const TwoDimensional<int8_t> *);
template void dump<uint8_t>(const TwoDimensional<uint8_t> *);
template void dump<int16_t>(const TwoDimensional<int16_t> *);
template void dump<uint16_t>(const TwoDimensional<uint16_t> *);
template void dump<int32_t>(const TwoDimensional<int32_t> *);
template void dump<uint32_t>(const TwoDimensional<uint32_t> *);
template void dump<int64_t>(const TwoDimensional<int64_t> *);
template void dump<uint64_t>(const TwoDimensional<uint64_t> *);

}  // namespace test
