// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_2D_5X5_SC_H
#define KLEIDICV_FILTER_2D_5X5_SC_H

#include "filter_2d.h"
#include "filter_2d_5x5_base.h"
#include "kleidicv/sve2.h"

namespace KLEIDICV_TARGET_NAMESPACE {
// Template for Filter2D 5x5.
template <typename InnerFilterType>
class Filter2D<InnerFilterType, 5UL>
    : public Filter2D5x5Base<typename InnerFilterType::SourceType> {
 public:
  using SourceType = typename InnerFilterType::SourceType;
  using DestinationType = typename InnerFilterType::DestinationType;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BorderInfoType =
      typename KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo5x5<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;
  using Base = Filter2D5x5Base<SourceType>;
  static constexpr size_t kMargin = 2UL;
  explicit Filter2D(InnerFilterType filter) KLEIDICV_STREAMING_COMPATIBLE
      : filter_{filter} {}

  void process_pixels_without_horizontal_borders(
      size_t width, Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) KLEIDICV_STREAMING_COMPATIBLE {
      svbool_t pg = SourceVecTraits::svptrue();
      process_elements_with_vector_operation(src_rows, dst_rows,
                                             window_row_offsets,
                                             window_col_offsets, index, pg);
    });

    loop.remaining(
        [&](size_t index, size_t length) KLEIDICV_STREAMING_COMPATIBLE {
          svbool_t pg = SourceVecTraits::svwhilelt(index, length);
          process_elements_with_vector_operation(src_rows, dst_rows,
                                                 window_row_offsets,
                                                 window_col_offsets, index, pg);
        });
  }

  void process_one_pixel_with_horizontal_borders(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const KLEIDICV_STREAMING_COMPATIBLE {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      process_elements_with_vector_operation(
          src_rows, dst_rows, window_row_offsets, window_col_offsets, index,
          SourceVecTraits::template svptrue_pat<SV_VL1>());
    }
  }

 private:
  void process_elements_with_vector_operation(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets, BorderOffsets window_col_offsets,
      size_t index, svbool_t pg) const KLEIDICV_STREAMING_COMPATIBLE {
    SourceVectorType src_0_0, src_0_1, src_0_2, src_0_3, src_0_4, src_1_0,
        src_1_1, src_1_2, src_1_3, src_1_4, src_2_0, src_2_1, src_2_2, src_2_3,
        src_2_4, src_3_0, src_3_1, src_3_2, src_3_3, src_3_4, src_4_0, src_4_1,
        src_4_2, src_4_3, src_4_4, output_vector;

    // Initialization
    ScalableVectorArray2D<SourceVectorType, 5, 5> KernelWindow = {{
        {std::ref(src_0_0), std::ref(src_0_1), std::ref(src_0_2),
         std::ref(src_0_3), std::ref(src_0_4)},
        {std::ref(src_1_0), std::ref(src_1_1), std::ref(src_1_2),
         std::ref(src_1_3), std::ref(src_1_4)},
        {std::ref(src_2_0), std::ref(src_2_1), std::ref(src_2_2),
         std::ref(src_2_3), std::ref(src_2_4)},
        {std::ref(src_3_0), std::ref(src_3_1), std::ref(src_3_2),
         std::ref(src_3_3), std::ref(src_3_4)},
        {std::ref(src_4_0), std::ref(src_4_1), std::ref(src_4_2),
         std::ref(src_4_3), std::ref(src_4_4)},
    }};

    auto load_array_element =
        [&](const SourceType& x)
            KLEIDICV_STREAMING_COMPATIBLE { return svld1(pg, &x); };

    Base::load_window(KernelWindow, load_array_element, src_rows,
                      window_row_offsets, window_col_offsets, index);
    filter_.vector_path(KernelWindow, output_vector, pg);
    svst1(pg, &dst_rows[index], output_vector);
  }

  InnerFilterType filter_;
};  // end of class Filter2D<InnerFilterType, 5UL>

// Shorthand for 5x5 2D filters driver type.
template <class InnerFilterType>
using Filter2D5x5 = Filter2D<InnerFilterType, 5UL>;

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTER_2D_5X5_SC_H
