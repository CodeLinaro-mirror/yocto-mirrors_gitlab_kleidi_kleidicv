// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_2D_5X5_NEON_H
#define KLEIDICV_FILTER_2D_5X5_NEON_H

#include "filter_2d.h"
#include "filter_2d_5x5_base.h"
#include "kleidicv/neon.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Template for Filter2D 5x5.
template <typename InnerFilterType>
class Filter2D<InnerFilterType, 5UL>
    : public Filter2D5x5Base<typename InnerFilterType::SourceType> {
 public:
  using SourceType = typename InnerFilterType::SourceType;
  using DestinationType = typename InnerFilterType::DestinationType;
  using SourceVecTraits = typename neon::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using Base = Filter2D5x5Base<SourceType>;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo5x5<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;
  static constexpr size_t kMargin = 2UL;
  explicit Filter2D(InnerFilterType filter) : filter_{filter} {}

  void process_pixels_without_horizontal_borders(
      size_t width, Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) {
      SourceVectorType src[5][5];
      SourceVectorType dst_vec;

      auto KernelWindow = [&](size_t row, size_t col) -> SourceVectorType& {
        return src[row][col];
      };

      auto load_array_element = [](const SourceType& x) { return vld1q(&x); };
      Base::load_window(KernelWindow, load_array_element, src_rows,
                        window_row_offsets, window_col_offsets, index);
      filter_.vector_path(KernelWindow, dst_vec);

      vst1q(&dst_rows[index], dst_vec);
    });

    loop.tail([&](size_t index) {
      process_one_element_with_horizontal_borders(
          src_rows, dst_rows, window_row_offsets, window_col_offsets, index);
    });
  }

  void process_one_pixel_with_horizontal_borders(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_one_element_with_horizontal_borders(
          src_rows, dst_rows, window_row_offsets, window_col_offsets, index);
    }
  }

 private:
  void process_one_element_with_horizontal_borders(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets, BorderOffsets window_col_offsets,
      size_t index) const KLEIDICV_STREAMING_COMPATIBLE {
    SourceType src[5][5];

    auto KernelWindow = [&](size_t row, size_t col)
                            KLEIDICV_STREAMING_COMPATIBLE -> SourceType& {
      return src[row][col];
    };

    auto load_array_element = [&](const SourceType& x)
                                  KLEIDICV_STREAMING_COMPATIBLE { return x; };

    Base::load_window(KernelWindow, load_array_element, src_rows,
                      window_row_offsets, window_col_offsets, index);

    filter_.scalar_path(KernelWindow, dst_rows[index]);
  }

  InnerFilterType filter_;
};  // end of class Filter2D<InnerFilterType, 5UL>

// Shorthand for 5x5 2D filters driver type.
template <class InnerFilterType>
using Filter2D5x5 = Filter2D<InnerFilterType, 5UL>;

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTER_2D_5X5_NEON_H
