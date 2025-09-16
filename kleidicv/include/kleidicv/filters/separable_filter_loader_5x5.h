// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_5X5_H
#define KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_5X5_H

#include "kleidicv/types.h"
#include "kleidicv/workspace/border_5x5.h"

namespace KLEIDICV_TARGET_NAMESPACE {

struct ZeroOffsets_5_rows {
  constexpr int c0() const KLEIDICV_STREAMING { return 0; }
  constexpr int c1() const KLEIDICV_STREAMING { return 0; }
  constexpr int c2() const KLEIDICV_STREAMING { return 0; }
  constexpr int c3() const KLEIDICV_STREAMING { return 0; }
  constexpr int c4() const KLEIDICV_STREAMING { return 0; }
};

template <typename SourceType>
class SeparableFilterLoader5x5 {
 public:
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using BorderInfoType =
      typename KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo5x5<SourceType>;
  using BorderOffsets = typename BorderInfoType::Offsets;
  template <typename LoadArrayElementFunctionType, typename KernelWindowFunctor,
            typename RowOffsetsT, typename ColOffsetsT>
  static void load_window(KernelWindowFunctor& KernelWindow,
                          LoadArrayElementFunctionType load_array_element,
                          Rows<const SourceType> src_rows,
                          RowOffsetsT window_row_offsets,
                          ColOffsetsT window_col_offsets,
                          size_t index) KLEIDICV_STREAMING {
    KernelWindow(0) = load_array_element(
        src_rows.at(window_row_offsets.c0(), window_col_offsets.c0())[index]);
    KernelWindow(1) = load_array_element(
        src_rows.at(window_row_offsets.c1(), window_col_offsets.c1())[index]);
    KernelWindow(2) = load_array_element(
        src_rows.at(window_row_offsets.c2(), window_col_offsets.c2())[index]);
    KernelWindow(3) = load_array_element(
        src_rows.at(window_row_offsets.c3(), window_col_offsets.c3())[index]);
    KernelWindow(4) = load_array_element(
        src_rows.at(window_row_offsets.c4(), window_col_offsets.c4())[index]);
  }

  template <typename LoadArrayElementFunctionType,
            typename KernelWindowFunctorA, typename KernelWindowFunctorB,
            typename RowOffsetsT, typename ColOffsetsT>
  static void double_load_window(
      KernelWindowFunctorA& KernelWindow_a,
      KernelWindowFunctorB& KernelWindow_b,
      LoadArrayElementFunctionType load_array_element,
      Rows<const SourceType> src_rows, RowOffsetsT window_row_offsets,
      ColOffsetsT window_col_offsets, size_t index) KLEIDICV_STREAMING {
    const SourceType* const src_0 =
        &src_rows.at(window_row_offsets.c0(), window_col_offsets.c0())[index];
    const SourceType* const src_1 =
        &src_rows.at(window_row_offsets.c1(), window_col_offsets.c1())[index];
    const SourceType* const src_2 =
        &src_rows.at(window_row_offsets.c2(), window_col_offsets.c2())[index];
    const SourceType* const src_3 =
        &src_rows.at(window_row_offsets.c3(), window_col_offsets.c3())[index];
    const SourceType* const src_4 =
        &src_rows.at(window_row_offsets.c4(), window_col_offsets.c4())[index];
    KernelWindow_a(0) = load_array_element(src_0[0]);
    KernelWindow_b(0) = load_array_element(src_0[SourceVecTraits::num_lanes()]);
    KernelWindow_a(1) = load_array_element(src_1[0]);
    KernelWindow_b(1) = load_array_element(src_1[SourceVecTraits::num_lanes()]);
    KernelWindow_a(2) = load_array_element(src_2[0]);
    KernelWindow_b(2) = load_array_element(src_2[SourceVecTraits::num_lanes()]);
    KernelWindow_a(3) = load_array_element(src_3[0]);
    KernelWindow_b(3) = load_array_element(src_3[SourceVecTraits::num_lanes()]);
    KernelWindow_a(4) = load_array_element(src_4[0]);
    KernelWindow_b(4) = load_array_element(src_4[SourceVecTraits::num_lanes()]);
  }
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_5X5_H
