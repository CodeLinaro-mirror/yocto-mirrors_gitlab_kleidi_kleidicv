// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/filter_2d.h"
#include "kleidicv/filters/filter_2d_5x5_neon.h"
#include "kleidicv/filters/filter_2d_7x7_neon.h"
#include "kleidicv/filters/median_blur.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/workspace/border_5x5.h"
#include "kleidicv/workspace/border_7x7.h"
#include "median_blur_sorting_network_5x5.h"
#include "median_blur_sorting_network_7x7.h"

namespace kleidicv::neon {

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
                                 SourceVectorType& right, Monostate&) {
      SourceVectorType max_value = vmaxq(left, right);
      SourceVectorType min_value = vminq(left, right);
      left = min_value;
      right = max_value;
    }

    static void min(SourceVectorType& left, SourceVectorType& right,
                    Monostate&) {
      left = vminq(left, right);
    }

    static void max(SourceVectorType& left, SourceVectorType& right,
                    Monostate&) {
      right = vmaxq(left, right);
    }
  };
  class scalar_comparator {
   public:
    static void compare_and_swap(ScalarType& left, ScalarType& right,
                                 Monostate&) {
      if (left > right) {
        std::swap(left, right);
      }
    }

    static void min(ScalarType& left, ScalarType& right, Monostate&) {
      left = std::min(left, right);
    }

    static void max(ScalarType& left, ScalarType& right, Monostate&) {
      right = std::max(left, right);
    }
  };
};

// Template for Median Blur 5x5 filters.
template <typename ScalarType>
class MedianBlur<ScalarType, 5> : public MedianBlurBase<ScalarType> {
 public:
  using SourceType = ScalarType;
  using DestinationType = SourceType;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using DestinationVectorType = typename VecTraits<DestinationType>::VectorType;
  using VectorComparator =
      typename MedianBlurBase<SourceType>::vectorized_comparator;
  using ScalarComparator =
      typename MedianBlurBase<SourceType>::scalar_comparator;

  template <typename KernelWindowFunctor>
  void vector_path(KernelWindowFunctor& KernelWindow,
                   DestinationVectorType& output_vec) const {
    Monostate ctx;
    sorting_network5x5<VectorComparator>(KernelWindow, output_vec, ctx);
  }

  template <typename KernelWindowFunctor>
  void scalar_path(KernelWindowFunctor& KernelWindow,
                   DestinationType& dst) const {
    Monostate ctx;
    sorting_network5x5<ScalarComparator>(KernelWindow, dst, ctx);
  }
};  // end of class MedianBlur<ScalarType, 5>

// Template for Median Blur 7x57 filters.
template <typename ScalarType>
class MedianBlur<ScalarType, 7> : public MedianBlurBase<ScalarType> {
 public:
  using SourceType = ScalarType;
  using DestinationType = SourceType;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using DestinationVectorType = typename VecTraits<DestinationType>::VectorType;
  using VectorComparator =
      typename MedianBlurBase<SourceType>::vectorized_comparator;
  using ScalarComparator =
      typename MedianBlurBase<SourceType>::scalar_comparator;

  template <typename KernelWindowFunctor>
  void vector_path(KernelWindowFunctor& KernelWindow,
                   DestinationVectorType& dst) const {
    Monostate ctx;
    sorting_network7x7<VectorComparator>(KernelWindow, dst, ctx);
  }

  template <typename KernelWindowFunctor>
  void scalar_path(KernelWindowFunctor& KernelWindow,
                   DestinationType& dst) const {
    Monostate ctx;
    sorting_network7x7<ScalarComparator>(KernelWindow, dst, ctx);
  }
};  // end of class MedianBlur<ScalarType, 7>

template <typename T>
kleidicv_error_t median_blur_stripe(const T* src, size_t src_stride, T* dst,
                                    size_t dst_stride, size_t width,
                                    size_t height, size_t y_begin, size_t y_end,
                                    size_t channels, size_t kernel_width,
                                    [[maybe_unused]] size_t kernel_height,
                                    FixedBorderType border_type) {
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

#define KLEIDICV_INSTANTIATE_TEMPLATE(type)                                    \
  template KLEIDICV_TARGET_FN_ATTRS kleidicv_error_t median_blur_stripe<type>( \
      const type* src, size_t src_stride, type* dst, size_t dst_stride,        \
      size_t width, size_t height, size_t y_begin, size_t y_end,               \
      size_t channels, size_t kernel_width, size_t kernel_height,              \
      FixedBorderType border_type)

KLEIDICV_INSTANTIATE_TEMPLATE(int8_t);
KLEIDICV_INSTANTIATE_TEMPLATE(uint8_t);
KLEIDICV_INSTANTIATE_TEMPLATE(int16_t);
KLEIDICV_INSTANTIATE_TEMPLATE(uint16_t);
KLEIDICV_INSTANTIATE_TEMPLATE(int32_t);
KLEIDICV_INSTANTIATE_TEMPLATE(uint32_t);
KLEIDICV_INSTANTIATE_TEMPLATE(float);

}  // namespace kleidicv::neon
