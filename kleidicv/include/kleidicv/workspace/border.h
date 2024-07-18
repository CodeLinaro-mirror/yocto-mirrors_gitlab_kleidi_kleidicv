// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_H
#define KLEIDICV_WORKSPACE_BORDER_H

#include "border_types.h"
#include "kleidicv/kleidicv.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Border offsets for fixed-size filters.
template <typename T, const size_t KernelSize>
class FixedBorderInfo;

template <typename T, const size_t KernelSize>
class FixedBorderInfo final {
 public:
  // Simple object holding read-only constant offsets.
  // Note: We are not using the default constructor, but it is defined for the
  // unreachable cases in the code below. NOLINTNEXTLINE
  class Offsets final {
   public:
    Offsets() = default;

    template <typename... Args>
    explicit Offsets(Args... args) : offsets_{static_cast<size_t>(args)...} {
      static_assert(sizeof...(args) == KernelSize);
    }

    size_t c(int i) const { return offsets_[i]; }

   private:
    size_t offsets_[KernelSize];
  };

  FixedBorderInfo(size_t length, FixedBorderType border_type)
      : length_(length), border_type_(border_type) {}

  // Returns offsets without the influence of any border.
  Offsets offsets_without_border() const KLEIDICV_STREAMING_COMPATIBLE {
    constexpr auto seq = std::make_integer_sequence<int, (KernelSize >> 1)>{};
    return get_no_border(seq);
  }

  // Returns offsets for rows/columns affected by the top or the left border.
  Offsets offsets_with_top_or_left_border(size_t index) const
      KLEIDICV_STREAMING_COMPATIBLE {
    constexpr auto seq = std::make_integer_sequence<int, (KernelSize >> 1)>{};
    switch (border_type_) {
      case FixedBorderType::REPLICATE:
        return get_border<FixedBorderType::REPLICATE, false>(index, seq);
        break;

      case FixedBorderType::REFLECT:
        return get_border<FixedBorderType::REFLECT, false>(index, seq);
        break;

      case FixedBorderType::WRAP:
        return get_border<FixedBorderType::WRAP, false>(index, seq);
        break;

      case FixedBorderType::REVERSE:
        return get_border<FixedBorderType::REVERSE, false>(index, seq);
        break;
    }
    // Unreachable. Compiler should emit a warning-as-error if any cases are
    // uncovered above.
    return Offsets{};  // GCOVR_EXCL_LINE
  }

  // Returns offsets for rows/columns affected by the bottom or the
  // right border.
  Offsets offsets_with_bottom_or_right_border(size_t index) const
      KLEIDICV_STREAMING_COMPATIBLE {
    constexpr auto seq = std::make_integer_sequence<int, (KernelSize >> 1)>{};
    index = length_ - index - 1;
    switch (border_type_) {
      case FixedBorderType::REPLICATE:
        return get_border<FixedBorderType::REPLICATE, true>(index, seq);
        break;

      case FixedBorderType::REFLECT:
        return get_border<FixedBorderType::REFLECT, true>(index, seq);
        break;

      case FixedBorderType::WRAP:
        return get_border<FixedBorderType::WRAP, true>(index, seq);
        break;

      case FixedBorderType::REVERSE:
        return get_border<FixedBorderType::REVERSE, true>(index, seq);
        break;
    }
    // Unreachable. Compiler should emit a warning-as-error if any cases are
    // uncovered above.
    return Offsets{};  // GCOVR_EXCL_LINE
  }

  // Returns offsets for rows/columns affected by any border.
  Offsets offsets_with_border(size_t row_or_column_index) const
      KLEIDICV_STREAMING_COMPATIBLE {
    if (row_or_column_index < (KernelSize >> 1)) {
      // Rows and columns have the same offsets.
      return offsets_with_top_or_left_border(row_or_column_index);
    }
    if (row_or_column_index >= (length_ - (KernelSize >> 1))) {
      // Rows and columns have the same offsets.
      return offsets_with_bottom_or_right_border(row_or_column_index);
    }
    return offsets_without_border();
  }

 private:
  // Creates the Offsets object containing offsets in the interval
  // [-(KernelSize / 2), KernelSize / 2].
  template <int... SeqNum>
  inline Offsets get_no_border(std::integer_sequence<int, SeqNum...>) const
      KLEIDICV_STREAMING_COMPATIBLE {
    // Example (15x15): Offsets{-7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6,
    // 7};
    return Offsets{(SeqNum - (KernelSize >> 1))..., 0, (SeqNum + 1)...};
  }

  // Creates the Offsets object containing offsets in various intervals
  // depending on the row/column index, border type as well the border
  // position used. NOLINTBEGIN(readability-function-cognitive-complexity)
  template <FixedBorderType BorderType, bool IsRight, int... SeqNum>
  inline Offsets get_border(int index, std::integer_sequence<int, SeqNum...>)
      const KLEIDICV_STREAMING_COMPATIBLE {
    if constexpr (BorderType == FixedBorderType::REPLICATE && !IsRight) {
      // Example (15x15, index 4, left): Offsets{-4, -4, -4, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 5, 6, 7};
      return Offsets{(SeqNum - static_cast<int>(KernelSize >> 1) < -index)
                         ? -index
                         : (SeqNum - (KernelSize >> 1))...,
                     0, (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::REPLICATE && IsRight) {
      // Example (15x15, index 4, right): Offsets{-7, -6, -5, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 4, 4, 4};
      return Offsets{(SeqNum - (KernelSize >> 1))..., 0,
                     (SeqNum >= index) ? index : (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::REFLECT && !IsRight) {
      // Example (15x15, index 4, left): Offsets{-2, -3, -4, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 5, 6, 7};
      return Offsets{(SeqNum - static_cast<int>(KernelSize >> 1) < -index)
                         ? ((KernelSize >> 1) - (index << 1) - (SeqNum + 1))
                         : (SeqNum - (KernelSize >> 1))...,
                     0, (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::REFLECT && IsRight) {
      // Example (15x15, index 4, right): Offsets{-7, -6, -5, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 4, 3, 2};
      return Offsets{
          (SeqNum - (KernelSize >> 1))..., 0,
          (SeqNum >= index) ? ((index << 1) - SeqNum) : (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::WRAP && !IsRight) {
      // Example (15x15, index 4, left): Offsets{length_ - 7, length_ - 6,
      // length_ - 5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7};
      return Offsets{(SeqNum - static_cast<int>(KernelSize >> 1) < -index)
                         ? (SeqNum - (KernelSize >> 1) + length_)
                         : (SeqNum - (KernelSize >> 1))...,
                     0, (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::WRAP && IsRight) {
      // Example (15x15, index 4, right): Offsets{-7, -6, -5, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 5 - length_, 6 - length_, 7 - length_};
      return Offsets{
          (SeqNum - (KernelSize >> 1))..., 0,
          (SeqNum >= index) ? (SeqNum - length_ + 1) : (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::REVERSE && !IsRight) {
      // Example (15x15, index 4, left): Offsets{-1, -2, -3, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 5, 6, 7};
      return Offsets{(SeqNum - static_cast<int>(KernelSize >> 1) < -index)
                         ? ((KernelSize >> 1) - (index << 1) - SeqNum)
                         : (SeqNum - (KernelSize >> 1))...,
                     0, (SeqNum + 1)...};
    }

    if constexpr (BorderType == FixedBorderType::REVERSE && IsRight) {
      // Example (15x15, index 4, right): Offsets{-7, -6, -5, -4, -3, -2, -1, 0,
      // 1, 2, 3, 4, 3, 2, 1};
      return Offsets{
          (SeqNum - (KernelSize >> 1))..., 0,
          (SeqNum >= index) ? ((index << 1) - (SeqNum + 1)) : (SeqNum + 1)...};
    }
  }
  // NOLINTEND(readability-function-cognitive-complexity)

  size_t length_;
  FixedBorderType border_type_;
};  // end of class FixedBorderInfo<T, KernelSize>

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_BORDER_H
