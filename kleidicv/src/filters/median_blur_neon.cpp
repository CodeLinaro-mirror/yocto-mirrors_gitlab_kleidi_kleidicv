// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/median_blur.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/neon.h"
#include "kleidicv/workspace/border_5x5.h"

namespace kleidicv::neon {

// Primary template for Median Blur filters.
template <typename ScalarType, size_t KernelSize>
class MedianBlur;

// Template for Median Blur 5x5 filters.
template <typename ScalarType>
class MedianBlur<ScalarType, 5> {
 public:
  using SourceType = ScalarType;
  using DestinationType = SourceType;
  using SourceVectorType = typename VecTraits<SourceType>::VectorType;
  using DestinationVectorType = typename VecTraits<DestinationType>::VectorType;

  void vector_path(SourceVectorType window[5][5],
                   DestinationVectorType& dst) const {
    sorting_network5x5<vectorized_comparator>(window, dst);
  }

  void scalar_path(SourceType window[5][5], DestinationType& dst) const {
    sorting_network5x5<scalar_comparator>(window, dst);
  }

 private:
  class vectorized_comparator {
   public:
    static void compare_and_swap(SourceVectorType& left,
                                 SourceVectorType& right) {
      SourceVectorType tmp_left = vmaxq(left, right);
      SourceVectorType tmp_right = vminq(left, right);
      left = tmp_left;
      right = tmp_right;
    }

    static void min(SourceVectorType& left, SourceVectorType& right) {
      right = vminq(left, right);
    }

    static void max(SourceVectorType& left, SourceVectorType& right) {
      left = vmaxq(left, right);
    }
  };

  class scalar_comparator {
   public:
    static void compare_and_swap(ScalarType& left, ScalarType& right) {
      if (left < right) {
        std::swap(left, right);
      }
    }

    static void min(ScalarType& left, ScalarType& right) {
      right = std::min(left, right);
    }

    static void max(ScalarType& left, ScalarType& right) {
      left = std::max(left, right);
    }
  };

  // R. B. Kent and M. S. Pattichis, ''Design of high-speed multiway merge
  // sorting networks using fast single-stage N-sorters and N-filters,'' *IEEE
  // Access*, vol. 10, pp. 79565–79581, Jul. 2022,
  // doi: 10.1109/ACCESS.2022.3193370. The paper is currently available at:
  // https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=9837930
  template <class Comparator, typename T>
  void sorting_network5x5(T window[5][5], T& dst) const {
    // full sort col
    // col 0
    Comparator::compare_and_swap(window[0][0], window[3][0]);
    Comparator::compare_and_swap(window[1][0], window[4][0]);
    Comparator::compare_and_swap(window[0][0], window[2][0]);
    Comparator::compare_and_swap(window[1][0], window[3][0]);
    Comparator::compare_and_swap(window[0][0], window[1][0]);
    Comparator::compare_and_swap(window[2][0], window[4][0]);
    Comparator::compare_and_swap(window[1][0], window[2][0]);
    Comparator::compare_and_swap(window[3][0], window[4][0]);
    Comparator::compare_and_swap(window[2][0], window[3][0]);
    // col 1
    Comparator::compare_and_swap(window[0][1], window[3][1]);
    Comparator::compare_and_swap(window[1][1], window[4][1]);
    Comparator::compare_and_swap(window[0][1], window[2][1]);
    Comparator::compare_and_swap(window[1][1], window[3][1]);
    Comparator::compare_and_swap(window[0][1], window[1][1]);
    Comparator::compare_and_swap(window[2][1], window[4][1]);
    Comparator::compare_and_swap(window[1][1], window[2][1]);
    Comparator::compare_and_swap(window[3][1], window[4][1]);
    Comparator::compare_and_swap(window[2][1], window[3][1]);
    // col 2
    Comparator::compare_and_swap(window[0][2], window[3][2]);
    Comparator::compare_and_swap(window[1][2], window[4][2]);
    Comparator::compare_and_swap(window[0][2], window[2][2]);
    Comparator::compare_and_swap(window[1][2], window[3][2]);
    Comparator::compare_and_swap(window[0][2], window[1][2]);
    Comparator::compare_and_swap(window[2][2], window[4][2]);
    Comparator::compare_and_swap(window[1][2], window[2][2]);
    Comparator::compare_and_swap(window[3][2], window[4][2]);
    Comparator::compare_and_swap(window[2][2], window[3][2]);
    // col 3
    Comparator::compare_and_swap(window[0][3], window[3][3]);
    Comparator::compare_and_swap(window[1][3], window[4][3]);
    Comparator::compare_and_swap(window[0][3], window[2][3]);
    Comparator::compare_and_swap(window[1][3], window[3][3]);
    Comparator::compare_and_swap(window[0][3], window[1][3]);
    Comparator::compare_and_swap(window[2][3], window[4][3]);
    Comparator::compare_and_swap(window[1][3], window[2][3]);
    Comparator::compare_and_swap(window[3][3], window[4][3]);
    Comparator::compare_and_swap(window[2][3], window[3][3]);
    // col 4
    Comparator::compare_and_swap(window[0][4], window[3][4]);
    Comparator::compare_and_swap(window[1][4], window[4][4]);
    Comparator::compare_and_swap(window[0][4], window[2][4]);
    Comparator::compare_and_swap(window[1][4], window[3][4]);
    Comparator::compare_and_swap(window[0][4], window[1][4]);
    Comparator::compare_and_swap(window[2][4], window[4][4]);
    Comparator::compare_and_swap(window[1][4], window[2][4]);
    Comparator::compare_and_swap(window[3][4], window[4][4]);
    Comparator::compare_and_swap(window[2][4], window[3][4]);
    // partialy sort row
    // sort row zero for only element 3 and 4
    Comparator::compare_and_swap(window[0][0], window[0][3]);
    Comparator::compare_and_swap(window[0][1], window[0][4]);
    Comparator::compare_and_swap(window[0][0], window[0][2]);
    Comparator::compare_and_swap(window[0][1], window[0][3]);
    Comparator::min(window[0][0], window[0][1]);
    Comparator::compare_and_swap(window[0][2], window[0][4]);
    Comparator::min(window[0][1], window[0][2]);
    Comparator::compare_and_swap(window[0][3], window[0][4]);
    Comparator::min(window[0][2], window[0][3]);
    // sort row 1 for only element {2, 3, 4}
    Comparator::compare_and_swap(window[1][0], window[1][3]);
    Comparator::compare_and_swap(window[1][1], window[1][4]);
    Comparator::compare_and_swap(window[1][0], window[1][2]);
    Comparator::compare_and_swap(window[1][1], window[1][3]);
    Comparator::min(window[1][0], window[1][1]);
    Comparator::compare_and_swap(window[1][2], window[1][4]);
    Comparator::min(window[1][1], window[1][2]);
    Comparator::compare_and_swap(window[1][3], window[1][4]);
    Comparator::compare_and_swap(window[1][2], window[1][3]);
    // sort row 2 {1, 2, 3}
    Comparator::compare_and_swap(window[2][0], window[2][3]);
    Comparator::compare_and_swap(window[2][1], window[2][4]);
    Comparator::compare_and_swap(window[2][0], window[2][2]);
    Comparator::compare_and_swap(window[2][1], window[2][3]);
    Comparator::min(window[2][0], window[2][1]);
    Comparator::compare_and_swap(window[2][2], window[2][4]);
    Comparator::compare_and_swap(window[2][1], window[2][2]);
    Comparator::max(window[2][3], window[2][4]);
    Comparator::compare_and_swap(window[2][2], window[2][3]);
    // sort row 3
    Comparator::compare_and_swap(window[3][0], window[3][3]);
    Comparator::compare_and_swap(window[3][1], window[3][4]);
    Comparator::compare_and_swap(window[3][0], window[3][2]);
    Comparator::compare_and_swap(window[3][1], window[3][3]);
    Comparator::compare_and_swap(window[3][0], window[3][1]);
    Comparator::compare_and_swap(window[3][2], window[3][4]);
    Comparator::compare_and_swap(window[3][1], window[3][2]);
    Comparator::max(window[3][3], window[3][4]);
    Comparator::max(window[3][2], window[3][3]);
    // sort row 4
    Comparator::compare_and_swap(window[4][0], window[4][3]);
    Comparator::compare_and_swap(window[4][1], window[4][4]);
    Comparator::compare_and_swap(window[4][0], window[4][2]);
    Comparator::max(window[4][1], window[4][3]);
    Comparator::compare_and_swap(window[4][0], window[4][1]);
    Comparator::max(window[4][2], window[4][4]);
    Comparator::max(window[4][1], window[4][2]);
    // partialy sort digonal
    // sort dig 0
    Comparator::min(window[0][3], window[2][1]);
    Comparator::min(window[1][2], window[3][0]);
    Comparator::min(window[2][1], window[3][0]);
    // sort dig 1
    Comparator::compare_and_swap(window[0][4], window[3][1]);
    Comparator::compare_and_swap(window[1][3], window[4][0]);
    Comparator::compare_and_swap(window[0][4], window[2][2]);
    Comparator::compare_and_swap(window[1][3], window[3][1]);
    Comparator::min(window[0][4], window[1][3]);
    Comparator::compare_and_swap(window[2][2], window[4][0]);
    Comparator::min(window[1][3], window[2][2]);
    Comparator::max(window[3][1], window[4][0]);
    Comparator::max(window[2][2], window[3][1]);
    // sort dig 2
    Comparator::max(window[1][4], window[3][2]);
    Comparator::max(window[2][3], window[4][1]);
    Comparator::max(window[1][4], window[2][3]);
    // find median
    Comparator::compare_and_swap(window[1][4], window[3][0]);
    Comparator::min(window[1][4], window[2][2]);
    Comparator::max(window[2][2], window[3][0]);
    dst = window[2][2];
  }
};  // end of class MedianBlur<ScalarType, 5>

// Primary Template for Filter 2D.
template <typename FilterType, const size_t S>
class Filter2D;

// Template for Filter2D 5x5.
template <typename FilterType>
class Filter2D<FilterType, 5UL> {
 public:
  using SourceType = typename FilterType::SourceType;
  using DestinationType = typename FilterType::DestinationType;
  using SourceVecTraits = typename neon::VecTraits<SourceType>;
  using SourceVectorType = typename SourceVecTraits::VectorType;
  using BorderInfoType =
      typename ::KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo5x5<SourceType>;
  using BorderType = FixedBorderType;
  using BorderOffsets = typename BorderInfoType::Offsets;
  static constexpr size_t margin = 2UL;
  explicit Filter2D(FilterType filter) : filter_{filter} {}

  void process_pixels_without_horizontal_borders(
      size_t width, Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const {
    LoopUnroll2<TryToAvoidTailLoop> loop{width * src_rows.channels(),
                                         SourceVecTraits::num_lanes()};

    loop.unroll_once([&](size_t index) {
      SourceVectorType src[5][5];
      SourceVectorType dst_vec;

      auto load_array_element = [](const SourceType& x) { return vld1q(&x); };
      load_window(src, load_array_element, src_rows, window_row_offsets,
                  window_col_offsets, index);
      filter_.vector_path(src, dst_vec);

      vst1q(&dst_rows[index], dst_vec);
    });

    loop.tail([&](size_t index) {
      process_one_element_with_or_without_horizontal_borders(
          src_rows, dst_rows, window_row_offsets, window_col_offsets, index);
    });
  }

  void process_one_pixel_with_horizontal_borders(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets,
      BorderOffsets window_col_offsets) const {
    for (size_t index = 0; index < src_rows.channels(); ++index) {
      disable_loop_vectorization();
      process_one_element_with_or_without_horizontal_borders(
          src_rows, dst_rows, window_row_offsets, window_col_offsets, index);
    }
  }

 private:
  template <typename T, typename LoadArrayElementFunctionType>
  void load_window(T src[5][5], LoadArrayElementFunctionType load_array_element,
                   Rows<const SourceType> src_rows,
                   BorderOffsets window_row_offsets,
                   BorderOffsets window_col_offsets, size_t index) const {
    src[0][0] = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c0())[index]);
    src[0][1] = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c1())[index]);
    src[0][2] = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c2())[index]);
    src[0][3] = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c3())[index]);
    src[0][4] = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c4())[index]);
    src[1][0] = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c0())[index]);
    src[1][1] = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c1())[index]);
    src[1][2] = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c2())[index]);
    src[1][3] = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c3())[index]);
    src[1][4] = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c4())[index]);
    src[2][0] = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c0())[index]);
    src[2][1] = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c1())[index]);
    src[2][2] = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c2())[index]);
    src[2][3] = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c3())[index]);
    src[2][4] = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c4())[index]);
    src[3][0] = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c0())[index]);
    src[3][1] = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c1())[index]);
    src[3][2] = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c2())[index]);
    src[3][3] = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c3())[index]);
    src[3][4] = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c4())[index]);
    src[4][0] = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c0())[index]);
    src[4][1] = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c1())[index]);
    src[4][2] = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c2())[index]);
    src[4][3] = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c3())[index]);
    src[4][4] = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c4())[index]);
  }

  void process_one_element_with_or_without_horizontal_borders(
      Rows<const SourceType> src_rows, Rows<SourceType> dst_rows,
      BorderOffsets window_row_offsets, BorderOffsets window_col_offsets,
      size_t index) const {
    SourceType src[5][5];

    auto load_array_element = [](const SourceType& x) { return x; };
    load_window(src, load_array_element, src_rows, window_row_offsets,
                window_col_offsets, index);

    filter_.scalar_path(src, dst_rows[index]);
  }
  FilterType filter_;
};  // end of class Filter2D<FilterType, 5UL>

template <typename FilterType>
void process_filter2d(Rectangle rect, size_t y_begin, size_t y_end,
                      Rows<const typename FilterType::SourceType> src_rows,
                      Rows<typename FilterType::DestinationType> dst_rows,
                      typename FilterType::BorderType border_type,
                      FilterType filter) {
  // Border helper which calculates border offsets.
  typename FilterType::BorderInfoType vertical_border{rect.height(),
                                                      border_type};
  typename FilterType::BorderInfoType horizontal_border{rect.width(),
                                                        border_type};

  for (size_t vertical_index = y_begin; vertical_index < y_end;
       ++vertical_index) {
    auto vertical_offsets = vertical_border.offsets_with_border(vertical_index);
    constexpr size_t margin = filter.margin;

    // Process data affected by left border.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t horizontal_index = 0; horizontal_index < margin;
         ++horizontal_index) {
      auto horizontal_offsets =
          horizontal_border.offsets_with_left_border(horizontal_index);
      filter.process_one_pixel_with_horizontal_borders(
          src_rows.at(vertical_index, horizontal_index),
          dst_rows.at(vertical_index, horizontal_index), vertical_offsets,
          horizontal_offsets);
    }

    // Process data which is not affected by any borders in bulk.
    size_t width_without_borders = rect.width() - (2 * margin);
    auto horizontal_offsets = horizontal_border.offsets_without_border();
    filter.process_pixels_without_horizontal_borders(
        width_without_borders, src_rows.at(vertical_index, margin),
        dst_rows.at(vertical_index, margin), vertical_offsets,
        horizontal_offsets);

    // Process data affected by right border.
    KLEIDICV_FORCE_LOOP_UNROLL
    for (size_t horizontal_index = 0; horizontal_index < margin;
         ++horizontal_index) {
      size_t index = rect.width() - margin + horizontal_index;
      auto horizontal_offsets =
          horizontal_border.offsets_with_right_border(index);
      filter.process_one_pixel_with_horizontal_borders(
          src_rows.at(vertical_index, index),
          dst_rows.at(vertical_index, index), vertical_offsets,
          horizontal_offsets);
    }
  }
}

// Shorthand for 5x5 2D filters driver type.
template <class FilterType>
using Filter2D5x5 = Filter2D<FilterType, 5UL>;

template <typename T>
kleidicv_error_t median_blur_stripe(const T* src, size_t src_stride, T* dst,
                                    size_t dst_stride, size_t width,
                                    size_t height, size_t y_begin, size_t y_end,
                                    size_t channels,
                                    [[maybe_unused]] size_t kernel_width,
                                    [[maybe_unused]] size_t kernel_height,
                                    FixedBorderType border_type) {
  Rectangle rect{width, height};
  Rows<const T> src_rows{src, src_stride, channels};
  Rows<T> dst_rows{dst, dst_stride, channels};
  MedianBlur<T, 5> median_filter;
  Filter2D5x5<MedianBlur<T, 5>> filter{median_filter};
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
