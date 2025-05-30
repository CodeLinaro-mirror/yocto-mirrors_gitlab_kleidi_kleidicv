// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTERS_FILTER_2D_H
#define KLEIDICV_FILTERS_FILTER_2D_H

#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"
#include "kleidicv/workspace/border_types.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Primary Template for Filter 2D.
template <typename InnerFilterType, const size_t S>
class Filter2D;

template <typename FilterType>
void process_filter2d(Rectangle rect, size_t y_begin, size_t y_end,
                      Rows<const typename FilterType::SourceType> src_rows,
                      Rows<typename FilterType::DestinationType> dst_rows,
                      typename FilterType::BorderType border_type,
                      FilterType filter) KLEIDICV_STREAMING_COMPATIBLE {
  // Border helper which calculates border offsets.
  typename FilterType::BorderInfoType vertical_border{rect.height(),
                                                      border_type};
  typename FilterType::BorderInfoType horizontal_border{rect.width(),
                                                        border_type};

  for (size_t vertical_index = y_begin; vertical_index < y_end;
       ++vertical_index) {
    auto vertical_offsets = vertical_border.offsets_with_border(vertical_index);
    constexpr size_t kMargin = filter.kMargin;

    // Process data affected by left border.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t horizontal_index = 0; horizontal_index < kMargin;
         ++horizontal_index) {
      auto horizontal_offsets =
          horizontal_border.offsets_with_left_border(horizontal_index);
      filter.process_one_pixel_with_horizontal_borders(
          src_rows.at(vertical_index, horizontal_index),
          dst_rows.at(vertical_index, horizontal_index), vertical_offsets,
          horizontal_offsets);
    }

    // Process data which is not affected by any borders in bulk.
    size_t width_without_borders = rect.width() - (2 * kMargin);
    auto horizontal_offsets = horizontal_border.offsets_without_border();
    filter.process_pixels_without_horizontal_borders(
        width_without_borders, src_rows.at(vertical_index, kMargin),
        dst_rows.at(vertical_index, kMargin), vertical_offsets,
        horizontal_offsets);

    // Process data affected by right border.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t horizontal_index = 0; horizontal_index < kMargin;
         ++horizontal_index) {
      size_t index = rect.width() - kMargin + horizontal_index;
      auto horizontal_offsets =
          horizontal_border.offsets_with_right_border(index);
      filter.process_one_pixel_with_horizontal_borders(
          src_rows.at(vertical_index, index),
          dst_rows.at(vertical_index, index), vertical_offsets,
          horizontal_offsets);
    }
  }
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTERS_FILTER_2D_H
