// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "kleidicv/types.h"

namespace {

struct TestRowBase : kleidicv::neon::RowBase<uint32_t> {
  using RowBase::add_stride;
  using RowBase::subtract_stride;
};

TEST(RowBase, SignedByteOffsets) {
  uint32_t storage[16] = {};
  uint32_t *middle = &storage[8];
  const uint32_t *const_middle = middle;
  for (ptrdiff_t offset : {-4, 0, 4}) {
    const ptrdiff_t bytes = offset * static_cast<ptrdiff_t>(sizeof(uint32_t));
    EXPECT_EQ(middle + offset, TestRowBase::add_stride(middle, bytes));
    EXPECT_EQ(middle - offset, TestRowBase::subtract_stride(middle, bytes));
    EXPECT_EQ(const_middle + offset,
              TestRowBase::add_stride(const_middle, bytes));
    EXPECT_EQ(const_middle - offset,
              TestRowBase::subtract_stride(const_middle, bytes));
  }
}

TEST(Rows, BackwardNavigation) {
  uint32_t storage[24] = {};
  kleidicv::neon::Rows<uint32_t> rows{storage, 4 * sizeof(uint32_t), 2};
  rows += 3;
  EXPECT_EQ(&storage[12], &rows[0]);
  EXPECT_EQ(&storage[6], &rows.at(-2, 1)[0]);
  rows += -3;
  EXPECT_EQ(storage, &rows[0]);
}

TEST(ParallelRows, BackwardNavigation) {
  uint32_t storage[24] = {};
  kleidicv::neon::ParallelRows<uint32_t> rows{storage, 4 * sizeof(uint32_t)};
  rows += 2;
  EXPECT_EQ(&storage[16], &rows.as_columns().first()[0]);
  EXPECT_EQ(&storage[20], &rows.as_columns().second()[0]);
  rows += -2;
  EXPECT_EQ(storage, &rows.as_columns().first()[0]);
  EXPECT_EQ(&storage[4], &rows.as_columns().second()[0]);
}

}  // namespace
