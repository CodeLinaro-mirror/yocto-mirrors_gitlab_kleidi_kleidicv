// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_MEDIAN_BLUR_SC_H
#define KLEIDICV_MEDIAN_BLUR_SC_H

#include <algorithm>
#include <utility>

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/filter_2d.h"
#include "kleidicv/filters/filter_2d_5x5_sc.h"
#include "kleidicv/filters/filter_2d_7x7_sc.h"
#include "kleidicv/filters/median_blur.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/sve2.h"
#include "kleidicv/workspace/border_5x5.h"
#include "kleidicv/workspace/border_7x7.h"
#include "median_blur_sorting_network_5x5.h"
#include "median_blur_sorting_network_7x7.h"
namespace KLEIDICV_TARGET_NAMESPACE {

// Primary template for Median Blur filters.
template <typename ScalarType, size_t KernelSize>
class MedianBlur;

template <typename ScalarType>
class MedianBlurBase {
 protected:
  class vectorized_comparator {
   public:
    using SourceVectorType = typename VecTraits<ScalarType>::VectorType;

    static void compare_and_swap(SourceVectorType& left,
                                 SourceVectorType& right,
                                 svbool_t& pg) KLEIDICV_STREAMING_COMPATIBLE {
      SourceVectorType max_value = svmax_m(pg, left, right);
      SourceVectorType min_value = svmin_m(pg, left, right);
      left = min_value;
      right = max_value;
    }

    static void min(SourceVectorType& left, SourceVectorType& right,
                    svbool_t& pg) KLEIDICV_STREAMING_COMPATIBLE {
      left = svmin_m(pg, left, right);
    }

    static void max(SourceVectorType& left, SourceVectorType& right,
                    svbool_t& pg) KLEIDICV_STREAMING_COMPATIBLE {
      right = svmax_m(pg, left, right);
    }
  };
};

// Template for Median Blur 5x5 filters.
template <typename ScalarType>
class MedianBlur<ScalarType, 5> : public MedianBlurBase<ScalarType> {
 public:
  using SourceType = ScalarType;
  using DestinationType = SourceType;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using DestinationVectorType = typename KLEIDICV_TARGET_NAMESPACE::VecTraits<
      DestinationType>::VectorType;
  using VectorComparator =
      typename MedianBlurBase<SourceType>::vectorized_comparator;
  template <typename KernelWindowFunctor>
  void vector_path(KernelWindowFunctor& KernelWindow,
                   DestinationVectorType& output_vec,
                   svbool_t& pg) const KLEIDICV_STREAMING_COMPATIBLE {
    sorting_network5x5<VectorComparator>(KernelWindow, output_vec, pg);
  }
};  // end of class MedianBlur<ScalarType, 5>

// Template for Median Blur 7x7 filters.
template <typename ScalarType>
class MedianBlur<ScalarType, 7> : public MedianBlurBase<ScalarType> {
 public:
  using SourceType = ScalarType;
  using DestinationType = SourceType;
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using DestinationVectorType = typename KLEIDICV_TARGET_NAMESPACE::VecTraits<
      DestinationType>::VectorType;
  using VectorComparator =
      typename MedianBlurBase<SourceType>::vectorized_comparator;

  template <typename KernelWindowFunctor>
  void vector_path(KernelWindowFunctor& KernelWindow,
                   DestinationVectorType& output_vec,
                   svbool_t& pg) const KLEIDICV_STREAMING_COMPATIBLE {
    sorting_network7x7<VectorComparator>(KernelWindow, output_vec, pg);
  }
};  // end of class MedianBlur<ScalarType, 7>

template <typename T>
kleidicv_error_t median_blur_stripe_sc(
    const T* src, size_t src_stride, T* dst, size_t dst_stride, size_t width,
    size_t height, size_t y_begin, size_t y_end, size_t channels,
    [[maybe_unused]] size_t kernel_width, [[maybe_unused]] size_t kernel_height,
    FixedBorderType border_type) KLEIDICV_STREAMING_COMPATIBLE {
  Rectangle rect{width, height};
  Rows<const T> src_rows{src, src_stride, channels};
  Rows<T> dst_rows{dst, dst_stride, channels};
  if (kernel_width == 5) {
    MedianBlur<T, 5> median_filter;
    Filter2D5x5<MedianBlur<T, 5>> filter{median_filter};
    process_filter2d(rect, y_begin, y_end, src_rows, dst_rows, border_type,
                     filter);
    return KLEIDICV_OK;
  }

  MedianBlur<T, 7> median_filter;
  Filter2D7x7<MedianBlur<T, 7>> filter{median_filter};
  process_filter2d(rect, y_begin, y_end, src_rows, dst_rows, border_type,
                   filter);
  return KLEIDICV_OK;
}

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_MEDIAN_BLUR_SC_H
