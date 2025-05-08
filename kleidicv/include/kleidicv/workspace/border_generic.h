// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_GENERIC_H
#define KLEIDICV_WORKSPACE_BORDER_GENERIC_H

#include "border_types.h"
#include "kleidicv/kleidicv.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Generic border offsets for any size filters.
template <kleidicv_border_type_t BorderType>
class GenericBorderInfo {
 public:
  GenericBorderInfo(size_t width) : width_(width), margin_(width / 2) {}

  // Returns offset without the influence of any border.
  ptrdiff_t offset_without_border(size_t index) const {
    return static_cast<ptrdiff_t>(index) - margin_;
  }

  // Returns offset for columns affected by left border.
  ptrdiff_t offset_with_left_border(size_t base_index, size_t index) const
      KLEIDICV_STREAMING_COMPATIBLE {
    // TODO this is the REPLICATE case, implement all cases
    return static_cast<ptrdiff_t>(base_index)
  }
  // Unreachable. Compiler should emit a warning-as-error if any cases are
  // uncovered above.
  return Offsets{};  // GCOVR_EXCL_LINE
}

// Returns offsets for columns affected by right border.
Offsets
offsets_with_right_border(size_t /* column_index */) const
    KLEIDICV_STREAMING_COMPATIBLE {
  switch (border_type_) {
    case FixedBorderType::REPLICATE:
    case FixedBorderType::REFLECT:
      return get(-1, 0, 0);
      break;

    case FixedBorderType::WRAP:
      return get(-1, 0, 1 - width_);
      break;

    case FixedBorderType::REVERSE:
      return get(-1, 0, -1);
      break;
  }
  // Unreachable. Compiler should emit a warning-as-error if any cases are
  // uncovered above.
  return Offsets{};  // GCOVR_EXCL_LINE
}

// Returns offsets for rows or columns affected by any border.
Offsets offsets_with_border(size_t row_or_column_index) const
    KLEIDICV_STREAMING_COMPATIBLE {
  if (row_or_column_index == 0U) {
    // Rows and columns have the same offsets.
    return offsets_with_left_border(row_or_column_index);
  }
  if (row_or_column_index == (width_ - 1U)) {
    // Rows and columns have the same offsets.
    return offsets_with_right_border(row_or_column_index);
  }
  return offsets_without_border();
}

private:
// Takes care of static signed to unsigned casts.
Offsets get(ptrdiff_t o0, ptrdiff_t o1, ptrdiff_t o2) const {
  return Offsets{o0, o1, o2};
}

size_t width_;
FixedBorderType border_type_;
};  // end of class FixedBorderInfo<T, 3UL>

// Shorthand for 3x3 filter border type.
template <typename T>
using FixedBorderInfo3x3 = FixedBorderInfo<T, 3UL>;

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_BORDER_GENERIC_H
