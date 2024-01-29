// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_WORKSPACE_BORDERS_H
#define INTRINSICCV_WORKSPACE_BORDERS_H

#include "intrinsiccv.h"

namespace intrinsiccv {

// Border offsets for fixed-size filters.
template <typename T, const size_t S>
class FixedBorderInfo;

// Border offsets for 3x3 filters.
template <typename T>
class FixedBorderInfo<T, 3UL> final {
 public:
  // Simple object holding read-only constant offsets.
  class Offsets final {
   public:
    Offsets() = default;

    Offsets(size_t o0, size_t o1, size_t o2) : offsets_{o0, o1, o2} {}

    size_t c0() const { return offsets_[0]; }
    size_t c1() const { return offsets_[1]; }
    size_t c2() const { return offsets_[2]; }

   private:
    size_t offsets_[3];
  };

  FixedBorderInfo(size_t height, intrinsiccv_border_type_t border_type)
      : height_(height), border_type_(border_type) {}

  // Retuns offsets without the influence of any border.
  Offsets offsets_without_border() const { return get(-1, 0, 1); }

  // Retuns offsets for columns affected by left border.
  Offsets offsets_with_left_border(size_t /* column_index */) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    switch (border_type_) {
      case INTRINSICCV_BORDER_TYPE_REPLICATE:
      case INTRINSICCV_BORDER_TYPE_REFLECT:
        return get(0, 0, 1);
        break;

      case INTRINSICCV_BORDER_TYPE_WRAP:
        return get(height_ - 1, 0, 1);
        break;

      case INTRINSICCV_BORDER_TYPE_REVERSE:
        return get(1, 0, 1);
        break;

      default:
        // There is no good value to return here.
        return Offsets{};
    }
  }

  // Retuns offsets for columns affected by right border.
  Offsets offsets_with_right_border(size_t /* column_index */) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    switch (border_type_) {
      case INTRINSICCV_BORDER_TYPE_REPLICATE:
      case INTRINSICCV_BORDER_TYPE_REFLECT:
        return get(-1, 0, 0);
        break;

      case INTRINSICCV_BORDER_TYPE_WRAP:
        return get(-1, 0, 1 - height_);
        break;

      case INTRINSICCV_BORDER_TYPE_REVERSE:
        return get(-1, 0, -1);
        break;

      default:
        // There is no good value to return here.
        return Offsets{};
    }
  }

  // Retuns offsets for rows or columns affected by any border.
  Offsets offsets_with_border(size_t row_or_column_index) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (row_or_column_index == 0U) {
      // Rows and columns have the same offsets.
      return offsets_with_left_border(row_or_column_index);
    }
    if (row_or_column_index == (height_ - 1U)) {
      // Rows and columns have the same offsets.
      return offsets_with_right_border(row_or_column_index);
    }
    return offsets_without_border();
  }

 private:
  // Takes care of static signed to unsigned casts.
  Offsets get(size_t o0, size_t o1, size_t o2) const {
    return Offsets{o0, o1, o2};
  }

  size_t height_;
  intrinsiccv_border_type_t border_type_;
};  // end of class FixedBorderInfo<T, 3UL>

// Border offsets for 5x5 filters.
template <typename T>
class FixedBorderInfo<T, 5UL> final {
 public:
  // Simple object holding read-only constant offsets.
  class Offsets final {
   public:
    // NOLINTBEGIN(hicpp-member-init)
    Offsets() = default;
    // NOLINTEND(hicpp-member-init)

    Offsets(size_t o0, size_t o1, size_t o2, size_t o3, size_t o4)
        : offsets_{o0, o1, o2, o3, o4} {}

    size_t c0() const { return offsets_[0]; }
    size_t c1() const { return offsets_[1]; }
    size_t c2() const { return offsets_[2]; }
    size_t c3() const { return offsets_[3]; }
    size_t c4() const { return offsets_[4]; }

   private:
    size_t offsets_[5];
  };

  FixedBorderInfo(size_t height, intrinsiccv_border_type_t border_type)
      : height_(height), border_type_(border_type) {}

  // Retuns offsets without the influence of any border.
  Offsets offsets_without_border() const INTRINSICCV_STREAMING_COMPATIBLE {
    return get(-2, -1, 0, 1, 2);
  }

  // Retuns offsets for columns affected by left border.
  Offsets offsets_with_left_border(size_t column_index) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    switch (border_type_) {
      case INTRINSICCV_BORDER_TYPE_REPLICATE:
        if (column_index == 0) {
          return get(0, 0, 0, 1, 2);
        } else {
          return get(-1, -1, 0, 1, 2);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_REFLECT:
        if (column_index == 0) {
          return get(1, 0, 0, 1, 2);
        } else {
          return get(-1, -1, 0, 1, 2);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_WRAP:
        if (column_index == 0) {
          return get(height_ - 2, height_ - 1, 0, 1, 2);
        } else {
          return get(height_ - 2, -1, 0, 1, 2);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_REVERSE:
        if (column_index == 0) {
          return get(2, 1, 0, 1, 2);
        } else {
          return get(0, -1, 0, 1, 2);
        }
        break;

      default:
        // There is no good value to return here.
        return Offsets{};
    }
  }

  // Retuns offsets for columns affected by right border.
  Offsets offsets_with_right_border(size_t column_index) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    switch (border_type_) {
      case INTRINSICCV_BORDER_TYPE_REPLICATE:
        if (column_index == (height_ - 2)) {
          return get(-2, -1, 0, 1, 1);
        } else {
          return get(-2, -1, 0, 0, 0);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_REFLECT:
        if (column_index == (height_ - 2)) {
          return get(-2, -1, 0, 1, 1);
        } else {
          return get(-2, -1, 0, 0, -1);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_WRAP:
        if (column_index == (height_ - 2)) {
          return get(-2, -1, 0, 1, 2 - height_);
        } else {
          return get(-2, -1, 0, 1 - height_, 2 - height_);
        }
        break;

      case INTRINSICCV_BORDER_TYPE_REVERSE:
        if (column_index == (height_ - 2)) {
          return get(-2, -1, 0, 1, 0);
        } else {
          return get(-2, -1, 0, -1, -2);
        }
        break;

      default:
        // There is no good value to return here.
        return Offsets{};
    }
  }

  // Retuns offsets for rows or columns affected by any border.
  Offsets offsets_with_border(size_t row_or_column_index) const
      INTRINSICCV_STREAMING_COMPATIBLE {
    if (row_or_column_index <= 1U) {
      // Rows and columns have the same offsets.
      return offsets_with_left_border(row_or_column_index);
    }
    if (row_or_column_index >= (height_ - 2U)) {
      // Rows and columns have the same offsets.
      return offsets_with_right_border(row_or_column_index);
    }
    return offsets_without_border();
  }

 private:
  // Takes care of static signed to unsigned casts.
  Offsets get(size_t o0, size_t o1, size_t o2, size_t o3,
              size_t o4) const INTRINSICCV_STREAMING_COMPATIBLE {
    return Offsets{o0, o1, o2, o3, o4};
  }

  size_t height_;
  intrinsiccv_border_type_t border_type_;
};  // end of class FixedBorderInfo<T, 5UL>

// Shorthand for 3x3 filter border type.
template <typename T>
using FixedBorderInfo3x3 = FixedBorderInfo<T, 3UL>;

// Shorthand for 5x5 filter border type.
template <typename T>
using FixedBorderInfo5x5 = FixedBorderInfo<T, 5UL>;

}  // namespace intrinsiccv

#endif  // INTRINSICCV_WORKSPACE_BORDERS_H
