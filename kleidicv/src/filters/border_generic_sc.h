// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_WORKSPACE_BORDER_GENERIC_SC_H
#define KLEIDICV_WORKSPACE_BORDER_GENERIC_SC_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "kleidicv/sve2.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Border offsets for generic filters.
template <kleidicv::FixedBorderType BorderType>
class GenericBorder final {
 public:
  explicit GenericBorder(size_t width, size_t channels, svuint16_t& left1,
                         svuint16_t& left2, svuint16_t& right1,
                         svuint16_t& right2) KLEIDICV_STREAMING_COMPATIBLE
      : width_(static_cast<ptrdiff_t>(width)),
        channels_{static_cast<ptrdiff_t>(channels)},
        total_width_(width_* channels_),
        border_indices_left_(left1),
        border_indices_left_ext_(left2),
        border_indices_right_(right1),
        border_indices_right_ext_(right2) {
    // The result will take some elements from the image (data), and the
    // remaining parts from the border.
    // An index vector is prepared here to help the process, e.g. for
    // replicated borders and 3 channels, the constructed index vector will
    // look like this:
    //               [1, 2, 0, 1, 2, 3, 4, 5]
    // (0,1,2 is repeated until index 0 is reached, when the image data
    // begins.) Right side is similar, but it is the [5,6,7] that repeats
    // after.
    uint16_t left[128 + 4], right[128 + 4];
    // This is to ensure the last element be (channels_ - 1)
    uint16_t bias = channels_ - 1 - ((svcnth() - 1) % channels_);
    for (size_t i = 0; i < svcnth() + 4; ++i) {
      left[i] = (i + bias) % channels_;
      right[i] = (i % channels_) + svcnth() - channels_;
    }
    // Analyser thinks left[0] is garbage, but it is not.
    // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
    border_indices_left_ = svld1_u16(svptrue_b16(), left);
    border_indices_left_ext_ =
        svld1_u16(svptrue_b16(), left + channels_ - left[0]);
    border_indices_right_ = svld1_u16(svptrue_b16(), right);
    border_indices_right_ext_ = svld1_u16(svptrue_b16(), right + left[0]);
    // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)
  }

  // Raw column can be bigger than width-1 or less than 0
  ptrdiff_t get_column(ptrdiff_t raw_column) const {
    // TODO more border types, this is only the Replicated
    return std::max<ptrdiff_t>(std::min<ptrdiff_t>(raw_column, width_ - 1),
                               ptrdiff_t{0});
  }

  // Assuming that start_offset is <= 0
  svuint16_t load_left(Rows<const uint8_t> src_rows,
                       ptrdiff_t start_offset) const {
    if constexpr (BorderType == FixedBorderType::REPLICATE) {
      svuint16_t data = svld1ub_u16(svptrue_b16(), &src_rows[0]);
      svuint16_t indices{};
      svuint16_t increasing = svindex_u16(0, 1);
      if (-start_offset < static_cast<ptrdiff_t>(svcnth())) {
        // '-start_offset' elements from the border, the others from the data
        svbool_t pg =
            svcmpge_n_u16(svptrue_b16(), increasing,
                          static_cast<uint16_t>(svcnth() + start_offset));
        indices = svsplice_u16(pg, border_indices_left_, increasing);
      } else {
        // 'shift' elements need to be shifted out
        ptrdiff_t shift = channels_ - (-start_offset - svcnth()) % channels_;
        svbool_t pg = svcmpge_n_u16(svptrue_b16(), increasing, shift);
        indices =
            svsplice_u16(pg, border_indices_left_, border_indices_left_ext_);
      }
      return svtbl_u16(data, indices);
    }
  }

  // Assuming that start_offset is >= width - svcnth()
  svuint16_t load_right(Rows<const uint8_t> src_rows,
                        ptrdiff_t start_offset) const {
    if constexpr (BorderType == FixedBorderType::REPLICATE) {
      svuint16_t data =
          svld1ub_u16(svptrue_b16(), &src_rows[total_width_ - svcnth()]);
      svuint16_t indices{};
      svuint16_t increasing = svindex_u16(0, 1);
      if (start_offset <= width_ * channels_) {
        svbool_t pg = svcmpge_n_u16(
            svptrue_b16(), increasing,
            static_cast<uint16_t>(
                start_offset -
                (total_width_ - static_cast<ptrdiff_t>(svcnth()))));
        indices = svsplice_u16(pg, increasing, border_indices_right_);
      } else {
        ptrdiff_t shift =
            svcnth() -
            (channels_ - (start_offset - width_ * channels_) % channels_);
        svbool_t pg = svcmpge_n_u16(svptrue_b16(), increasing, shift);
        indices =
            svsplice_u16(pg, border_indices_right_ext_, border_indices_right_);
      }

      return svtbl_u16(data, indices);
    }
  }

 private:
  ptrdiff_t width_, channels_, total_width_;
  svuint16_t &border_indices_left_, &border_indices_left_ext_,
      &border_indices_right_, &border_indices_right_ext_;
};  // end of class GenericBorder<BorderType>
}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_WORKSPACE_BORDER_GENERIC_NEON_H
