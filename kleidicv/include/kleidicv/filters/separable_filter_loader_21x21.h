// SPDX-FileCopyrightText: 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_21X21_H
#define KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_21X21_H

#include "kleidicv/types.h"
#include "kleidicv/workspace/border_21x21.h"

namespace KLEIDICV_TARGET_NAMESPACE {

struct ZeroOffsets_21_rows {
  constexpr int c0() const KLEIDICV_STREAMING { return 0; }
  constexpr int c1() const KLEIDICV_STREAMING { return 0; }
  constexpr int c2() const KLEIDICV_STREAMING { return 0; }
  constexpr int c3() const KLEIDICV_STREAMING { return 0; }
  constexpr int c4() const KLEIDICV_STREAMING { return 0; }
  constexpr int c5() const KLEIDICV_STREAMING { return 0; }
  constexpr int c6() const KLEIDICV_STREAMING { return 0; }
  constexpr int c7() const KLEIDICV_STREAMING { return 0; }
  constexpr int c8() const KLEIDICV_STREAMING { return 0; }
  constexpr int c9() const KLEIDICV_STREAMING { return 0; }
  constexpr int c10() const KLEIDICV_STREAMING { return 0; }
  constexpr int c11() const KLEIDICV_STREAMING { return 0; }
  constexpr int c12() const KLEIDICV_STREAMING { return 0; }
  constexpr int c13() const KLEIDICV_STREAMING { return 0; }
  constexpr int c14() const KLEIDICV_STREAMING { return 0; }
  constexpr int c15() const KLEIDICV_STREAMING { return 0; }
  constexpr int c16() const KLEIDICV_STREAMING { return 0; }
  constexpr int c17() const KLEIDICV_STREAMING { return 0; }
  constexpr int c18() const KLEIDICV_STREAMING { return 0; }
  constexpr int c19() const KLEIDICV_STREAMING { return 0; }
  constexpr int c20() const KLEIDICV_STREAMING { return 0; }
};

template <typename SourceType>
class SeparableFilterLoader21x21 {
 public:
  using SourceVecTraits =
      typename KLEIDICV_TARGET_NAMESPACE::VecTraits<SourceType>;
  using BorderInfoType =
      typename KLEIDICV_TARGET_NAMESPACE::FixedBorderInfo21x21<SourceType>;
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
    KernelWindow(5) = load_array_element(
        src_rows.at(window_row_offsets.c5(), window_col_offsets.c5())[index]);
    KernelWindow(6) = load_array_element(
        src_rows.at(window_row_offsets.c6(), window_col_offsets.c6())[index]);
    KernelWindow(7) = load_array_element(
        src_rows.at(window_row_offsets.c7(), window_col_offsets.c7())[index]);
    KernelWindow(8) = load_array_element(
        src_rows.at(window_row_offsets.c8(), window_col_offsets.c8())[index]);
    KernelWindow(9) = load_array_element(
        src_rows.at(window_row_offsets.c9(), window_col_offsets.c9())[index]);
    KernelWindow(10) = load_array_element(
        src_rows.at(window_row_offsets.c10(), window_col_offsets.c10())[index]);
    KernelWindow(11) = load_array_element(
        src_rows.at(window_row_offsets.c11(), window_col_offsets.c11())[index]);
    KernelWindow(12) = load_array_element(
        src_rows.at(window_row_offsets.c12(), window_col_offsets.c12())[index]);
    KernelWindow(13) = load_array_element(
        src_rows.at(window_row_offsets.c13(), window_col_offsets.c13())[index]);
    KernelWindow(14) = load_array_element(
        src_rows.at(window_row_offsets.c14(), window_col_offsets.c14())[index]);
    KernelWindow(15) = load_array_element(
        src_rows.at(window_row_offsets.c15(), window_col_offsets.c15())[index]);
    KernelWindow(16) = load_array_element(
        src_rows.at(window_row_offsets.c16(), window_col_offsets.c16())[index]);
    KernelWindow(17) = load_array_element(
        src_rows.at(window_row_offsets.c17(), window_col_offsets.c17())[index]);
    KernelWindow(18) = load_array_element(
        src_rows.at(window_row_offsets.c18(), window_col_offsets.c18())[index]);
    KernelWindow(19) = load_array_element(
        src_rows.at(window_row_offsets.c19(), window_col_offsets.c19())[index]);
    KernelWindow(20) = load_array_element(
        src_rows.at(window_row_offsets.c20(), window_col_offsets.c20())[index]);
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
    const SourceType* const src_5 =
        &src_rows.at(window_row_offsets.c5(), window_col_offsets.c5())[index];
    const SourceType* const src_6 =
        &src_rows.at(window_row_offsets.c6(), window_col_offsets.c6())[index];
    const SourceType* const src_7 =
        &src_rows.at(window_row_offsets.c7(), window_col_offsets.c7())[index];
    const SourceType* const src_8 =
        &src_rows.at(window_row_offsets.c8(), window_col_offsets.c8())[index];
    const SourceType* const src_9 =
        &src_rows.at(window_row_offsets.c9(), window_col_offsets.c9())[index];
    const SourceType* const src_10 =
        &src_rows.at(window_row_offsets.c10(), window_col_offsets.c10())[index];
    const SourceType* const src_11 =
        &src_rows.at(window_row_offsets.c11(), window_col_offsets.c11())[index];
    const SourceType* const src_12 =
        &src_rows.at(window_row_offsets.c12(), window_col_offsets.c12())[index];
    const SourceType* const src_13 =
        &src_rows.at(window_row_offsets.c13(), window_col_offsets.c13())[index];
    const SourceType* const src_14 =
        &src_rows.at(window_row_offsets.c14(), window_col_offsets.c14())[index];
    const SourceType* const src_15 =
        &src_rows.at(window_row_offsets.c15(), window_col_offsets.c15())[index];
    const SourceType* const src_16 =
        &src_rows.at(window_row_offsets.c16(), window_col_offsets.c16())[index];
    const SourceType* const src_17 =
        &src_rows.at(window_row_offsets.c17(), window_col_offsets.c17())[index];
    const SourceType* const src_18 =
        &src_rows.at(window_row_offsets.c18(), window_col_offsets.c18())[index];
    const SourceType* const src_19 =
        &src_rows.at(window_row_offsets.c19(), window_col_offsets.c19())[index];
    const SourceType* const src_20 =
        &src_rows.at(window_row_offsets.c20(), window_col_offsets.c20())[index];
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
    KernelWindow_a(5) = load_array_element(src_5[0]);
    KernelWindow_b(5) = load_array_element(src_5[SourceVecTraits::num_lanes()]);
    KernelWindow_a(6) = load_array_element(src_6[0]);
    KernelWindow_b(6) = load_array_element(src_6[SourceVecTraits::num_lanes()]);
    KernelWindow_a(7) = load_array_element(src_7[0]);
    KernelWindow_b(7) = load_array_element(src_7[SourceVecTraits::num_lanes()]);
    KernelWindow_a(8) = load_array_element(src_8[0]);
    KernelWindow_b(8) = load_array_element(src_8[SourceVecTraits::num_lanes()]);
    KernelWindow_a(9) = load_array_element(src_9[0]);
    KernelWindow_b(9) = load_array_element(src_9[SourceVecTraits::num_lanes()]);
    KernelWindow_a(10) = load_array_element(src_10[0]);
    KernelWindow_b(10) =
        load_array_element(src_10[SourceVecTraits::num_lanes()]);
    KernelWindow_a(11) = load_array_element(src_11[0]);
    KernelWindow_b(11) =
        load_array_element(src_11[SourceVecTraits::num_lanes()]);
    KernelWindow_a(12) = load_array_element(src_12[0]);
    KernelWindow_b(12) =
        load_array_element(src_12[SourceVecTraits::num_lanes()]);
    KernelWindow_a(13) = load_array_element(src_13[0]);
    KernelWindow_b(13) =
        load_array_element(src_13[SourceVecTraits::num_lanes()]);
    KernelWindow_a(14) = load_array_element(src_14[0]);
    KernelWindow_b(14) =
        load_array_element(src_14[SourceVecTraits::num_lanes()]);
    KernelWindow_a(15) = load_array_element(src_15[0]);
    KernelWindow_b(15) =
        load_array_element(src_15[SourceVecTraits::num_lanes()]);
    KernelWindow_a(16) = load_array_element(src_16[0]);
    KernelWindow_b(16) =
        load_array_element(src_16[SourceVecTraits::num_lanes()]);
    KernelWindow_a(17) = load_array_element(src_17[0]);
    KernelWindow_b(17) =
        load_array_element(src_17[SourceVecTraits::num_lanes()]);
    KernelWindow_a(18) = load_array_element(src_18[0]);
    KernelWindow_b(18) =
        load_array_element(src_18[SourceVecTraits::num_lanes()]);
    KernelWindow_a(19) = load_array_element(src_19[0]);
    KernelWindow_b(19) =
        load_array_element(src_19[SourceVecTraits::num_lanes()]);
    KernelWindow_a(20) = load_array_element(src_20[0]);
    KernelWindow_b(20) =
        load_array_element(src_20[SourceVecTraits::num_lanes()]);
  }
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_FILTER_SEPARABLE_FILTER_LOADER_21X21_H
